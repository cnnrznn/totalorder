#include <netdb.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "channel.h"

static int sk = -1;
static struct addrinfo hints, *skaddr;

int
ch_init(char *hostfile, char *port)
{
        // TODO read hostfile

        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_INET;
        hints.ai_socktype = SOCK_DGRAM;
        hints.ai_flags = AI_PASSIVE;

        if (getaddrinfo(NULL, port, &hints, &skaddr)) {
                perror("Unable to getaddrinfo()\n");
                return -1;
        }
        if ((sk = socket(skaddr->ai_family, skaddr->ai_socktype, skaddr->ai_protocol)) == -1) {
                perror("Unable to create socket\n");
                return -1;
        }
        if (bind(sk, skaddr->ai_addr, skaddr->ai_addrlen) == -1) {
                perror("Unable to bind socket\n");
                return -1;
        }

        return 0;
}

int
ch_fini(void)
{
        // TODO
        return -1;
}

int
ch_recv()
{
        char msg[MSGLEN + 1] = { 0 };
        struct sockaddr_storage from;
        int fromlen = sizeof(struct sockaddr_storage);
        int flags = 0;
        int n;

        if ((n = recvfrom(sk, msg, MSGLEN, flags, (struct sockaddr *)&from, &fromlen)) == -1) {
                perror("Error receiving UDP packet");
                return -1;
        }

        fprintf(stderr, "%s\n", msg);

        return n;
}

int
ch_broadcast(char *msg, int len)
{
        return -1;
}
