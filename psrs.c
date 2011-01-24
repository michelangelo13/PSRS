#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <string.h>
#include <mpi.h>

#define SEED 42
#define RAND_MAGNITUDE_FACTOR 5
#define PRINT_PER_ROW 25

// Zufallszahlen erzeugen
void generate_random_numbers( int numbers[], int amount );
// lokale Zahlen mit Pivots in Blöcke teilen
void divide_into_blocks( int block_sizes[], int size, int numbers_per_processor[], int numbers_per_processor_size, int pivots[] );
// Displacements bestimmen
void displacements( int displacements[], int part_sizes[], int size );

// sonstige Hilfsfunktionen
void quicksort( int a[], int l, int r );
void print_array( int a[], int size_of_a );
int sum( int part_sizes[], int part_sizes_length );
int is_sorted( int numbers[], int numbers_size );

int main( int argc, char *argv[] )
{
  // MPI-Variablen und Initialisierung
  int rank, size;
  MPI_Init(&argc, &argv);
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  MPI_Comm_size (MPI_COMM_WORLD, &size);

  // Zeitmesspunkte
  double time_start, time_gen, time_rest_1, time_local_sort, time_rest_2;

  // (minimalistischer) Plausibilitäts-Check der Übergabeparameter
  if ( argc < 2 )
  {
    if ( rank == 0 )
      printf( "Synopsis: psrs <size of random array> [<output mode=(silent|table|full)>]\n" );
    MPI_Finalize();
    return 1;
  }

  // Größe des zu erzeugenden Zahlenfeldes bestimmen
  int numbers_size = atoi( argv[1] );

  // Detailgrad der Ausgabe auf stdout bestimmen
  int output_level = 0;
  if ( argc == 3 )
  {
    if ( strcmp( argv[2], "full" ) == 0 )
      output_level = 0;
    if ( strcmp( argv[2], "table" ) == 0 )
      output_level = 1;
    if ( strcmp( argv[2], "silent" ) == 0 )
      output_level = 2;
  }

  // Zeitmessung initialisieren
  time_start = MPI_Wtime();

  // Zufallszahlen erzeugen
  int numbers[ numbers_size ];
  if (rank == 0) {
    // eigentliche Erzeugung
    generate_random_numbers(numbers, numbers_size);
    // Ausgabe der Aufrufparameter
    if ( output_level == 0 )
      printf("starting to sort %d values on %d nodes\n\n", numbers_size, size);
    if ( output_level == 1 )
      printf("%d %d ", numbers_size, size);
  }

  // Zeit für Zufallszahlengenerierung messen
  time_gen = MPI_Wtime() - time_start;

  // Zufallszahlen gleichmäßig verteilen
  int temp_numbers_per_processor_size = numbers_size / size;
  // Anzahl der bei Abrundung von ( numbers_size / size ) übrig gebliebenen Zahlen
  int spare_numbers = numbers_size - temp_numbers_per_processor_size * size;
  int numbers_per_processor_sizes[ size ];
  for ( int pos = 0; pos < size; pos++ )
  {
    numbers_per_processor_sizes[ pos ] = temp_numbers_per_processor_size;
    // übrig gebliebene Zahlen auf erste Knoten aufteilen
    if (spare_numbers > 0)
    {
      numbers_per_processor_sizes[ pos ]++;
      spare_numbers--;
    }
  }
  
  int numbers_per_processor_size;
  MPI_Scatter(numbers_per_processor_sizes, 1, MPI_INT, &numbers_per_processor_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
  
  int scatter_displacements[size];
  displacements(scatter_displacements, numbers_per_processor_sizes, size);
  
  int numbers_per_processor[ numbers_per_processor_size ];
  MPI_Scatterv(numbers, numbers_per_processor_sizes, scatter_displacements, MPI_INT, numbers_per_processor,
    numbers_per_processor_size, MPI_INT, 0, MPI_COMM_WORLD);

  // Kommunikationszeit für Verteilung der Zufallszahlen
  time_rest_1 = MPI_Wtime() - time_start - time_gen;

  // lokal sortieren
  quicksort( numbers_per_processor, 0, numbers_per_processor_size-1 );

  // Zeit für lokales Sortieren
  time_local_sort = MPI_Wtime() - time_start - time_gen - time_rest_1;

  // repräsentative Auswahl erstellen
  int w = numbers_size / ( size * size );
  int representative_selection[ size ];
  for( int pos=0; pos < size; pos++ )
    representative_selection[ pos ] = numbers_per_processor[ pos * w ];

  // Auswahl auf einem Knoten einsammeln
  int selected_numbers[ size * size ];
  MPI_Gather( representative_selection, size, MPI_INT, selected_numbers, size, MPI_INT, 0, MPI_COMM_WORLD );

  // Auswahl sortieren
  int pivots[ size - 1 ];
  if ( rank == 0 )
  {
    quicksort( selected_numbers, 0, size * size - 1 );
    // Pivots selektieren
    int t = size / 2;
    for ( int pos = 1; pos < size; pos++ )
      pivots[ pos - 1 ] = selected_numbers[ pos * size + t ];
  }

  // Pivots verteilen
  MPI_Bcast( pivots, size - 1, MPI_INT, 0, MPI_COMM_WORLD );

  // Blockbildung
  int block_sizes[ size ];
  divide_into_blocks( block_sizes, size, numbers_per_processor, numbers_per_processor_size, pivots );

  // Blöcke nach Rang an Knoten versenden
  int receive_block_sizes[ size ];
  MPI_Alltoall( block_sizes, 1, MPI_INT, receive_block_sizes, 1, MPI_INT, MPI_COMM_WORLD );
  
  int receive_block_displacements[ size ];
  displacements( receive_block_displacements, receive_block_sizes, size );
  int block_displacements[ size ];
  displacements( block_displacements, block_sizes, size );

  int blocksize = sum( receive_block_sizes, size );
  int blocks_per_processor[ blocksize ];
  
  MPI_Alltoallv( numbers_per_processor, block_sizes, block_displacements, MPI_INT, blocks_per_processor,
    receive_block_sizes, receive_block_displacements, MPI_INT, MPI_COMM_WORLD );

  // jeder Knoten sortiert seine Blöcke
  quicksort( blocks_per_processor, 0, blocksize -1 );

  // sortierte Blöcke einsammeln
  // kann durch MPI_Allgather statt MPI_Alltoall vermieden werden
  int blocksizes[ size ];
  MPI_Gather( &blocksize, 1, MPI_INT, blocksizes, 1, MPI_INT, 0, MPI_COMM_WORLD );

  int receive_sorted_displacements[ size ];
  displacements( receive_sorted_displacements, blocksizes, size );

  int sorted[ numbers_size ];
  MPI_Gatherv( blocks_per_processor, blocksize, MPI_INT, sorted, blocksizes, receive_sorted_displacements,
    MPI_INT, 0, MPI_COMM_WORLD );

  // Kommunikationszeit für alles andere
  time_rest_2 = MPI_Wtime() - time_start - time_gen - time_rest_1 - time_local_sort;

  // lokale Sortierzeiten zusammenrechnen
  double time_total_local_sort = 0.0;
  MPI_Reduce( &time_local_sort, &time_total_local_sort, 1, MPI_DOUBLE, MPI_SUM, 0, MPI_COMM_WORLD );

  // Fertig!
  if ( rank == 0 )
  {
    if ( output_level == 0 )
    {
      print_array( sorted, numbers_size );
      printf( "\nnumber generation time          = %.15f msec\n",
	      time_gen * 1000 );
      printf( "local sorting time              = %.15f msec\n",
	      time_local_sort * 1000 );
      printf( "total local sorting time        = %.15f msec\n",
	      time_total_local_sort * 1000 );
      printf( "communication/organization time = %.15f msec\n",
	      ( time_rest_1 + time_rest_2 ) * 1000 );
      printf( "-------------------------------\n" );
      printf( "total time                      = %.15f msec\n",
	      ( time_rest_1 + time_rest_2 + time_total_local_sort + time_gen ) * 1000 );
      printf( "total time w/o generation       = %.15f msec\n",
	      ( time_rest_1 + time_rest_2 + time_total_local_sort ) * 1000 );
    }
    if ( output_level == 1 )
    {
      printf( "%.15f %.15f %.15f %.15f %.15f %.15f ",
	      time_gen * 1000, // Zeit zur Generation des Zufallsfelds
	      time_local_sort * 1000, // lokale Sortierzeit auf Knoten 0
	      time_total_local_sort * 1000, // Summe der lokalen Sortierzeiten
	      ( time_rest_1 + time_rest_2 ) * 1000, // Kommunikationszeit auf Knoten 0
	      ( time_rest_1 + time_rest_2 + time_total_local_sort + time_gen ) * 1000, // Gesamtzeit mit Generation des Zufallsfelds
	      ( time_rest_1 + time_rest_2 + time_total_local_sort ) * 1000 // Gesamtzeit ohne Generation des Zufallsfelds
      );
      if ( is_sorted( sorted, numbers_size ) == 1 )
	printf( "valid\n" );
      else
	printf( "invalid\n" );
    }
  }
  MPI_Finalize();
  return 0;
}

void print_array( int a[], int size_of_a )
{
  for( int pos=0; pos<size_of_a; pos++ )
  {
    printf( "%d ", a[pos] );
    if ( pos > 0 && ( pos % PRINT_PER_ROW ) == 0 )
      printf( "\n" );
  }
  printf( "\n" );
}

void quicksort( int a[], int l, int r )
{
  if( r > l )
  {
    int i = l -1, j = r, tmp;
    for( ; ; )
    {
      while( a[ ++i ] < a[ r ] );
      while( a[ --j ] > a[ r ] && j > i );
      if( i >= j )
	break;
      tmp = a[ i ];
      a[ i ] = a[ j ];
      a[ j ] = tmp;
    }
    tmp = a[ i ];
    a[ i ] = a[ r ];
    a[ r ] = tmp;
    quicksort( a, l, i -1 );
    quicksort( a, i +1, r );
  }
}

void generate_random_numbers( int numbers[], int amount )
{
  srand ( SEED );
  for( int pos=0; pos < amount; pos++ )
  {
    int next;
    int already_in_numbers;
    do
    {
      already_in_numbers = 0;
      next = rand() % ( RAND_MAGNITUDE_FACTOR * amount );
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

void divide_into_blocks( int block_sizes[], int size, int numbers_per_processor[], int numbers_per_processor_size, int pivots[] )
{
  int pivot_pos = 0;
  int pivot;  
  if ( size == 1 ) // to handle 0 pivots
    pivot = INT_MAX;
  else
    pivot = pivots[pivot_pos];  
  int block_sizes_pos = 0;
  int length = 0;
  for( int pos=0; pos < numbers_per_processor_size; pos++ )
  {
    if( numbers_per_processor[ pos ] <= pivot )
      length++;
    else
    {
      block_sizes[ block_sizes_pos ] = length;
      block_sizes_pos++;
      length = 0;
      if( ++pivot_pos < ( size - 1 ) )
        pivot = pivots[ pivot_pos ];
      else
        pivot = INT_MAX;
      pos--;
    }
  }
  block_sizes[ block_sizes_pos ] = length;
  for ( int pos = block_sizes_pos +1; pos < size; pos++ )
    block_sizes[ pos ] = 0;
}

void displacements( int displacements[], int part_sizes[], int size )
{
  displacements[ 0 ] = 0;
  for ( int pos = 1; pos < size; pos++ )
    displacements[ pos ] = part_sizes[ pos-1 ] + displacements[ pos-1 ];
}

int sum( int part_sizes[], int part_sizes_length )
{
  int sum = 0;
  for ( int pos = 0; pos < part_sizes_length; pos++ )
    sum += part_sizes[ pos ];
  return sum;
}

int is_sorted( int numbers[], int numbers_size )
{
  int is_sorted = 1;
  if ( numbers_size > 1 )
    for ( int pos = 1; pos < numbers_size; pos++ )
      if ( numbers[ pos ] < numbers[ pos-1 ] )
      {
	is_sorted = 0;
	break;
      }
  return is_sorted; 
}
