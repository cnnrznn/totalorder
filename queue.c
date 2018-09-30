#include <stdlib.h>

#include "queue.h"

int
q_push(queue* q, void* e)
{
        // TODO logic to grow queue

        q->q[q->n++] = e;
}

queue*
q_alloc(size_t size)
{
        queue* q = malloc(sizeof(queue));

        q->q = malloc(size * sizeof(void*));
        q->size = size;
        q->n = 0;

        return q;
}

void
q_free(queue* q)
{
        free(q->q);
        free(q);
}
