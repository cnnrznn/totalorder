#include <stdlib.h>

#include "queue.h"

int
q_push(queue* q, void* e)
{
        // TODO logic to grow queue
        if (q->size == q->n + 1)
                abort(); // play it safe

        q->arr[q->n++] = e;
}

queue*
q_alloc(size_t size)
{
        queue* q = malloc(sizeof(queue));

        q->arr = malloc(size * sizeof(void*));
        q->size = size;
        q->n = 0;

        return q;
}

void
q_free(queue* q)
{
        free(q->arr);
        free(q);
}
