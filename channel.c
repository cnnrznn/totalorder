#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "channel.h"

static int sk = -1;
static struct addrinfo hints, *skaddr;
static char *hosts[1024];
static int nhosts = 0;

static int
broadcast()
{
}

int
ch_init(char *hostfile, char *port)
{
        FILE *f;
        size_t linelen = 32;
        size_t strlen;
        char *line;

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
                fprintf(stderr, "Read '%s(%lu:%lu)' from hostfile\n", hosts[nhosts], linelen, strlen);
                nhosts++;
        }

        free(line);
        fclose(f);

        // allocate socket
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;

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
        // TODO
        // 1. push message into "send queue"

        return -1;
}

int
ch_recv(SeqMessage *sm)
{
        char msg[MSGLEN + 1] = { 0 };
        struct sockaddr_storage from;
        int fromlen = sizeof(struct sockaddr_storage);
        int flags = 0;
        int n;

        // 1. if there is a message in the send queue
        // 1.a. if haven't received all ack's, broadcast
        // 1.b. if haven't received all final_acks's, send calculate and send final_seq
        // 1.c. if received all final_acks, remove from queue
        // 2. try to recv a message
        // 2.a. if the message is a data message, add it to the undeliverable queue (if it isn't there already) and ack it
        // 2.b. if the message is an ack, turn on "received" flag for the sender for that message
        // 2.c. if the message is a final seq, deliver the message (if we havent already) and send a final_ack

        return 0;
}
