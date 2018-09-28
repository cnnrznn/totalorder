#ifndef _CHANNEL_H
#define _CHANNEL_H

int ch_init(char *hostfile, char *port);
int ch_fini(void);
int broadcast(char *msg, int len);

#endif /* _CHANNEL_H */
