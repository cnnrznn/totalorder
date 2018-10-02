#include <stdio.h>
#include <stdlib.h>

#include "queue.h"

int
q_push(queue* q, void* e)
{
        // TODO logic to grow queue
        if (q->size == q->n + 1)
                abort(); // play it safe

        q->arr[q->n++] = e;

        fprintf(stderr, "Queue contains %lu\n", q->n);
}

void*
q_pop(queue* q)
{
        void* ret;
        int i;

        if (q->size <= 0)
                return NULL;

        ret = q->arr[0];

        for (i=0; i<q->n-1; i++)
                q->arr[i] = q->arr[i+1];

        q->n--;

        return ret;
}

void*
q_peek(queue* q)
{
        if (q->size <= 0)
                return NULL;

        return q->arr[0];
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
