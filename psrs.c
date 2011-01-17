#include <stdio.h>
#include <stdlib.h>
#include <mpi.h>

#define SEED 42
#define RAND_MAGNITUDE_FACTOR 5

int main( int argc, char *argv[] )
{
  int rank, size;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  MPI_Comm_size (MPI_COMM_WORLD, &size);

  if ( argc != 2 )
  {
    if ( rank == 0 )
      printf( "Synopsis: psrs <size of random array>\n" );
    MPI_Finalize();
    return 1;
  }

  int numbers_size = atoi( argv[1] ); // amount of numbers to generate
  int numbers[ numbers_size ]; // will contain all generated numbers in root process

  if (numbers_size % size != 0) {
    if ( rank == 0 )
      printf("<size of random array> must be a multiple of the number of nodes (%d)\n", size);
    MPI_Finalize();
    return 1;
  }
  
  int numbers_per_processor_size = numbers_size / size;
  int numbers_per_processor[ numbers_per_processor_size ];
  
  if ( rank == 0 )
  {
    // Zufallszahlen erzeugen (mit seed)
    srand ( SEED );
    for( int pos=0; pos < numbers_size; pos++ )
    {
      int next;
      int already_in_numbers;
      do
      {
        already_in_numbers = 0;
        next = rand() % ( RAND_MAGNITUDE_FACTOR * numbers_size );
        for ( int check_pos=0; check_pos <= pos; check_pos++ )
        {
          if ( numbers[ check_pos ] == next )
          {
            already_in_numbers = 1;
            break;
          }
        }
      } while ( already_in_numbers == 1 );
      numbers[ pos ] = next;
    }
  }

  // Zufallszahlen gleichmäßig verteilen
  MPI_Scatter(&numbers, numbers_per_processor_size, MPI_INT, &numbers_per_processor, numbers_per_processor_size, MPI_INT, 0, MPI_COMM_WORLD);
  
  printf("rank: %d\t", rank);
  for (int i = 0; i < numbers_per_processor_size; i++) {
    printf("%d, ", numbers_per_processor[i]);
  }
  printf("\n\n");

  // repräsentative Selektion erstellen

  // Selektion auf einem Knoten einsammeln
  // int MPI_Gather(void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype, recvtype, int root, MPI_Comm comm)
  int selected_numbers[ size * size ];
  MPI_Gather(numbers_per_processor, size, MPI_INT, selected_numbers, size, MPI_INT, 0, MPI_COMM_WORLD);
  
  if (rank == 0) {
    for (int j = 0; j < size * size; j++) {
      printf("%d, ", selected_numbers[j]);
    }
  }
  
  // Selektion sortieren

  // Pivots selektieren

  // Pivots verteilen

  // Blockbildung

  // Blöcke nach Rang an Knoten versenden

  // jeder Prozessor sortiert seine Blöcke

  // sortierte Blöcke einsammeln

  // Fertig!
  MPI_Finalize();
  return 0;
}
