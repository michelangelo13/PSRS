#include <stdio.h>
#include <mpi.h>

int main(int argc, char *argv[])
{
  int rank, size;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank (MPI_COMM_WORLD, &rank);
  MPI_Comm_size (MPI_COMM_WORLD, &size);
  
  // Zufallszahlen erzeugen (mit seed)
  
  // Zufallszahlen gleichmäßig verteilen
  
  int random_numbers[size * 100], numbers_per_processor[100];
  int i;
  for (i = 0; i < size * 100; i++) {
    random_numbers[i] = size * 100 - i;
  }
  
  // MPI_Scatter(sendbuf, sendcount, sendtype, recvbuf, recvcount, recvtype, root, comm)
  MPI_Scatter(random_numbers, size * 100, MPI_INT, numbers_per_processor, 100, MPI_INT, MPI_COMM_WORLD);
  printf("my rank is %d, my data is ", rank);
  for (i = 0; i < 100; i++) {
    printf("%d", numbers_per_processor[i]);
  }
  printf("\n\n");
  
  // repräsentative Selektion erstellen
  
  // Selektion auf einem Knoten einsammeln
  
  // Selektion sortieren
  
  // Pivots selektieren
  
  // Pivots verteilen
  
  // Blockbildung
  
  // Blöcke nach Rang an Knoten versenden
  
  // jeder Prozessor sortiert seine Blöcke
  
  // sortierte Blöcke einsammeln
  
  MPI_Finalize();
  return 0;
}