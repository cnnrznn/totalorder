#ifndef _CHANNEL_H
#define _CHANNEL_H

#include "messages.h"

#define MSGLEN 1024

int ch_init(char *, char *, int, size_t);
int ch_fini(void);
int ch_send(int);
int ch_recv(int*);

void ch_deliver(int*);

int ch_ckpt(void);

#endif /* _CHANNEL_H */
