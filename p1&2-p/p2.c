#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <math.h>
#include <mpi.h>
#define ROOT 0

/*-----------------------------------------------------------------*/
/* 
 Function:    MPI_BinomialBcast
   Purpose:     Broadcasts a message from the process with rank root to all
      other processes of the group

   Input args:  
             buffer    Starting address of buffer (choice).

             count     Number of entries in buffer (integer).

             datatype  Data type of buffer (handle).

             root      Rank of broadcast root (integer).

             comm      Communicator (handle).

 Implementation:
    1. We used tree structured comunication
    2. We applied the exponential formulas 
    that we were given by using a Binary Left Shift operator. 
    Since all the exponential operations are in base two, 
    we used an equivalent operation in binary which is shifting 
    the bits the same number of positions specified in the exponent. 
    In order to get rid of the complex "pow" operation.
    
 */

void MPI_BinomialBcast(void *buffer, int count, MPI_Datatype datatype,
            int root, MPI_Comm comm) {
   int size,rank, maxRank;
   unsigned        partner;
   unsigned   cSerie = 1; //Current range between the sender and the receiver
   int        participate = cSerie << 1; //Serie of numbers that participate

   MPI_Comm_size(comm,&size);
   MPI_Comm_rank(comm,&rank);
   maxRank = size - 1;

  //Check if the range of ranks between sender an receiver is within our maximum rank
   while(cSerie < size){
      //Check if the processes is going to be involved in recv/send
      if(rank < participate){
        partner = rank ^ cSerie; //Obtain the partner
        //Check if the process is going to send or recv
        if(rank < cSerie){
          //Check if the partner does not exceed the number of maxRank
          if (partner <= maxRank){
            MPI_Send(buffer, count, MPI_INT, partner, 0, MPI_COMM_WORLD);
          }
        }else {
          MPI_Recv(buffer, count, MPI_INT, partner, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        }
      }
   cSerie <<=1;
   participate <<=1;
 }
}  

void  MPI_FlattreeColective(const void *sendbuf, void *recvbuf, int count, MPI_Datatype datatype,
                            int root, MPI_Comm comm){

    int rank, numprocs;
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    
    if (rank != ROOT)
        MPI_Send(sendbuf, count, datatype, ROOT, 0, comm);
    else {
        int i, totalcount;
        totalcount = *(int*) sendbuf;
        for (i = 1; i < numprocs; i++){
            MPI_Recv(recvbuf, count, datatype, i, 0, comm, MPI_STATUS_IGNORE);
            totalcount += *(int*) recvbuf;
        }
        *(int*) recvbuf = totalcount;
    }
}

int main(int argc, char *argv[])
{
    int numprocs, rank, n, count, i, prime, j, modulo, k,maxMod;
    int totalcount;
    int done = 0;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    while (!done){
       
        // I/O only by root process (rank = 0)
        if (rank == ROOT) {
            printf("Enter the maximum number to check for primes: (0 quits) \n");
            scanf("%d", &n);
            }

        //Share n with all the processes
         MPI_BinomialBcast(&n, /* in/out parameter */
              1,     /* count */
              MPI_INT, /* datatype */
              ROOT,       /* root */
              MPI_COMM_WORLD); /* communicator */

        // if n = 0, we finish the program
        if (n == 0) break; 
        
        count = 0;
        modulo = rank;
        //Maximun modulo 
        maxMod = (numprocs - 1) - rank;
        k = 0;

        /*
            The following for loop distributes the numbers using the following criteria:
                In the ith (even number) iteration you start assign the numbers by using an ascending criteria depending on 
                rank of the process, and in the ith+1 (odd number) iteration you assing the numbers by using a descending criteria 
            Example: Number 18 with 4 processes
                      +----+----+----+----+
                      | P1 | P2 | P3 | P4 |
                      +----+----+----+----+
                      |  2 |  3 |  4 |  5 |                     
                      |  9 |  8 |  7 |  6 |
                      | 10 | 11 | 12 | 13 |
                      | 17 | 16 | 15 | 14 |
                      +----+----+----+----+
        */
        for (i = 2 + modulo; i < n; i = 2 + modulo + k * numprocs){
            prime = 1;
            k++;
            // Check if any number lower than i is multiple
            for (j = 2; j < i; j++){ //We optimize the algorithm by 
                if((i%j) == 0) {     //distributing the works between the processes
                    prime = 0;
                    break;
                }
            }
            count += prime;
            //Check if the iteration number is odd or even
            modulo = ((k & 1) == 0) ? rank : maxMod;
            
        }
        
        //Compute the final value of the count by adding all the processes counts  
            MPI_FlattreeColective(&count, /* operand */
               &totalcount,        /* result */
               1,             /* count */
               MPI_INT,    /* datatype */
                          /* operator, we assume sum */
               ROOT,             /* root rank */
               MPI_COMM_WORLD); /* communicator */

        // so the root receives all the 'count' and add them up to 'totalcount'
        if (rank == ROOT) {
        	printf("The number of primes lower than %d is %d\n", n, totalcount);
        }
        
    }

    MPI_Finalize();
}