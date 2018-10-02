#ifndef _QUEUE_H
#define _QUEUE_H

typedef struct {
        void** arr;
        size_t size;
        size_t n;
} queue;

queue* q_alloc(size_t);
void q_free(queue*);

int q_push(queue*, void*);
void* q_pop(queue*);
void* q_peek(queue*);

#endif /*_QUEUE_H */
