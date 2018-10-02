#ifndef _CHANNEL_H
#define _CHANNEL_H

#include "messages.h"

#define MSGLEN 1024

int ch_init(char *hostfile, char *port, int id);
int ch_fini(void);
int ch_send(int);
int ch_recv(int*);

#endif /* _CHANNEL_H */
