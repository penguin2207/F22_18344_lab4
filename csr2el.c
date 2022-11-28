#include <stdio.h>
#include <stdlib.h>
#include "el.h"
#include "csr.h"
#include "graph.h"

int main(int argc, char *argv[]){

  if( argc < 3 ){
    fprintf(stderr,"csr2el loads in a csr and produces a canonicalized edge list\n\nUsage: csr2el <input csr file> <output el file> [printdebug]\n");
    exit(-1);
  }

  int printdebug = 0; 
  if( argc == 4){
    printdebug = atoi(argv[3]); 
  }

  unsigned long num_vertex = 0; 
  unsigned long num_edges = 0; 
  csr_t *csr = CSR_in(argv[1],&num_vertex,&num_edges);
  CSR_EL_out(csr,argv[2],num_vertex,num_edges,printdebug);
 
}
