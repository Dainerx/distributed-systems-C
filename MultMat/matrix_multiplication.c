#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <time.h>
#include <omp.h>
#include <stdarg.h>
#include <errno.h>
#include "matrix_util.h"
#include "solver.h"
#include "metrics.h"
#include "cmd.h"
#include "strassen.h"

int main(int argc, char *argv[])
{
  /*
    Vérifier la syntaxe ainsi que le contenu de la ligne de commande et afficher un message d'erreur en cas d'erreur
    exemple: si un flag n'a pas été renseigné ou les valeurs sont négatives ou le nombre de colonnes de la matrice A
    est different du nombre de lignes de la matrice B, un message d'erreur personnalisé est affiché
    La suntaxe est: -a 700 -b 900 -c 900 -d 600 -n 25
  */
  struct CmdInput ci = read_input(argc, argv);
  if (check_input(ci) == false)
  {
    return 1;
  }

  printf("\n    A: (%d,%d)\n    B: (%d,%d)\n", ci.lines_a, ci.columns_a, ci.lines_b, ci.columns_b);
  int num_threads = ci.num_threads;
  printf("    Threads number: %d \n", num_threads);

  // Allocation des matrice A, B et C
  int **mat_A = malloc_mat(ci.lines_a, ci.columns_a);
  int **mat_B = malloc_mat(ci.lines_b, ci.columns_b);
  int **mat_C = malloc_mat(ci.lines_a, ci.columns_b);

  const char *labels[4] = {"seq", "seq strassen", "parallel", "parallel optimized"};
  float **metrics = malloc_matf(4, 4);
  srand(time(NULL));

  // Remlissage des matrices A et B
  fill_mat(mat_A, ci.lines_a, ci.columns_a);
  fill_mat(mat_B, ci.lines_b, ci.columns_b);

  //Initialiser la structure input par le nombre de lignes et de colonnes des matrices A et B
  init_solver(ci.lines_a, ci.columns_a, ci.lines_b, ci.columns_b);

  // Multiplication séquentielle des matrices A et B
  double cpu_time_used_seq;
  cpu_time_used_seq = sequential_mult(mat_A, mat_B, mat_C);
  // Mettre les résultats des metriques pour le calcul séquentiel dans la matrice metrics
  metrics[0][0] = cpu_time_used_seq;
  metrics[0][1] = 1;
  metrics[0][2] = 1;
  metrics[0][3] = 1;

  // Multiplication séquentielle en réduisant le nombre de multiplication.
  fill_mat(mat_C, ci.lines_a, ci.columns_b); // Réinitialissation des lignes de cache.
  //cpu_time_used_seq = strassen_mult(mat_A, mat_B, mat_C);

  int m = get_max( ci.lines_a, ci.columns_a);
  int max = get_max(m, ci.columns_b);

  // SI LES MATRICES NE SONT PAS CARRÉES
 if(ci.lines_a != ci.columns_a && ci.lines_a != ci.columns_a)
  {
    int** mat_A_squared = malloc_mat(max, max);
    int** mat_B_squared = malloc_mat(max, max);
    int** mat_C_squared = malloc_mat(max, max);

    make_square(mat_A_squared, mat_A, ci.lines_a, ci.columns_a, max);
    make_square(mat_B_squared, mat_B, ci.lines_b, ci.columns_b,max);
    cpu_time_used_seq = strassen_mult(mat_A_squared, mat_B_squared, mat_C_squared,mat_C, max);
  }else{
    cpu_time_used_seq = strassen_mult(mat_A, mat_B, mat_C, mat_C, ci.columns_a);
  }

  metrics[1][0] = cpu_time_used_seq;
  metrics[1][1] = 1;
  metrics[1][2] = 1;
  metrics[1][3] = 1;

  // Multiplication en parallèle version naive des matrices A et B (Omp)
  fill_mat(mat_C, ci.lines_a, ci.columns_b); // Réinitialissation des lignes de cache.
  double cpu_time_used_parallel = parallel_mult(num_threads, mat_A, mat_B, mat_C);

  // Mettre les résultats des metriques pour le calcul en parallèle version naive dans la matrice metrics
  metrics[2][0] = cpu_time_used_parallel;
  metrics[2][1] = speedup(cpu_time_used_seq, cpu_time_used_parallel);
  metrics[2][2] = efficiency(cpu_time_used_seq, cpu_time_used_parallel, num_threads);
  metrics[2][3] = cost(cpu_time_used_parallel, num_threads);

  // Multiplication en parallèle version optimisée des matrices A et B
  fill_mat(mat_C, ci.lines_a, ci.columns_b); // Réinitialissation des lignes de cache.
  cpu_time_used_parallel = optimized_parallel_multiply(num_threads, mat_A, mat_B, mat_C);

  // Mettre les résultats des metriques pour le calcul en parallèle version optimisée dans la matrice metrics
  metrics[3][0] = cpu_time_used_parallel;
  metrics[3][1] = speedup(cpu_time_used_seq, cpu_time_used_parallel);
  metrics[3][2] = efficiency(cpu_time_used_seq, cpu_time_used_parallel, num_threads);
  metrics[3][3] = cost(cpu_time_used_parallel, num_threads);

  // Afficher les metriques pour chaque solveur
  print_metrics(labels, metrics);

  // Désallocation des matrice pour éviter les fuites de mémoire.
  free_mat(mat_A, ci.lines_a);
  free_mat(mat_B, ci.lines_b);
  free_mat(mat_C, ci.lines_a);
  return EXIT_SUCCESS;
}
