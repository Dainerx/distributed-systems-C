#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <mpi.h>
#include <omp.h>
#include <time.h>
#define LINES_A 100
#define COLUMNS_A 200
#define LINES_B 200
#define COLUMNS_B 100

#include "matrix_util.h"

void unflat_Lin(int *A, int lines, int columns, int **A_unflatted)
{
  int i, j;
#pragma omp parallel for private(j) schedule(guided) shared(A_unflatted)
  for( i = 0; i < lines; i++)
  {
    for( j = 0; j < columns; j++)
    {
        A_unflatted[i][j] = A[i * columns + j];
    }
  }
}

void unflat_Col(int *B, int lines, int columns, int **B_unflatted)
{
  int i, j;
#pragma omp parallel for private(j) schedule(guided) shared(B_unflatted)
  for( i = 0; i < lines; i++)
  {
    for( j = 0; j< columns; j++)
    {
        B_unflatted[i][j] = B[j * lines + i];
    }
  }
}
double sequential_mult(int **mat_A, int **mat_B, int **mat_C)
{
    double start, end, cpu_time_used;
    start = omp_get_wtime();
    int sum;
    for (int i = 0; i < LINES_A; i++)
    {
        for (int j = 0; j < COLUMNS_B; j++)
        {
            sum = 0;
            for (int k = 0; k < COLUMNS_A; k++)
            {
                sum += (mat_A[i][k] * mat_B[k][j]);
            }
            mat_C[i][j] = sum;
        }
    }
    end = omp_get_wtime();
    cpu_time_used = (end - start);
    return cpu_time_used;
}

bool mat_equal(int **A, int** B)
{
  bool correct = true;
  for (int i = 0; i < LINES_A; i++)
  {
      for (int j = 0; j < COLUMNS_B; j++)
      {
        if (A[i][j] != B[i][j])
        {
            printf("Found no equal cell: (%d,%d) a(%d,%d)=%d ; b(%d,%d)=%d\n", i, j, i, j, A[i][j], i, j, B[i][j]);
            correct = false;
        }
      }
  }
  return correct;
}
void convertMat(int **matrixA, int **matrixB, int *a, int *b)
{
  int i, j;
  #pragma omp parallel
  {
    #pragma omp parallel for schedule(guided) private(j) shared(a)
        for ( i = 0; i < LINES_A; i++)
        {
            for (j = 0; j < COLUMNS_A; j++)
            {
                a[i * COLUMNS_A + j] = matrixA[i][j];
            }
        }

    #pragma omp parallel for schedule(guided) private(j) shared(b)
        for (i = 0; i < LINES_B; i++)
        {
            for (j = 0; j < COLUMNS_B; j++)
            {
                b[j * LINES_B + i] = matrixB[i][j];
            }
        }
  }
}

 void mult(int **A, int **B, int **C, int linesA, int columnsA,int linesB, int columnsB)
 {
   int i, j, k, sum;
   #pragma omp parallel for schedule(guided) collapse(2) private(j, k, sum) shared(A, B, C)
   for ( i = 0; i < linesA; i++)
   {
       for ( j = 0; j < columnsB; j++)
       {
           sum = 0;
           for ( k = 0; k < columnsA; k++)
           {
               sum += (A[i][k] * B[k][j]);
           }
           C[i][j] = sum;
       }
   }
 }

void get_res_lines_C(int *local_res,int *received_part_A,int *flatB, int nbr_linesA_received)
{
  int i, j;
  int **partA_unflatted = malloc_mat(nbr_linesA_received, COLUMNS_A);
  int **B_unflatted = malloc_mat(LINES_B, COLUMNS_B);
  int **C = malloc_mat(nbr_linesA_received, COLUMNS_B);

  unflat_Lin(received_part_A, nbr_linesA_received, COLUMNS_A, partA_unflatted);

  unflat_Col(flatB,LINES_B ,COLUMNS_B, B_unflatted);

  mult(partA_unflatted, B_unflatted, C, nbr_linesA_received, COLUMNS_A, LINES_B, COLUMNS_B);

#pragma omp parallel for private(j)shared(local_res)
  for( i = 0; i < nbr_linesA_received; i++)
  {
      for ( j = 0; j < COLUMNS_B; j++)
      {
          local_res[i * COLUMNS_B + j] = C[i][j];
      }
  }
  free_mat(partA_unflatted, nbr_linesA_received);
  free_mat(B_unflatted,LINES_B );
  free_mat(C,nbr_linesA_received );
}

void display_linear_mat(int *mat, int lines, int columns)
{
  for(int i=0; i<lines*columns; i++)
  {
    if(i%columns ==0)
    {
      printf("\n");
    }
    printf("%d\t", mat[i]);
  }
}
int main(int argc, char **argv)
{
    const int root = 0;
    int rank, world_size;
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD, &rank);
    MPI_Comm_size( MPI_COMM_WORLD, &world_size);

    srand(time(NULL)+rank);
    int **mat_A;
    int **mat_B;
    int *flatA;
    int* res_final;
    int **mat_C;

    double start, end, time_used_mpi;
    if((LINES_A * COLUMNS_A) % world_size != 0)
    {
      printf("Le nombre de machines doit etre divisible par LINES_A * COLUMNS_A\n");
      return -1;
    }
    if((LINES_A * COLUMNS_A / world_size) % COLUMNS_A != 0)
    {
      printf(" lingesA * colunnesA /nombre de machines doit etre divisible sur colonnesA\n");
      return -2;
    }
    int *flatB = malloc((LINES_B * COLUMNS_B) * sizeof(int));
    if(rank == root)
    {
      mat_A = malloc_mat(LINES_A, COLUMNS_A);
      mat_B = malloc_mat(LINES_B, COLUMNS_B);
      mat_C = malloc_mat(LINES_A, COLUMNS_B);

      fill_mat(mat_A, LINES_A, COLUMNS_A);
      fill_mat(mat_B, LINES_B, COLUMNS_B);

      int time_seq = sequential_mult(mat_A, mat_B, mat_C);
      start= MPI_Wtime();
      flatA =  malloc((LINES_A * COLUMNS_A) * sizeof(int));
      convertMat(mat_A, mat_B,flatA,flatB);

      // Le root va allouer la taille mémoire pour contenir le résultat de la multiplication
      res_final = malloc((LINES_A*COLUMNS_B)*sizeof(int));
    }

    MPI_Barrier(MPI_COMM_WORLD);

    // Envoie de la matrice B a tout le monde; B sera partagée
    MPI_Bcast(flatB,LINES_B*COLUMNS_B,MPI_INT,root,MPI_COMM_WORLD);

    int length_revceived_A = COLUMNS_A*LINES_A / world_size;

    // Chacun alloue un tableau de taille COLUMNS_A*LINES_A / world_size pour recevoir sa partie de la matrice A
    int* received_part_A = (int *)malloc(length_revceived_A *sizeof(int));

    // Le root envoie à toutes les machines y compris lui sa part de la matrice A
    if(received_part_A)
    {
      MPI_Scatter(flatA,length_revceived_A, MPI_INT, received_part_A, length_revceived_A, MPI_INT, root, MPI_COMM_WORLD);
    }
    int nbr_linesA_received = length_revceived_A / COLUMNS_A;

    // Chacun dispose désormais de la matrice B en 1D (B est partagée) et un nombre x de lignes de la matrice A
    // Chacun doit avoir comme résultat x lignes de la matrice C
    // Ensuite tout le monde envoie sa partie calculée au root (gather)
    int* local_res = (int*) malloc(nbr_linesA_received*COLUMNS_B*sizeof(int));

    // Chacun calcule x lignes de la matrice C
    get_res_lines_C(local_res,received_part_A,flatB, nbr_linesA_received);

    // Tout le monde envoie sa partie calculée au root (gather)
    //Le root rencoi le résultat de toutes les machines
    MPI_Gather(local_res,nbr_linesA_received*COLUMNS_B , MPI_INT, res_final, nbr_linesA_received*COLUMNS_B, MPI_INT, root, MPI_COMM_WORLD );

    MPI_Barrier(MPI_COMM_WORLD);
    if(rank == root)
    {
      end= MPI_Wtime();
      time_used_mpi = end - start;
      printf("temps USED MPI %f:\n", time_used_mpi);
      int ** CC = malloc_mat( LINES_A, COLUMNS_B );
      unflat_Lin(res_final, LINES_A, COLUMNS_B, CC );

      if(!mat_equal(mat_C, CC))
      {
        printf("error mat not equal \n");
        return -1;
      }
      else{
        printf("bravo \n");
      }
    }

    free(flatB);
    if(rank == root)
       {
         free_mat(mat_A, LINES_A );
         free_mat(mat_B, LINES_B);
         free(flatA);
      }
    MPI_Finalize();
    return EXIT_SUCCESS;
}
