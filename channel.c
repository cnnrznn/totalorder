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

static int sk = -1;
static struct addrinfo hints, *skaddr;
static char *hosts[HOSTS_MAX];
static struct addrinfo hostaddrs[HOSTS_MAX], *tmpaddr;
static int nhosts = 0;
static int id = -1;

static queue* sendq = NULL;
static int sendq_last = 0;
static queue* recvq = NULL;

#define QSIZE 1024

typedef struct {
        DataMessage dm;
        char *acks, *facks;
        char acked, facked;
} sendq_elem;

static int
broadcast()
{
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
ch_send(DataMessage dm)
{
        sendq_elem *e = malloc(sizeof(sendq_elem));
        e->dm = dm;
        e->acks = calloc(nhosts, sizeof(char));
        e->facks = calloc(nhosts, sizeof(char));
        e->acked = 0;
        e->facked = 0;

        q_push(sendq, e);
}

int
ch_recv(SeqMessage *sm)
{
        char msg[MSGLEN + 1] = { 0 };
        struct sockaddr_storage from;
        int fromlen = sizeof(struct sockaddr_storage);
        int flags = 0;
        int n;
        sendq_elem *se;
        int i;

        // 1. if there is a message in the send queue
        if ((se = q_peek(sendq))) {
                if (0 == se->acked) {
                        for (i=0; i<nhosts; i++) {
                                if (0 == se->acks[i]) {
                                        sendto(sk, &se->dm, sizeof(DataMessage), 0,
                                                        NULL, 0);
                                }
                        }
                }
                else if (0 == se->facked) {
                        // 1.b. if haven't received all final_acks's, calculate and send final_seq
                } else {
                        // 1.c. if received all final_acks, remove from queue
                        q_pop(sendq);

                        free(se->acks);
                        free(se->facks);
                        free(se);
                }
        }

        // 2. try to recv a message
        // 2.a. if the message is a data message, add it to the undeliverable queue (if it isn't there already) and ack it
        // 2.b. if the message is an ack, turn on "received" flag for the sender for that message
        // 2.c. if the message is a final seq, deliver the message (if we havent already) and send a final_ack

        return 0;
}
