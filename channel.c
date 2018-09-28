#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>

#include "channel.h"

static int sk = -1;
static struct sockaddr_in skaddr = { 0 };

int
ch_init(char *hostfile, int port)
{
        // TODO read hostfile

        skaddr.sin_family = AF_INET;
        skaddr.sin_port = port;
        skaddr.sin_addr.s_addr = INADDR_ANY;

        if ((sk = socket(PF_INET, SOCK_DGRAM, 0)) == -1) {
                perror("Unable to create socket\n");
                return -1;
        }
        if (bind(sk, (struct sockaddr *)&skaddr, sizeof(skaddr)) == -1) {
                perror("Unable to bind socket\n");
                return -1;
        }
}

int
ch_broadcast(char *msg, int len)
{
        return -1;
}
