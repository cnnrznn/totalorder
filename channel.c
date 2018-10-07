#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <time.h>
#include <unistd.h>

#include "channel.h"
#include "queue.h"

#define HOSTS_MAX 1024
#define QSIZE 1024
#define GARBAGE 1337

static int sk = -1;
static struct addrinfo hints, *skaddr;
static char *hosts[HOSTS_MAX];
static struct addrinfo *tmpaddr;
static struct sockaddr hostaddrs[HOSTS_MAX];
static size_t hostaddrslen[HOSTS_MAX];
static int nhosts = 0;
static int id = -1;
static size_t timeout;
static uint32_t msg_curr = 0;
static uint32_t seq_curr = 0;
static uint32_t ckpt_curr = 0;
static size_t *ckpt_vec;

static queue* sendq = NULL;
static queue* recvq = NULL;
static queue* holdq = NULL;

typedef struct {
        DataMessage dm;
        SeqMessage sm;
        CkptMessage cm;
        char is_ckpt;
        char *acks, *facks;
        size_t nacks, nfacks;
        size_t *timers;
        double *timeouts;
} sendq_elem;

typedef struct {
        char msg[MSGLEN];
        uint32_t type;
        DataMessage *dm;
        AckMessage *am;
        SeqMessage *sm;
        FinMessage *fm;
        CkptMessage *cm;
        CkptAck *ca;
} recvq_elem;

typedef struct {
        DataMessage dm;
        AckMessage am;
        FinMessage fm;
        uint32_t final_seq;
        uint32_t final_seq_proposer;
        char deliverable;
} holdq_elem;

static char
comp_holdq_elem(void *a, void *b)
{
        holdq_elem *x = a;
        holdq_elem *y = b;

        if (x->final_seq < y->final_seq)
                return 1;
        else if (x->final_seq > y->final_seq)
                return -1;
	else if (x->final_seq_proposer < y->final_seq_proposer)
		return 1;
	else if (x->final_seq_proposer > y->final_seq_proposer)
		return -1;
        else
                return 0;
}

static char
comp_holdq_elem_msg(void *a, void *b)
{
        holdq_elem *x = a;
        holdq_elem *y = b;

        if (x->dm.sender < y->dm.sender)
                return 1;
        else if (x->dm.sender > y->dm.sender)
                return -1;
        else if (x->dm.msg_id < y->dm.msg_id)
                return 1;
        else if (x->dm.msg_id > y->dm.msg_id)
                return -1;
        else
                return 0;
}

static int
deliver(int *res)
{
        holdq_elem *he;

        q_sort(holdq, comp_holdq_elem);

        if (NULL == (he = q_peek(holdq)))
                return -1;

        if (he->deliverable) {
                *res = he->dm.data;
                fprintf(stdout, "Processed message %d from sender %d with seq (%d, %d)\n",
                                he->dm.msg_id, he->dm.sender, he->final_seq,
                                he->final_seq_proposer);
                q_pop(holdq);
                free(he);
                return 0;
        }

        return -1;
}

static void
do_ckpt(CkptMessage cm)
{
        if (ckpt_vec[cm.initiator] >= cm.ckpt_id)
                return; // already have processed this checkpoint

        ckpt_vec[cm.initiator] = cm.ckpt_id;

        fprintf(stdout, "Checkpoint %u from initiator %u\n", cm.ckpt_id, cm.initiator);
}

static void
process_sendq()
{
        //fprintf(stderr, "Entering process_sendq\n");

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
                                if (se->timeouts[i] < se->timers[i]) {
                                        if (se->is_ckpt) {
                                                sendto(sk, &se->cm, sizeof(CkptMessage), 0,
                                                                &hostaddrs[i], hostaddrslen[i]);
                                        }
                                        else {
                                                sendto(sk, &se->dm, sizeof(DataMessage), 0,
                                                                &hostaddrs[i], hostaddrslen[i]);
                                        }
                                        se->timers[i] = 0;
                                } else {
                                        se->timers[i]++;
                                }
                        }
                }
        }
        else if (se->nfacks < nhosts) {
                // unicast final_seq to those who haven't fack'd
                for (i=0; i<nhosts; i++) {
                        if (0 == se->facks[i]) {
                                if (se->timeouts[i] < se->timers[i]) {
                                        sendto(sk, &se->sm, sizeof(SeqMessage), 0,
                                                        &hostaddrs[i], hostaddrslen[i]);
                                        se->timers[i] = 0;
                                } else {
                                        se->timers[i]++;
                                }
                        }
                }
        }
        else {
                // 1.c. if received all final_acks, remove from queue
                q_pop(sendq);

                free(se->acks);
                free(se->facks);
                free(se->timers);
                free(se->timeouts);
                free(se);

                goto again;
        }
}

static void
process_recvq()
{
        //fprintf(stderr, "Entering process_recvq\n");

        recvq_elem *re = NULL;
        holdq_elem *he = NULL, other;
        sendq_elem *se = NULL;

        if (NULL == (re = q_pop(recvq))) {
                //fprintf(stderr, "No items in recvq\n");
                return;
        }

        switch (re->type) {
        case 1:                 // DataMessage
                fprintf(stderr, "Received DataMessage (%d:%d)\n", re->dm->sender, re->dm->msg_id);

                // set 'other'
                other.dm.sender = re->dm->sender;
                other.dm.msg_id = re->dm->msg_id;

                q_sort(holdq, comp_holdq_elem_msg);
                if (!(he = q_search(holdq, &other, comp_holdq_elem_msg))) {
                        // if not in holdq, put there
                        he = malloc(sizeof(holdq_elem));
                        he->dm = *(re->dm);
                        he->am.type = 2;
                        he->am.sender = he->dm.sender;
                        he->am.msg_id = he->dm.msg_id;
                        he->am.proposed_seq = ++seq_curr;
                        he->am.proposer = id;
                        he->fm.type = 4;
                        he->fm.sender = he->dm.sender;
                        he->fm.msg_id = he->dm.msg_id;
                        he->fm.proposer = id;
                        he->deliverable = 0;
                        he->final_seq = 0;

                        q_push(holdq, he);

                        //fprintf(stdout, "Assigning sequence number %u\n", he->am.proposed_seq);
                }

                // ack the DataMessage
                sendto(sk, &he->am, sizeof(AckMessage), 0,
                                &hostaddrs[he->am.sender], hostaddrslen[he->am.sender]);
                break;
        case 3:                 // SeqMessage
        	fprintf(stderr, "Received SeqMessage (%d:%d)(%u)\n", re->sm->sender, re->sm->msg_id,
                                re->sm->final_seq);

                other.dm.sender = re->sm->sender;
                other.dm.msg_id = re->sm->msg_id;

                q_sort(holdq, comp_holdq_elem_msg);
                if (!(he = q_search(holdq, &other, comp_holdq_elem_msg)))
                        break;

                he->final_seq = re->sm->final_seq;
                he->final_seq_proposer = re->sm->final_seq_proposer;
                he->deliverable = 1;

                // fin the SeqMessage
                sendto(sk, &he->fm, sizeof(FinMessage), 0,
                                &hostaddrs[he->fm.sender], hostaddrslen[he->fm.sender]);
                break;
        case 2:                 // AckMessage
                fprintf(stderr, "Received AckMessage (%d:%d)\n", re->am->sender, re->am->msg_id);

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
                fprintf(stderr, "Received FinMessage (%d:%d)\n", re->fm->sender, re->fm->msg_id);

                if (!(se = q_peek(sendq)))
                        break; // no messages in sendq
                if (se->dm.sender != re->fm->sender ||
                                se->dm.msg_id != re->fm->msg_id)
                        break; // already pop'd message if its not the first

                if (0 == se->facks[re->fm->proposer]) {
                        se->nfacks++;
                        se->facks[re->fm->proposer] = 1;
                }
                break;
        case 5:
                fprintf(stderr, "Received CkptMessage (%u:%u)\n", re->cm->initiator, re->cm->ckpt_id);

                // dump ckpt
                do_ckpt(*re->cm);

                // ack checkpoint
                CkptAck ca;
                ca.type = 6;
                ca.initiator = re->cm->initiator;
                ca.ckpt_id = re->cm->ckpt_id;
                ca.recipient = id;

                sendto(sk, &ca, sizeof(CkptAck), 0,
                                &hostaddrs[re->cm->initiator],
                                hostaddrslen[re->cm->initiator]);
                break;
        case 6:
                fprintf(stderr, "Received CkptAck (%u:%u)\n", re->ca->initiator, re->ca->ckpt_id);

                if (!(se = q_peek(sendq)))
                        break; // no messages in sendq
                if (se->cm.initiator != re->ca->initiator ||
                                se->cm.ckpt_id != re->ca->ckpt_id)
                        break; // already pop'd the pertaining ckpt marker

                if (0 == se->acks[re->ca->recipient]) {
                        se->nacks++;
                        se->acks[re->ca->recipient] = 1;
                }
                break;
        default:
                fprintf(stderr, "Received unexpected message type\n");
                break;
        }

        free(re);
}

int
ch_init(char *hostfile, char *port, int _id, size_t _timeout)
{
        FILE *f;
        size_t linelen = 32;
        ssize_t strlen;
        char *line;
        int i;

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;

        id = _id;
        timeout = _timeout;

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
                hostaddrs[nhosts] = *(tmpaddr->ai_addr);
                hostaddrslen[nhosts] = tmpaddr->ai_addrlen;
                freeaddrinfo(tmpaddr);

                fprintf(stderr, "Read '%s(%lu:%lu)' from hostfile\n", hosts[nhosts], linelen, strlen);
                nhosts++;
        }

        free(line);
        fclose(f);

        // allocate socket
        fprintf(stderr, "I am %s\n", hosts[id]);
        if (getaddrinfo(hosts[id], port, &hints, &skaddr)) {
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

        ckpt_vec = calloc(nhosts, sizeof(size_t));

        //fprintf(stderr, "ch_init: success\n");
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
ch_ckpt(void)
{
        int i;
        sendq_elem *se = malloc(sizeof(sendq_elem));

        se->is_ckpt = 1;

        se->cm.type = 5;
        se->cm.initiator = id;
        se->cm.ckpt_id = ++ckpt_curr;

        se->acks = calloc(nhosts, sizeof(char));
        se->nacks = 0;
        se->facks = calloc(nhosts, sizeof(char));
        se->nfacks = nhosts;

        se->timers = malloc(nhosts * sizeof(size_t));
        se->timeouts = malloc(nhosts * sizeof(size_t));
        for (i=0; i<nhosts; i++) {
                se->timers[i] = 0;
                se->timeouts[i] = timeout;
        }

        q_push(sendq, se);
}

int
ch_send(int data)
{
        int i;
        sendq_elem *se = malloc(sizeof(sendq_elem));

        se->is_ckpt = 0;

        se->dm.type = 1;
        se->dm.sender = id;
        se->dm.msg_id = msg_curr;
        se->dm.data = data;

        se->sm.type = 3;
        se->sm.sender = id;
        se->sm.msg_id = msg_curr;
        se->sm.final_seq = 0;
        se->sm.final_seq_proposer = id;

        se->acks = calloc(nhosts, sizeof(char));
        se->nacks = 0;
        se->facks = calloc(nhosts, sizeof(char));
        se->nfacks = 0;

        se->timers = malloc(nhosts * sizeof(size_t));
        se->timeouts = malloc(nhosts * sizeof(size_t));
        for (i=0; i<nhosts; i++) {
                se->timers[i] = 0;
                se->timeouts[i] = timeout;
        }

        fprintf(stderr, "Multicasting %d\n", data);
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
                re->fm = (4 == type) ? (FinMessage*)re->msg : NULL;
                re->cm = (5 == type) ? (CkptMessage*)re->msg : NULL;
                re->ca = (6 == type) ? (CkptAck*)re->msg : NULL;

                //fprintf(stderr, "Pushing to recvq\n");
                q_push(recvq, re);
        }

        // process the asynchronous queues
        process_sendq();
        process_recvq();

        return deliver(res);
}
