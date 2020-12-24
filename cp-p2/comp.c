#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdlib.h>
#include <fcntl.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <pthread.h>
#include "compress.h"
#include "chunk_archive.h"
#include "queue.h"
#include "options.h"

#define CHUNK_SIZE (1024*1024)
#define QUEUE_SIZE 20

#define COMPRESS 1
#define DECOMPRESS 0

struct thread_info {
    pthread_t       thread_id;        // id returned by pthread_create()
    int             thread_num;       // application defined thread #
};

struct buffer{
    queue   in;
    queue  out;
    archive ar;   
};

struct args {
    int          thread_num;       // application defined thread #
    int                  fd;
    int                size;
    int         numOfChunks;
    int           * counter;
    pthread_mutex_t   *lock;
    struct  buffer * buffer;    // Shared buffer 
    chunk (*process)(chunk);
};

//Error while creating threads
void errorThread(int i){
    printf("Could not create thread #%d", i);
    exit(1);
}

// take chunks from queue in, run them through process (compress or decompress), send them to queue out
void *worker(void * ptr) {
	 struct args *args = (struct args*) ptr;
    chunk ch, res;
    while(1) {
    	pthread_mutex_lock(args->lock);
    	if (*args->counter > 0){
    		*args->counter -= 1;
    		pthread_mutex_unlock(args->lock);
    	}
    	else {
    		pthread_mutex_unlock(args->lock);
    		break;
    	}
        ch = q_remove(args->buffer->in);
        printf("Compressing chunk with %d offset and %d size\n",ch->offset,
        ch->size);
        res = args->process(ch);
        free_chunk(ch);
        q_insert(args->buffer->out, res);
    }
}


void *readFileC(void *ptr){
    struct args *args = (struct args *) ptr;
    int offset;
    int i;
    chunk ch;

    // read input file and send chunks to the in queue
    for(i=0; i< args->numOfChunks; i++) {
        ch         = alloc_chunk(args->size);
        offset     = lseek(args->fd, 0, SEEK_CUR);
        ch->size   = read(args->fd, ch->data, args->size);
        ch->num    = i;
        ch->offset = offset;
        printf("Reading chunk %d with %d offset with %d size\n",i,ch->offset,
        ch->size);
        q_insert(args->buffer->in, ch);
    }
}

void *readFileD(void * ptr){
    struct args * args = (struct args *) ptr;
    int i; 
    chunk ch;

     // read chunks with compressed data
    for(i=0; i<args->numOfChunks; i++) {
        ch = get_chunk(args->buffer->ar, i);
        q_insert(args->buffer->in, ch);
        printf("Reading chunk %d with %d offset with %d size\n",i,ch->offset,
        ch->size);
    } 
}

void *sendChunks(void *ptr){
    struct args * args = (struct args*) ptr;
    chunk ch;
    int i;

    // send chunks to the output archive file
    for(i=0; i<args->numOfChunks; i++) {
        ch = q_remove(args->buffer->out);
        printf("Adding chunk %d number %d offset %d size\n",ch->num,ch->offset,
        ch->size);
        add_chunk(args->buffer->ar, ch);
        free_chunk(ch);
    }
}

void *writeChunks(void *ptr){
    chunk ch;
    struct args * args = (struct args *) ptr;
    int i;

    for(i=0; i<args->numOfChunks; i++) {
        ch=q_remove(args->buffer->out);
        lseek(args->fd, ch->offset, SEEK_SET);  
        write(args->fd, ch->data, ch->size);
        printf("Writting chunk %d number %d offset %d size\n",i,ch->offset,
        ch->size);
        free_chunk(ch);
    }
}


// Compress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the archive file
void comp(struct options opt) {
	pthread_t readThreadID, writeThreadID;
    int fd, i,numOfChunks;
    int * counter;
    struct args * args;
    char comp_file[256];
    struct buffer buffer;
    struct thread_info * threads;
    pthread_mutex_t * lock = malloc(sizeof(pthread_mutex_t));

    //Open and create the compressed file (.ch)
    struct stat st;

    if((fd=open(opt.file, O_RDONLY))==-1) {
        printf("Cannot open %s\n", opt.file);
        exit(0);
    }

    fstat(fd, &st);

     if(opt.out_file) {
        strncpy(comp_file,opt.out_file,255);
    } else {
        strncpy(comp_file, opt.file, 255);
        strncat(comp_file, ".ch", 255);
    }

    numOfChunks = (st.st_size/opt.size+(st.st_size % opt.size ? 1:0));

    //Alloc the args and the threads
    printf("Creating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * (opt.num_threads+2));
    args    = malloc(sizeof(struct args));
    counter = malloc(sizeof(int));
    *counter   = numOfChunks;

    if (threads == NULL || args==NULL || lock ==NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    //Common mutex 
    if (pthread_mutex_init(lock, NULL) != 0) { 
        printf("\n Mutex init has failed\n"); 
        exit(1); 
    } 

    //Initialize the buffer
    buffer.ar  = create_archive_file(comp_file);
    buffer.in  = q_create(opt.queue_size);
    buffer.out = q_create(opt.queue_size);
   

    //Args init
       args->buffer        = &buffer;
       args->fd            = fd;
       args->size          = opt.size;
       args->numOfChunks   = numOfChunks;
       args->process       = zcompress;
       args->counter       = counter;
       args->lock 	       = lock;

       if ( (0 != pthread_create(&readThreadID, NULL,
                     readFileC, args)) ) {
            errorThread(0);
        }

        if (0 != pthread_create(&writeThreadID, NULL,
                     sendChunks, args)){
            errorThread(1);
        }

    // Create num_thread threads running (workers)
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].thread_num = i;

        if (0 != pthread_create(&threads[i].thread_id, NULL,
                     worker, args)) {
            errorThread(i);
        }
        
        
    }

    //Wait for the reader and the writer
    pthread_join(writeThreadID, NULL);
    pthread_join(readThreadID, NULL);

    
    // Wait for the threads to finish
    for (i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].thread_id, NULL);

    
    close_archive_file(buffer.ar);
    close(args->fd);
    q_destroy(buffer.in);
    q_destroy(buffer.out);
    free(args);
    free(threads);
}



// Decompress file taking chunks of opt.size from the input file,
// inserting them into the in queue, running them using a worker,
// and sending the output from the out queue into the decompressed file

void decomp(struct options opt) {
    int *counter;
    int fd, i;
    char uncomp_file[256];
    struct buffer buffer;
    struct thread_info * threads;
    struct args * args;
    pthread_t readThreadID,writeThreadID;
    pthread_mutex_t * lock = malloc(sizeof(pthread_mutex_t));

     //Alloc the args and the threads
    printf("Creating %d threads\n", opt.num_threads);
    threads = malloc(sizeof(struct thread_info) * (opt.num_threads+2));
    args    = malloc(sizeof(struct args));
    counter = malloc(sizeof(int));

    if (threads == NULL || args==NULL || counter==NULL) {
        printf("Not enough memory\n");
        exit(1);
    }

    if((buffer.ar=open_archive_file(opt.file))==NULL) {
        printf("Cannot open archive file\n");
        exit(0);
    };

    if(opt.out_file) {
        strncpy(uncomp_file, opt.out_file, 255);
    } else {
        strncpy(uncomp_file, opt.file, strlen(opt.file) -3);
        uncomp_file[strlen(opt.file)-3] = '\0';
    }

    if((fd=open(uncomp_file, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH))== -1) {
        printf("Cannot create %s: %s\n", uncomp_file, strerror(errno));
        exit(0);
    }

    //Common mutex 
    if (pthread_mutex_init(lock, NULL) != 0) { 
        printf("\n Mutex init has failed\n"); 
        exit(1); 
    } 

    *counter = chunks(buffer.ar);

    //Initialize the buffer
    buffer.in  = q_create(opt.queue_size);
    buffer.out = q_create(opt.queue_size);

    //Args init
       args->buffer        = &buffer;
       args->fd            = fd;
       args->size          = opt.size;
       args->numOfChunks   = chunks(buffer.ar);
       args->process       = zdecompress;
       args->counter       = counter;
       args->lock 	       = lock;

       if (0 != pthread_create(&readThreadID, NULL,
                     readFileD, args)) {
            errorThread(0);
        }

       if (0 != pthread_create(&writeThreadID, NULL,
                     writeChunks, args)){
            errorThread(1);
        }



    // Create num_thread threads running (workers)
    for (i = 0; i < opt.num_threads; i++) {
        threads[i].thread_num = i;

       if (0 != pthread_create(&threads[i].thread_id, NULL,
                     worker, args)) {
            errorThread(i);
        }
        
        
    }

    //Wait for the reader and the writer
    pthread_join(writeThreadID, NULL);
    pthread_join(readThreadID, NULL);

     // Wait for the threads to finish
    for (i = 0; i < opt.num_threads; i++)
        pthread_join(threads[i].thread_id, NULL);

    //Close and deallocs
    close_archive_file(buffer.ar);
    close(args->fd);
    q_destroy(buffer.in);
    q_destroy(buffer.out);
    free(args);
    free(threads);
}

int main(int argc, char *argv[]) {
    struct options opt;

    opt.compress    = COMPRESS;
    opt.num_threads = 3;
    opt.size        = CHUNK_SIZE;
    opt.queue_size  = QUEUE_SIZE;
    opt.out_file    = NULL;

    read_options(argc, argv, &opt);

    if(opt.compress == COMPRESS) comp(opt);
    else decomp(opt);
}
