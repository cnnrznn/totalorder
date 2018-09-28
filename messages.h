#ifndef _MSG_H
#define _MSG_H

#include <stdint.h>

typedef struct {
        uint32_t type;
        uint32_t sender;
        uint32_t msg_id;
        uint32_t data;
} DataMessage;

typedef struct {
        uint32_t type;
        uint32_t sender;
        uint32_t msg_id;
        uint32_t proposed_seq;
        uint32_t proposer;
} AckMessage;

typedef struct {
        uint32_t type;
        uint32_t sender;
        uint32_t msg_id;
        uint32_t final_seq;
        uint32_t final_seq_proposer;
} SeqMessage;

#endif /* _MSG_H */
