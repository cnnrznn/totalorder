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
                fprintf(stderr, "Read '%s(%d:%d)' from hostfile\n", line, linelen, strlen);
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
