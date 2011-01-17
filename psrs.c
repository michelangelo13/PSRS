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

  if ( rank == 0 )
  {
    // Zufallszahlen erzeugen (mit seed)
    int numbers_size = atoi( argv[1] );
    int numbers[ numbers_size ];
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
    // Zufallszahlen gleichmäßig verteilen
  }  
  
  // repräsentative Selektion erstellen
  
  // Selektion auf einem Knoten einsammeln
  
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
