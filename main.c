#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "channel.h"

void print_usage(void);
void print_delivery(SeqMessage *);

int main(int argc, char **argv)
{
        int count = -1;
        int n_sent = 0;;
        char *port = NULL;
        char *hostfile = NULL;
        int id = -1;
        double timeout = 0.0;
        int opt;
        char options[] = { "c:h:p:i:t:" };

        int data;

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
                                timeout = atof(optarg);
                                fprintf(stderr, "timeout is %f\n", timeout);
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

        while (1) {
                if (n_sent < count) {
                        ch_send(rand() % 1000);
                        n_sent++;
                }
                if (ch_recv(&data)) {
                        // handle error, ignore
                }
                sleep(1);
                fflush(stdout);
        }

        return 0;
err:
        return 1;
}

void
print_usage()
{
        // TODO implement
}
