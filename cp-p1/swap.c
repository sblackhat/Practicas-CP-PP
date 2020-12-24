#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include "options.h"
#include <sys/time.h>
struct buffer {
    int *data;
    int size;
};

struct thread_info {
    pthread_t       thread_id;        // id returned by pthread_create()
    int             thread_num;       // application defined thread #
};

struct args {
    int         thread_num;       // application defined thread #
    int             delay;            // delay between operations
    int     iterations;
    struct buffer   *buffer;          // Shared buffer
    pthread_mutex_t *mutex;
};

void findDuplicates(struct buffer b){
    int i,j,num = 0;
    for (i = 0; i < b.size; ++i)
    {
        for (j = 0; j < b.size; ++j)
        {
            if(i == b.data[j]){
                num++;
                break;
            }
        }
    }
    if (num == b.size)
        printf("There is no duplicates\n");
    else printf("There is duplicates\n");
}

void *swap(void *ptr)
{
    struct args *args =  ptr;
    
    

    while(args->iterations--) {
        int i,j, tmp;
        
        i=rand() % args->buffer->size;
        j=rand() % args->buffer->size;

        printf("Thread %d swapping positions %d (== %d) and %d (== %d)\n", 
            args->thread_num, i, args->buffer->data[i], j, args->buffer->data[j]);
         
        //Critical section
        pthread_mutex_lock(args->mutex);
        tmp = args->buffer->data[i];
        if(args->delay) usleep(args->delay); // Force a context switch

        args->buffer->data[i] = args->buffer->data[j];
        if(args->delay) usleep(args->delay);
        
        args->buffer->data[j] = tmp;
        if(args->delay) usleep(args->delay);
        pthread_mutex_unlock(args->mutex); //End of critical section
        
    }
    
    return NULL;
}

void print_buffer(struct buffer buffer) {
    int i;
    
    for (i = 0; i < buffer.size; i++)
        printf("%i ", buffer.data[i]);
    printf("\n");
}

void start_threads(struct options opt)
{
    int i;
    struct thread_info *threads;
    struct args *args;
    struct buffer buffer;
    pthread_mutex_t * lock = malloc(sizeof(pthread_mutex_t));

    srand(time(NULL));

    if((buffer.data=malloc(opt.buffer_size*sizeof(int)))==NULL) {
        printf("Out of memory\n");
        exit(1);
    }
    buffer.size = opt.buffer_size;

    for(i=0; i<buffer.size; i++)
        buffer.data[i]=i;

    printf("creating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * opt.num_threads);
    args = malloc(sizeof(struct args) * opt.num_threads);

    if (threads == NULL || args==NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    printf("Buffer before: ");
    print_buffer(buffer);

    // This is just for rough comparison

    struct timeval t1,t2;
    gettimeofday(&t1,NULL);
    // Create num_thread threads running swap() 
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].thread_num = i;
        
        args[i].thread_num = i;
        args[i].buffer     = &buffer;
        args[i].delay      = opt.delay;
        args[i].iterations = opt.iterations;
        args[i].mutex      = lock;

        if ( 0 != pthread_create(&threads[i].thread_id, NULL,
                     swap, &args[i])) {
            printf("Could not create thread #%d", i);
            exit(1);
        }
    }

    // Wait for the threads to finish
    for (i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].thread_id, NULL);
    
    //Measure the time at the end of the program execution
    gettimeofday(&t2,NULL);
    printf("Time elapsed : %ld \n", (t2.tv_usec-t1.tv_usec));
    // Print the buffer
    printf("Buffer after:  ");
    print_buffer(buffer);
    findDuplicates(buffer);
    free(args);
    free(threads);
    free(buffer.data);
    pthread_mutex_destroy(args->mutex); 
    pthread_exit(NULL);
}

int main (int argc, char **argv)
{
    struct options opt;
    
    // Default values for the options
    opt.num_threads = 10;
    opt.buffer_size = 10;
    opt.iterations  = 100;
    opt.delay       = 10;
    
    read_options(argc, argv, &opt);

    start_threads(opt);

    exit (0);
}
