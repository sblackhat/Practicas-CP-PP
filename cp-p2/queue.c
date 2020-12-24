#include <stdlib.h>
#include <semaphore.h>
#define NUM_LOOPS 10

// circular array
typedef struct _queue {
    int size;
    int used;
    int first;
    void **data;
    sem_t *sUsed;
    sem_t *sEmpty;
    sem_t *mutex;
} _queue;

#include "queue.h"

queue q_create(int size) {
    queue q = malloc(sizeof(_queue));
    q->sUsed = malloc(sizeof(sem_t));
    q->sEmpty = malloc(sizeof(sem_t));
    q->mutex = malloc(sizeof(sem_t));
    q->size  = size;
    q->used  = 0;
    q->first = 0;
    q->data  = malloc(size*sizeof(void *));
    
    sem_init(q->sUsed, 0,0);
    sem_init(q->sEmpty, 0,size);
    sem_init(q->mutex, 0, 1);
    return q;
}

int q_elements(queue q) {
    return q->used;
}

int q_insert(queue q, void *elem) {
    sem_wait(q->sEmpty);
    sem_wait(q->mutex);
    q->data[(q->first+q->used) % q->size] = elem;    
    q->used++;
    sem_post(q->mutex);
    sem_post(q->sUsed);    
    return 1;
}

void *q_remove(queue q) {
    void *res;
    sem_wait(q->sUsed);
    sem_wait(q->mutex);
    res = q->data[q->first];
    
    q->first = (q->first+1) % q->size;
    q->used--;
    sem_post(q->mutex);
    sem_post(q->sEmpty);
    return res;
}

void q_destroy(queue q) {
    free(q->data);
    free(q);
    sem_destroy(q->sEmpty);
    sem_destroy(q->sUsed);
    sem_destroy(q->mutex);
    free(q->sEmpty);
    free(q->sUsed);
    free(q->mutex);
}
