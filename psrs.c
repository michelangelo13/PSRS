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