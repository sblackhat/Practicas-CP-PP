#include <stdio.h>
#include <stdlib.h>
#include <math.h>
#include <mpi.h>
#define ROOT 0

int main(int argc, char *argv[])
{
    int numprocs, rank, n, count, i, prime, j, modulo, k;
    int totalcount, maxMod;
    int done = 0;
    MPI_Init(&argc, &argv);
    MPI_Comm_size(MPI_COMM_WORLD, &numprocs);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);

    while (!done){
       
        // I/O only by root process (rank = 0)
        if (rank == ROOT) {
            printf("Enter the maximum number to check for primes: (0 quits) \n");
            scanf("%d", &n);
            //Share the number (n) with all the processes using MPI_Send
            for (i = 1; i < numprocs; i++)
                MPI_Send(&n, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
        } 
    
        //The other processes receive the number
        if (rank != ROOT)           
                //We set MPI_STATUS_IGNORE because we are not interested in the status
                MPI_Recv(&n, 1, MPI_INT, ROOT, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
            
        // Zero quits
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
        //The processes send their own count to the root
        if (rank != ROOT) 
            MPI_Send(&count, 1, MPI_INT, ROOT, 0, MPI_COMM_WORLD);
        
        // I/O only by root
        // so the root receives all the 'count' and add them up to 'totalcount'
        if (rank == ROOT) {
            // saves ROOT count
            totalcount = count;
            for (i = 1; i < numprocs; i++){
                MPI_Recv(&count, 1, MPI_INT, i, 0, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
                totalcount += count;
            }
            printf("The number of primes lower than %d is %d\n", n, totalcount);
        }
        
    }

    MPI_Finalize();
}
