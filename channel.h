#ifndef _CHANNEL_H
#define _CHANNEL_H

#include "messages.h"

#define MSGLEN 1024

int ch_init(char *hostfile, char *port);
int ch_fini(void);
int ch_send(DataMessage);
int ch_recv(SeqMessage *);

#endif /* _CHANNEL_H */
