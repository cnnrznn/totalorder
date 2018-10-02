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

static queue* sendq = NULL;
static queue* recvq = NULL;

static int seq = 0;

typedef struct {
        DataMessage dm;
        char *acks;
        char acked;
} sendq_elem;

typedef struct {
        char msg[MSGLEN];
        uint32_t type;
        DataMessage *dm;
        AckMessage *am;
        SeqMessage *sm;
} recvq_elem;

static int
broadcast()
{
}

static void
process_sendq()
{
        int i;
        sendq_elem *se = NULL;

        // 1. if there is a message in the send queue
        if ((se = q_peek(sendq))) {
                if (0 == se->acked) {
                        for (i=0; i<nhosts; i++) {
                                if (0 == se->acks[i]) {
                                        sendto(sk, &se->dm, sizeof(DataMessage), 0,
                                                        hostaddrs[i].ai_addr, hostaddrs[i].ai_addrlen);
                                }
                        }
                }
                else {
                        // 1.b. broadcast decision to group
                        // TODO

                        // 1.c. if received all final_acks, remove from queue
                        q_pop(sendq);

                        free(se->acks);
                        free(se);
                }
        }
}

static void
process_recvq()
{
        // 2.a. if the message is a data message, add it to the undeliverable queue (if it isn't there already) and ack it
        // 2.b. if the message is an ack, turn on "received" flag for the sender for that message
        // 2.c. if the message is a final seq, deliver the message (if we havent already) and send a final_ack
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
        sendq_elem *e = malloc(sizeof(sendq_elem));
        e->dm.type = 1;
        e->dm.sender = id;
        e->dm.msg_id = GARBAGE;
        e->dm.data = data;
        e->acks = calloc(nhosts, sizeof(char));
        e->acked = 0;

        q_push(sendq, e);
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

        process_sendq();

        // 2. try to recv a message

        if ((ret = recvfrom(sk, msg, MSGLEN, flags, (struct sockaddr *)&from, &fromlen)) <= 0) {
                return -1;
        }

        addr = (struct sockaddr_in *)&from;
        type = *((uint32_t*)msg);

        recvq_elem *re = malloc(sizeof(recvq_elem));
        memcpy(re->msg, msg, MSGLEN);
        re->type = type;
        re->dm = (1 == type) ? (DataMessage*)re->msg : NULL;
        re->am = (2 == type) ? (AckMessage*)re->msg : NULL;
        re->sm = (3 == type) ? (SeqMessage*)re->msg : NULL;

        q_push(recvq, re);

        process_recvq();

        return -1;
}
