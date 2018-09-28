#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "channel.h"
#include "messages.h"

void print_usage(void);
void print_delivery(SeqMessage *);

int main(int argc, char **argv)
{
        int count = -1;
        char *port = NULL;
        char *hostfile = NULL;
        int opt;
        char options[] = { "c:h:p:" };

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
                        default:
                                fprintf(stderr, "Unknown option, committing sepuku...\n");
                                goto err;
                }
        }

        if (count < 0 || NULL == port || NULL == hostfile) {
                print_usage();
                goto err;
        }

        ch_init(hostfile, port);

        return 0;
err:
        return 1;
}

void
print_delivery(SeqMessage *sm)
{
        printf("%u: Processed message %u from sender %u with seq (%u, %u)\n",
                getpid(), sm->msg_id, sm->sender, sm->final_seq, sm->final_seq_proposer);
}

void
print_usage()
{
        // TODO implement
}
