#ifndef _CHANNEL_H
#define _CHANNEL_H

int ch_init(char *hostfile, int port);
int broadcast(char *msg, int len);

#endif /* _CHANNEL_H */
