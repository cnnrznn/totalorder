#ifndef _CHANNEL_H
#define _CHANNEL_H

#define MSGLEN 1024

int ch_init(char *hostfile, char *port);
int ch_fini(void);
int ch_recv(void);
int broadcast(char *msg, int len);

#endif /* _CHANNEL_H */
