#ifndef _QUEUE_H
#define _QUEUE_H

typedef struct {
        void** q;
        size_t size;
        size_t n;
} queue;

queue* q_alloc(size_t);
void q_free(queue*);

int q_push(queue*, void*);

#endif /*_QUEUE_H */
