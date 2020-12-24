#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <mpi.h>

#define LIMIT 10
#define ROOT 0		

void MPI_Scan2(void * sendbuf, void * recvbuf, int count,MPI_Datatype datatype, MPI_Op op,
                      MPI_Comm comm){
	int rank,p,i, tmp;
	MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm, &rank);
    int valueOfInt = sizeof(int);

    //The process zero only has to send its buffer (ring communication)
    if(rank == 0){
    	MPI_Send(sendbuf, count, datatype, rank+1, 0, comm);
    	for (i = 0; i < count; ++i) {
    		tmp = *(int *)sendbuf;
    		*(int *)sendbuf = tmp;
    		recvbuf += valueOfInt;
    	}
    }else{
    	//The rest have to receive the buffer from the preceding one
    	int * intArray = malloc(sizeof(int)*3);
    	MPI_Recv(intArray, count, datatype, rank-1, 0, comm, NULL);
    	for (i = 0; i < count; ++i){
    		*(int *)recvbuf += intArray[i]; //Cast the receiving buffer to int
    		recvbuf += valueOfInt;			//Advance one position in the array
    	}
    	//The last process does not have to send its computed buffer
    	if(rank != (p-1)) MPI_Send(recvbuf, count, datatype, rank+1, 0, comm);
    free(intArray);
	}
}
int main(int argc, char * argv[]){
	MPI_Comm comm;
	comm = MPI_COMM_WORLD;
	int n;
	srand(time(NULL));

	MPI_Init(NULL,NULL);

	int rank,p,i;
	MPI_Comm_size(comm, &p);
    MPI_Comm_rank(comm, &rank);

    if(rank == ROOT){
		printf("Introduce the size of the array : \n");
		scanf("%d",&n);
	}

	//Share the size of the array with all the processes
	MPI_Bcast(&n, 1, MPI_INT, ROOT, comm);

	//Create the sendbuff and recvbuff
	int * integerArray = malloc(sizeof(int)*n);
	int * recvArray = malloc(sizeof(int)*n);

	//Print the array before computing the result
	printf("BEFORE : Process number %d\n",rank);
	printf("[");
	for (i = 0; i < n; ++i)
	{
		integerArray[i] = (i + rand() % (rank+1) + rank * 7) ; 
		printf(" %2d ", integerArray[i]);
	} printf(" ] \n");

	//This is our implementation of the MPI_Scan
	MPI_Scan2(integerArray, recvArray, n, MPI_INT, MPI_SUM, comm);
	//MPI_Scan(integerArray, recvArray, n, MPI_INT, MPI_SUM, comm);

	//Check results afterwards
	printf("AFTER : Process number %d\n",rank);
	printf("[");
	for (i = 0; i < n; ++i)
	{
		printf(" %2d ", recvArray[i]);
	} printf(" ] \n");

	free(integerArray);
	free(recvArray);
	MPI_Finalize();
}    

