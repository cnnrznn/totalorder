#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "channel.h"
#include "queue.h"

#define HOSTS_MAX 1024
#define QSIZE 1024
#define GARBAGE 1337

static int sk = -1;
static struct addrinfo hints, *skaddr;
static char *hosts[HOSTS_MAX];
static struct addrinfo hostaddrs[HOSTS_MAX], *tmpaddr;
static int nhosts = 0;
static int id = -1;
static uint32_t msg_curr = 0;
static uint32_t seq_curr = 0;

static queue* sendq = NULL;
static queue* recvq = NULL;
static queue* holdq = NULL;

typedef struct {
        DataMessage dm;
        SeqMessage sm;
        char *acks, *facks;
        size_t nacks, nfacks;
} sendq_elem;

typedef struct {
        char msg[MSGLEN];
        uint32_t type;
        DataMessage *dm;
        AckMessage *am;
        SeqMessage *sm;
} recvq_elem;

typedef struct {
        DataMessage dm;
        AckMessage am;
        FinMessage fm;
        uint32_t final_seq;
        char deliverable;
} holdq_elem;

static char
comp_holdq_elem(void *a, void *b)
{
        holdq_elem *x = a;
        holdq_elem *y = b;

        if (x->dm.sender == y->dm.sender && x->dm.msg_id == y->dm.msg_id)
                return 0;
        else if (x->final_seq < y->final_seq)
                return 1;
        else if (x->final_seq > y->final_seq)
                return -1;
        else
                return 0;
}

static void
deliver()
{
}

static void
process_sendq()
{
        int i;
        sendq_elem *se = NULL;

again:
        // 1. if there is a message in the send queue
        if (!(se = q_peek(sendq)))
                return;

        if (se->nacks < nhosts) {
                // unicast message to those who haven't ack'd
                for (i=0; i<nhosts; i++) {
                        if (0 == se->acks[i]) {
                                sendto(sk, &se->dm, sizeof(DataMessage), 0,
                                                hostaddrs[i].ai_addr, hostaddrs[i].ai_addrlen);
                        }
                }
        }
        else if (se->nfacks < nhosts) {
                // unicast final_seq to those who haven't fack'd
                for (i=0; i<nhosts; i++) {
                        if (0 == se->facks[i]) {
                                sendto(sk, &se->sm, sizeof(SeqMessage), 0,
                                                hostaddrs[i].ai_addr, hostaddrs[i].ai_addrlen);
                        }
                }
        }
        else {
                // 1.c. if received all final_acks, remove from queue
                q_pop(sendq);

                free(se->acks);
                free(se->facks);
                free(se);

                goto again;
        }
}

static void
process_recvq()
{
        recvq_elem *re = NULL;
        holdq_elem *he = NULL, other;
        sendq_elem *se = NULL;

        if (!(re = q_pop(recvq)))
                return;

        switch (re->type) {
        case 1:                 // DataMessage
                // set 'other'
                other.dm.sender = re->dm->sender;
                other.dm.msg_id = re->dm->msg_id;

                if (!(he = q_search(holdq, &other, comp_holdq_elem))) {
                        // if not in holdq, put there
                        he = malloc(sizeof(holdq_elem));
                        he->dm = *(re->dm);
                        he->am.type = 2;
                        he->am.sender = he->dm.sender;
                        he->am.msg_id = he->dm.msg_id;
                        he->am.proposed_seq = seq_curr++;
                        he->am.proposer = id;
                        he->deliverable = 0;
                        he->final_seq = -1;

                        q_push(holdq, he);
                }

                // ack the DataMessage
                sendto(sk, &he->am, sizeof(AckMessage), 0,
                                hostaddrs[he->am.sender].ai_addr,
                                hostaddrs[he->am.sender].ai_addrlen);

                break;
        case 3:                 // SeqMessage
                other.dm.sender = re->sm->sender;
                other.dm.msg_id = re->sm->msg_id;

                if (!(he = q_search(holdq, &other, comp_holdq_elem)))
                        break;

                he->final_seq = re->sm->final_seq;
                he->deliverable = 1;

                q_sort(holdq, comp_holdq_elem);
                deliver();
                break;
        case 2:                 // AckMessage
                if (!(se = q_peek(sendq)))
                        break; // no messages in sendq
                if (se->dm.sender != re->am->sender ||
                                se->dm.msg_id != re->am->msg_id)
                        break; // already pop'd message if its not the first

                if (0 == se->acks[re->am->proposer]) {
                        se->nacks++;
                        se->acks[re->am->proposer] = 1;
                }

                if (se->sm.final_seq < re->am->proposed_seq) {
                        se->sm.final_seq = re->am->proposed_seq;
                        se->sm.final_seq_proposer = re->am->proposer;
                }
                break;
        case 4:                 // FinMessage
                break;
        default:
                fprintf(stderr, "Received unexpected message type\n");
                break;
        }

        free(re);
}

int
ch_init(char *hostfile, char *port, int _id)
{
        FILE *f;
        size_t linelen = 32;
        ssize_t strlen;
        char *line;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;

        id = _id;

        // read hostfile
        f = fopen(hostfile, "r");
        if (NULL == f) {
                perror("Unable to open hostfile");
                goto err_open;
        }

        line = malloc(linelen);

        while ((strlen = getline(&line, &linelen, f)) > 1) {
                line[strlen-1] = '\0';
                hosts[nhosts] = malloc(strlen);
                memcpy(hosts[nhosts], line, strlen);

                if (getaddrinfo(hosts[nhosts], port, &hints, &tmpaddr)) {
                        perror("Unable to get target addr info for target addr");
                        goto err_addr;
                }
                hostaddrs[nhosts] = *tmpaddr;
                freeaddrinfo(tmpaddr);

                fprintf(stderr, "Read '%s(%lu:%lu)' from hostfile\n", hosts[nhosts], linelen, strlen);
                nhosts++;
        }

        free(line);
        fclose(f);

        // allocate socket
        if (getaddrinfo(NULL, port, &hints, &skaddr)) {
                perror("Unable to getaddrinfo()");
                goto err_addr;
        }
        if ((sk = socket(skaddr->ai_family, skaddr->ai_socktype, skaddr->ai_protocol)) == -1) {
                perror("Unable to create socket");
                goto err_addr;
        }
        if (bind(sk, skaddr->ai_addr, skaddr->ai_addrlen) == -1) {
                perror("Unable to bind socket");
                goto err_addr;
        }

        // allocate queues
        sendq = q_alloc(QSIZE);
        recvq = q_alloc(QSIZE);
        holdq = q_alloc(QSIZE);

        fprintf(stderr, "ch_init: success\n");
        return 0;
err_addr:
        freeaddrinfo(skaddr);
err_open:
        return -1;
}

int
ch_fini(void)
{
        close(sk);

        // TODO
        return -1;
}

int
ch_send(int data)
{
        sendq_elem *se = malloc(sizeof(sendq_elem));

        se->dm.type = 1;
        se->dm.sender = id;
        se->dm.msg_id = msg_curr;
        se->dm.data = data;

        se->sm.type = 3;
        se->sm.sender = id;
        se->sm.msg_id = msg_curr;
        se->sm.final_seq = -1;
        se->sm.final_seq_proposer = id;

        se->acks = calloc(nhosts, sizeof(char));
        se->nacks = 0;
        se->facks = calloc(nhosts, sizeof(char));
        se->nfacks = 0;

        q_push(sendq, se);

        msg_curr++;

        return 0;
}

int
ch_recv(int *res)
{
        char msg[MSGLEN + 1] = { 0 };
        struct sockaddr_storage from;
        int fromlen = sizeof(struct sockaddr_storage);
        int flags = MSG_DONTWAIT;
        ssize_t ret;
        struct sockaddr_in *addr;
        uint32_t type;

        // try to recv a message
        if ((ret = recvfrom(sk, msg, MSGLEN, flags, (struct sockaddr *)&from, &fromlen)) > 0) {
                addr = (struct sockaddr_in *)&from;
                type = *((uint32_t*)msg);

                recvq_elem *re = malloc(sizeof(recvq_elem));
                memcpy(re->msg, msg, MSGLEN);
                re->type = type;
                re->dm = (1 == type) ? (DataMessage*)re->msg : NULL;
                re->am = (2 == type) ? (AckMessage*)re->msg : NULL;
                re->sm = (3 == type) ? (SeqMessage*)re->msg : NULL;

                q_push(recvq, re);
        }

        // process the asynchronous queues
        process_sendq();
        process_recvq();

        return 0;
}
