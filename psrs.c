#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <mpi.h>

#define SEED 42
#define RAND_MAGNITUDE_FACTOR 5
#define PRINT_PER_ROW 25

void print_array( int a[], int size_of_a );
void quicksort( int a[], int l, int r );

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
  MPI_Scatter(numbers, numbers_per_processor_size, MPI_INT, numbers_per_processor, numbers_per_processor_size, MPI_INT, 0, MPI_COMM_WORLD);

  // lokal sortieren
  quicksort( numbers_per_processor, 0, numbers_per_processor_size-1 );
  // print_array( numbers_per_processor, numbers_per_processor_size );

  // repräsentative Selektion erstellen
  int w = numbers_size / ( size * size );
  int representative_selection[ size ];
  for( int pos=0; pos < size; pos++ )
  {
    representative_selection[ pos ] = numbers_per_processor[ pos * w ];
  }

  // Selektion auf einem Knoten einsammeln
  // int MPI_Gather(void *sendbuf, int sendcount, MPI_Datatype sendtype, void *recvbuf, int recvcount, MPI_Datatype, recvtype, int root, MPI_Comm comm)
  int selected_numbers[ size * size ];
  MPI_Gather(representative_selection, size, MPI_INT, selected_numbers, size, MPI_INT, 0, MPI_COMM_WORLD);

  // Selektion sortieren
  int pivots[size - 1];
  if (rank == 0) {
    quicksort(selected_numbers, 0, size * size - 1);
    // print_array(selected_numbers, size * size);

    // Pivots selektieren
    int t = size / 2;
    for (int pos = 1; pos < size; pos++) {
      pivots[pos - 1] = selected_numbers[pos * size + t];
    }
  }

  // Pivots verteilen
  // int MPI_Bcast ( void *buffer, int count, MPI_Datatype datatype, int root, MPI_Comm comm)
  MPI_Bcast(pivots, size - 1, MPI_INT, 0, MPI_COMM_WORLD);

  // Blockbildung
  int* blocks[ size ];
  int block_sizes[ size ];
  for( int pos=0; pos < size; pos++ )
  {
    blocks[ pos ] = ( int* ) malloc( numbers_per_processor_size * sizeof( int ) );
  }
  int pivot_pos = 0;
  int pivot = pivots[ pivot_pos ];
  int block_pos = 0;
  int in_block_pos = 0;
  for( int pos=0; pos < numbers_per_processor_size; pos++ )
  {
    if( numbers_per_processor[ pos ] <= pivot )
    {
      blocks[ block_pos ][ in_block_pos++ ] = numbers_per_processor[ pos ];
    }
    else
    {
      block_sizes[ block_pos ] = in_block_pos;
      blocks[ block_pos ] = ( int* ) realloc( blocks[ block_pos ], block_sizes[ block_pos ] * sizeof( int ) );
      block_pos++;
      in_block_pos = 0;
      if( pivot_pos < ( size-1 ) -1 )
        pivot = pivots[ ++pivot_pos ];
      else
        pivot = INT_MAX;
      pos--;
    }
  }
  block_sizes[ block_pos ] = in_block_pos;
  blocks[ block_pos ] = ( int* ) realloc( blocks[ block_pos ], block_sizes[ block_pos ] * sizeof( int ) );

  // Blöcke nach Rang an Knoten versenden
  int receive_block_sizes[size];
  int receive_block_displacements[size];

  MPI_Alltoall(block_sizes, 1, MPI_INT, receive_block_sizes, 1, MPI_INT, MPI_COMM_WORLD);

  receive_block_displacements[0] = 0;
  for (int pos = 1; pos < size; pos++) {
    receive_block_displacements[pos] = receive_block_sizes[pos - 1] + receive_block_displacements[pos - 1];
  }

  int displacements[size];
  displacements[0] = 0;
  for (int pos = 1; pos < size; pos++) {
    displacements[pos] = block_sizes[pos - 1] + displacements[pos - 1];
  }
  int blocksize = 0;

  for (int pos = 0; pos < size; pos++) {
    blocksize += receive_block_sizes[pos];
  }

  int blocks_per_processor[blocksize];

  MPI_Alltoallv(numbers_per_processor, block_sizes, displacements, MPI_INT, blocks_per_processor, receive_block_sizes, receive_block_displacements, MPI_INT, MPI_COMM_WORLD);

  // jeder Prozessor sortiert seine Blöcke
  quicksort(blocks_per_processor, 0, blocksize - 1);
  // sortierte Blöcke einsammeln
  int blocksizes[size];
    // kann durch MPI_Allgather statt MPI_Alltoall vermieden werden
  MPI_Gather(&blocksize, 1, MPI_INT, blocksizes, 1, MPI_INT, 0, MPI_COMM_WORLD);

  int receive_sorted_displacements[size];
  receive_sorted_displacements[0] = 0;
  for (int pos = 1; pos < size; pos++) {
    receive_sorted_displacements[pos] = blocksizes[pos - 1] + receive_sorted_displacements[pos - 1];
  }

  int sorted[numbers_size];
  MPI_Gatherv(blocks_per_processor, blocksize, MPI_INT, sorted, blocksizes, receive_sorted_displacements, MPI_INT, 0, MPI_COMM_WORLD);
  // Fertig!
  if (rank == 0) {
    printf("Sorted numbers: \n");
    print_array(sorted, numbers_size);
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

void quicksort(int a[], int l, int r)
{
  if(r>l){
    int i=l-1, j=r, tmp;
    for(;;){
      while(a[++i]<a[r]);
      while(a[--j]>a[r] && j>i);
      if(i>=j) break;
      tmp=a[i]; a[i]=a[j]; a[j]=tmp;
    }
    tmp=a[i]; a[i]=a[r]; a[r]=tmp;

    quicksort(a, l, i-1);
    quicksort(a, i+1, r);
  }
}
