#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>
#include <time.h>
#define M 10
#define N 10

int log2_custom (int x)
{
    int count = 0;
    while(x>1)
    {
        x >>= 1;
        count++;
    }
    return count;
}


int two_to_power_custom(int x)
{
    int res = 1;
    for(int i = 0; i < x; i++)
        res *= 2;
    return res;
}


int main(int argc, char **argv)
{
    const int root = 0;
    int rank, world_size,token, tag =0;
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size( MPI_COMM_WORLD, &world_size);
    token = rank;

    // Simple star implementation
    /*
    if (rank == root)
    {
        for (int i = 0; i < world_size; i++)
        {
            if(i != root)
                MPI_Send(&token,1, MPI_INT,i,tag,MPI_COMM_WORLD);
        }
    }
    else
    {
        MPI_Recv(&token, 1, MPI_INT, root, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
        printf("Process %d, I recieved from root a token of value=%d\n",rank,token);
    }
    */

    // Binomial tree
    int steps = log2_custom(world_size);
    for (int i = 0; i<steps; i++)
    {
        int to_send_to = (rank + two_to_power_custom(i))%world_size;
        printf("%d\n",i);
        if (to_send_to < rank)
        {
            printf("hit to_send_to < rank\n");
            if ((rank == root && i>=0)||(rank != root && i>=(log2_custom(rank) + 1 )))
            {
                printf("hit this\n");
                MPI_Send(&token,1, MPI_INT,to_send_to,tag,MPI_COMM_WORLD);
            }
        }
    }
    MPI_Recv(&token, 1, MPI_INT, root, tag, MPI_COMM_WORLD, MPI_STATUS_IGNORE);
    printf("I recieved from %d",token);

    MPI_Finalize();
    return EXIT_SUCCESS;
}
