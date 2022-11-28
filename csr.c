#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <pthread.h>

#include "el.h"
#include "csr.h"
#include "graph.h"

/*Allocate a new CSR*/
csr_t *CSR_alloc(){
  csr_t *csr = (csr_t*)malloc(sizeof(csr_t));
  return csr;
}

/*Make an offset array of the specified size*/
csr_offset_t *CSR_alloc_offset_array(unsigned long size){

  csr_offset_t *oa = (csr_offset_t*)malloc(sizeof(csr_offset_t) * size);
  return oa;

}

/*Make a neighbor array of the specified size*/
csr_offset_t *CSR_alloc_neigh_array(unsigned long size){

  csr_vertex_t *na = (csr_vertex_t*)malloc(sizeof(csr_vertex_t) * size);
  return na;

}


/*Accumulate the offsets in CSR_offset_array and copy into CSR_offset_array_out*/
void CSR_cumul_neigh_count(csr_offset_t *CSR_offset_array, csr_offset_t *CSR_offset_array_out){

  unsigned long sum_so_far = 0; 
  for(unsigned long i = 0; i < MAX_VTX; i++){

    unsigned long tmp = CSR_offset_array[i];
    CSR_offset_array[i] = sum_so_far;
    sum_so_far += tmp;

  }
  
  memcpy(CSR_offset_array_out, CSR_offset_array, MAX_VTX * sizeof(unsigned long));

}

/*Print the neighbor counts in the csr*/
void CSR_print_neigh_counts(csr_offset_t *CSR_offset_array){
  
  printf("Neighbor Counts\n---------------\n");
  for(unsigned long i = 0; i < MAX_VTX; i++){

    if( CSR_offset_array[i] > 0 ){

      printf("v%lu %lu\n",i,CSR_offset_array[i]);

    }

  } 

}

/*count neighbors and update the OA*/
void *CSR_EL_count_neigh(el_t *el, csr_offset_t *oa){
  vertex_t *cur = (vertex_t *)el->el;
  for(int i = 0; i < el->num_edges; i++){ 

    vertex_t src = *cur;
    cur++;  
   
    vertex_t dst = *cur;
    cur++;  
   
    oa[src]++;
  }
}

/*Populate the neighbors into the CSR's NA array, destroying the OA in the
 * process*/
void *CSR_EL_neigh_pop(el_t *el, csr_t *csr){

  csr_offset_t *CSR_offset_array = csr->oa;
  csr_vertex_t *CSR_neigh_array = csr->na;

  vertex_t *cur = (vertex_t *)el->el;
  for(int i = 0; i < el->num_edges; i++){ 

    vertex_t src = *cur;
    cur++;  
   
    vertex_t dst = *cur;
    cur++;  
        
    unsigned long neigh_ind = CSR_offset_array[src];
    CSR_neigh_array[ neigh_ind ] = dst;
    CSR_offset_array[src] = CSR_offset_array[src] + 1;

  }
   
}

/*Write the CSR out to a file*/
void CSR_out(char *out, unsigned long num_edges, csr_t *csr_data){
 
  printf ("Writing out CSR data to [%s]...",out);

  csr_offset_t *CSR_offset_array_out = csr_data->oa;
  csr_vertex_t *CSR_neigh_array = csr_data->na;

  unsigned long outsize = MAX_VTX * sizeof(unsigned long) + num_edges * sizeof(vertex_t) + 2 * sizeof(unsigned long);
  struct stat stat;
 
  /*open file*/
  int outfd = open(out,  O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
  if( outfd == -1 ){  

    fprintf(stderr,"Couldn't open CSR output file\n");
    exit(-1); 

  }

  /*Seek to end and write dummy byte to make filesize large enough*/
  if(lseek(outfd, outsize - 1, SEEK_SET) == -1){
    fprintf(stderr,"csr seek error\n");
    exit(-1);
  }
 
  if(write(outfd,"",1) == -1){
    fprintf(stderr,"csr write error\n");
    exit(-1);
  }

  /*Map big empty file into memory*/
  char *csr = 
    mmap(NULL, outsize, 
          PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, outfd, 0);

  if( csr == MAP_FAILED ){
    fprintf(stderr,"%s\n",strerror(errno));
    fprintf(stderr,"Couldn't memory map CSR output file\n");
    exit(-1);
  }

  unsigned long num_vtx = MAX_VTX;
  unsigned long *tmp = (unsigned long*)csr;
  *tmp = num_vtx;
  tmp++;
  *tmp = num_edges;
  tmp++;

  /*Copy offsets from memory to file*/
  memcpy(tmp, CSR_offset_array_out, MAX_VTX * sizeof(unsigned long));

  /*Copy neighs from memory to file*/
  memcpy(tmp + MAX_VTX, CSR_neigh_array, 
         (num_edges+1) * sizeof(vertex_t));

  
  /*Sync to flush data*/
  msync(csr, outsize, MS_SYNC);
  /*unmap output file*/
  munmap(csr, outsize);
  /*close file*/
  close(outfd);
  printf("Done.\n");
  
}

/*Read a CSR in from a file*/
csr_t * CSR_in(char *infile, unsigned long *num_vertex, unsigned long *num_edges){
 
  printf ("Reading in CSR data from [%s]...",infile);

  csr_t * csr = malloc(sizeof(csr_t));
  int fd = open(infile,O_RDONLY,0);
  if( fd == -1 ){  

    fprintf(stderr,"Couldn't open CSR input file\n");
    exit(-1); 

  }
  
  struct stat stat;
  fstat(fd,&stat);
  
  size_t sz = stat.st_size;

  char b[8];
  long ret = read(fd,b, 8);
  if(ret != 8){  fprintf(stderr,"couldn't read\n");  exit(-1); }
  *num_vertex = *((unsigned long*)b);

  ret = read(fd,b, 8);
  if(ret != 8){  fprintf(stderr,"couldn't read\n");  exit(-1); }
  *num_edges = *((unsigned long*)b);
  
  char *map_csr = 
    mmap(NULL, sz, PROT_READ, MAP_PRIVATE /*| MAP_HUGETLB | MAP_HUGE_1GB*/, fd, 0);

  if( map_csr == MAP_FAILED ){
    fprintf(stderr,"bad edgelist map attempt\n");
  }
  
  csr->oa = (csr_offset_t *)map_csr; 
  (csr->oa)++;
  (csr->oa)++;

  csr->na = (csr_vertex_t *)(csr->oa + *num_vertex); 

  /*close file*/
  close(fd);
  printf("Done.\n");

  return csr;
  
}

/*Write out an edge list based on the contents of the CSR*/
void CSR_EL_out(csr_t *csr, char *fname, unsigned long num_vertex, unsigned long num_edges, int print_debug){

  unsigned long outsize = num_edges * sizeof(vertex_t) * 2;
  struct stat stat;
  printf ("Writing out EL data to [%s]...",fname);
 
  /*open file*/
  int outfd = open(fname,  O_CREAT | O_RDWR | O_TRUNC, S_IRWXU | S_IRWXG | S_IRWXO);
  if( outfd == -1 ){  

    fprintf(stderr,"Couldn't open CSR2EL output file\n");
    exit(-1); 

  }
  

  /*Seek to end and write dummy byte to make filesize large enough*/
  if(lseek(outfd, outsize - 1, SEEK_SET) == -1){
    fprintf(stderr,"csr2el seek error\n");
    exit(-1);
  }
 
  if(write(outfd,"",1) == -1){
    fprintf(stderr,"csr2el write error\n");
    exit(-1);
  }

  /*Map big empty file into memory*/
  char *el_map = 
    mmap(NULL, outsize, 
          PROT_READ | PROT_WRITE | PROT_EXEC, MAP_SHARED, outfd, 0);

  if( el_map == MAP_FAILED ){
    fprintf(stderr,"%s\n",strerror(errno));
    fprintf(stderr,"Couldn't memory map CSR2EL output file\n");
    exit(-1);
  }

  el_t *el = (el_t *)malloc(sizeof(el_t));
  el->num_edges = num_edges;
  el->el = el_map;

  vertex_t *cur = (vertex_t*) el->el;

  /*handle all but last vertex*/
  for(unsigned long i = 0; i < MAX_VTX; i++){
   
    unsigned long cur_edges = 0;
    if( i < MAX_VTX-1){
      cur_edges = csr->oa[i+1] - csr->oa[i];
    }else{
      cur_edges = num_edges - csr->oa[i];
    }

    csr_offset_t cur_offset = csr->oa[i];

    if(print_debug){ 
      fprintf(stderr,"%lu: ",(unsigned long)i);
    }

    for(unsigned long j = cur_offset; j < cur_offset + cur_edges; j++){
      if(print_debug){ 
        fprintf(stderr,"(%lu %lu) ",(unsigned long)i, csr->na[j]);
      }
      *cur = (unsigned long)i;  cur++;
      *cur = csr->na[j]; cur++;
    }

    if(print_debug){ 
      fprintf(stderr,"\n");
    }

  }
  fprintf(stderr,"Done!\n");


}

