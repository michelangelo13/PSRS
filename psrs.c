#include <stdio.h>
#include <stdlib.h>
#include <limits.h>
#include <mpi.h>

#define SEED 42
#define RAND_MAGNITUDE_FACTOR 5
#define PRINT_PER_ROW 25

void print_array( int a[], int size_of_a );
void quicksort( int a[], int l, int r );
void generate_random_numbers(int numbers[], int amount);
void divide_into_blocks(int block_sizes[], int size, int numbers_per_processor[], int numbers_per_processor_size, int pivots[]);
void displacements(int displacements[], int part_sizes[], int size);
int sum(int part_sizes[], int part_sizes_length);

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

  // Zufallszahlen erzeugen (mit seed)
  int numbers_size = atoi( argv[1] ); // amount of numbers to generate
  int numbers[ numbers_size ]; // will contain all generated numbers in root process
  if (rank == 0) {
    generate_random_numbers(numbers, numbers_size);
  }

  // Zufallszahlen gleichmaessig verteilen
  int temp_numbers_per_processor_size = numbers_size / size;
  int spare_numbers = numbers_size - temp_numbers_per_processor_size * size;
  
  int numbers_per_processor_sizes[size];
  for (int pos = 0; pos < size; pos++) {
    numbers_per_processor_sizes[pos] = temp_numbers_per_processor_size;
    if (spare_numbers > 0) {
      numbers_per_processor_sizes[pos]++;
      spare_numbers--;
    }
  }
  
  int numbers_per_processor_size;
  MPI_Scatter(numbers_per_processor_sizes, 1, MPI_INT, &numbers_per_processor_size, 1, MPI_INT, 0, MPI_COMM_WORLD);
  
  int numbers_per_processor[ numbers_per_processor_size ];
  MPI_Scatter(numbers, numbers_per_processor_size, MPI_INT, numbers_per_processor, numbers_per_processor_size, MPI_INT, 0, MPI_COMM_WORLD);

  // lokal sortieren
  quicksort( numbers_per_processor, 0, numbers_per_processor_size-1 );

  // repraesentative Selektion erstellen
  int w = numbers_size / ( size * size );
  int representative_selection[ size ];
  for( int pos=0; pos < size; pos++ )
  {
    representative_selection[ pos ] = numbers_per_processor[ pos * w ];
  }

  // Selektion auf einem Knoten einsammeln
  int selected_numbers[ size * size ];
  MPI_Gather(representative_selection, size, MPI_INT, selected_numbers, size, MPI_INT, 0, MPI_COMM_WORLD);

  // Selektion sortieren
  int pivots[size - 1];
  if (rank == 0) {
    quicksort(selected_numbers, 0, size * size - 1);

    // Pivots selektieren
    int t = size / 2;
    for (int pos = 1; pos < size; pos++) {
      pivots[pos - 1] = selected_numbers[pos * size + t];
    }
  }

  // Pivots verteilen
  MPI_Bcast(pivots, size - 1, MPI_INT, 0, MPI_COMM_WORLD);

  // Blockbildung
  int block_sizes[ size ];
  divide_into_blocks(block_sizes, size, numbers_per_processor, numbers_per_processor_size, pivots);

  // Bloecke nach Rang an Knoten versenden
  int receive_block_sizes[size];
  MPI_Alltoall(block_sizes, 1, MPI_INT, receive_block_sizes, 1, MPI_INT, MPI_COMM_WORLD);
  
  int receive_block_displacements[size];
  displacements(receive_block_displacements, receive_block_sizes, size);
  int block_displacements[size];
  displacements(block_displacements, block_sizes, size);

  int blocksize = sum(receive_block_sizes, size);
  int blocks_per_processor[blocksize];

  MPI_Alltoallv(numbers_per_processor, block_sizes, block_displacements, MPI_INT, blocks_per_processor, receive_block_sizes, receive_block_displacements, MPI_INT, MPI_COMM_WORLD);

  // jeder Prozessor sortiert seine Bloecke
  quicksort(blocks_per_processor, 0, blocksize - 1);
  // sortierte Bloecke einsammeln
  // kann durch MPI_Allgather statt MPI_Alltoall vermieden werden
  int blocksizes[size];
  MPI_Gather(&blocksize, 1, MPI_INT, blocksizes, 1, MPI_INT, 0, MPI_COMM_WORLD);

  int receive_sorted_displacements[size];
  displacements(receive_sorted_displacements, blocksizes, size);

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

void generate_random_numbers(int numbers[], int amount) {
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

void divide_into_blocks(int block_sizes[], int size, int numbers_per_processor[], int numbers_per_processor_size, int pivots[]) {
  int pivot_pos = 0;
  int pivot = pivots[ pivot_pos ];
  int block_sizes_pos = 0;
  int length = 0;
  for( int pos=0; pos < numbers_per_processor_size; pos++ )
  {
    if( numbers_per_processor[ pos ] <= pivot )
    {
      length++;
    }
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
}

void displacements(int displacements[], int part_sizes[], int size) {
  displacements[0] = 0;
  for (int pos = 1; pos < size; pos++) {
    displacements[pos] = part_sizes[pos - 1] + displacements[pos - 1];
  }
}

int sum(int part_sizes[], int part_sizes_length) {
  int sum = 0;
  for (int pos = 0; pos < part_sizes_length; pos++) {
    sum += part_sizes[pos];
  }
  return sum;
}
