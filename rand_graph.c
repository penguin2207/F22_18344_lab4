#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <time.h> 
#include <unistd.h>
#include <errno.h>

#include "graph.h"


unsigned long num_edges;
void write_rand_el_file(char *f){

  struct stat stat;
 
  /*open file*/
  int fd = open(f,  O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
  if( fd == -1 ){  

    fprintf(stderr,"bad edgelist file\n");
    exit(-1); 

  }

  /*Seek to end and write dummy byte to make filesize large enough*/
  if(lseek(fd, num_edges * sizeof(unsigned long) * 2 - 1, SEEK_SET) == -1){
    fprintf(stderr,"seek error\n");
    exit(-1);
  }
 
  if(write(fd,"",1) == -1){
    fprintf(stderr,"write error\n");
    exit(-1);
  }

  /*Map big empty file into memory*/
  char *el = 
    mmap(NULL, num_edges * sizeof(unsigned long) * 2, 
          PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, fd, 0);

  if( el == MAP_FAILED ){
    fprintf(stderr,"%s\n",strerror(errno));
    fprintf(stderr,"bad edgelist map attempt\n");
    exit(-1);
  }

  printf ("Generating edge array...");
  unsigned long *edges = malloc(sizeof(unsigned long) * num_edges * 2);  
  for(int i = 0; i < num_edges * 2; i += 2){

    edges[i] = (rand() % (MAX_VTX-1)) + 1; 
    edges[i+1] = (rand() % (MAX_VTX-1)) + 1;
    if(edges[i+1] == edges[i]){ 
      edges[i]++;  /*No self edges in this format*/
    }

  }

  /*Copy from edges in memory to edges in file*/
  printf ("Done.\n");
  printf ("Copying to file...");
  memcpy(el, edges, num_edges * sizeof(unsigned long) * 2);

  /*Sync to flush data*/
  printf ("Done.\n");
  printf ("Syncing file...");
  msync(el, num_edges * sizeof(unsigned long) * 2, MS_SYNC);

  /*unmap output file*/
  printf ("Done.\n");
  printf ("Unmapping file...");
  munmap(el, num_edges * sizeof(unsigned long) * 2);

  /*close file*/
  printf ("Done.\n");
  close(fd);

}

int main (int argc, char *argv[]){

  srand( time(NULL) );

  if(argc != 3){ 
    fprintf(stderr,"rand_graph <outfile> <numedges>\n"); 
    exit(-1); 
  }

  num_edges = atol(argv[2]);
  printf("Generating %lu edges\n",num_edges);
  write_rand_el_file(argv[1]);

}

