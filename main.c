#include <getopt.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "channel.h"

void print_usage(void);
void print_delivery(SeqMessage *);

static char cont = 1;

void
handle_sigint(int sig)
{
        cont = 0;
}

int main(int argc, char **argv)
{
        int count = -1;
        int n_sent = 0;;
        char *port = NULL;
        char *hostfile = NULL;
        int id = -1;
        int ckpt_time = -1;
        size_t timeout = 0;
        int opt;
        char options[] = { "c:h:p:i:t:s:" };

        int data;

	if (SIG_ERR == signal(SIGINT, handle_sigint)) {
                perror("Could not set signal handler");
                goto err;
        }

        while ((opt = getopt(argc, argv,  options)) != -1) {
                switch (opt) {
                        case 'c':
                                // TODO sanitize this
                                count = atoi(optarg);
                                fprintf(stderr, "Count is %d\n", count);
                                break;
                        case 'h':
                                hostfile = optarg;
                                fprintf(stderr, "Hostfile is %s\n", hostfile);
                                break;
                        case 'p':
                                port = optarg;
                                fprintf(stderr, "Port is %s\n", port);
                                break;
                        case 'i':
                                id = atoi(optarg);
                                fprintf(stderr, "ID is %d\n", id);
                                break;
                        case 't':
                                timeout = atol(optarg);
                                fprintf(stderr, "timeout is %lu\n", timeout);
                                break;
                        case 's':
                                ckpt_time = atoi(optarg);
                                fprintf(stderr, "ckpt at time %d\n", ckpt_time);
                                break;
                        default:
                                fprintf(stderr, "Unknown option, committing sepuku...\n");
                                goto err;
                }
        }

        if (count < 0 || NULL == port || NULL == hostfile ||
                        id < 0) {
                print_usage();
                goto err;
        }

        if (ch_init(hostfile, port, id, timeout))
                goto err;

        srand(time(NULL));

        while (cont) {
                if (n_sent == ckpt_time) {
                        ch_ckpt();
                }
                else if (n_sent < count) {
                        ch_send(rand() % 1000);
                }
                n_sent++;
                if (ch_recv(&data)) {
                        // handle error, ignore
                }
                sleep(0.00001);
                fflush(stdout);
        }

        ch_deliver(&data);
        //fprintf(stdout, "nsent = %lu\n", stat_nsent);
        fflush(stdout);

        return 0;
err:
        return 1;
}

void
print_usage()
{
        // TODO implement
}
