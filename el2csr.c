#include <stdio.h>
#include <stdlib.h>
#include "el.h"
#include "csr.h"
#include "graph.h"
#include <sys/mman.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>

typedef struct bin {

  edge_t *es;
  int entries;
  int maxEnt;

} bin_t;

bin_t *bins;

unsigned long NUM_BINS;

void bin_init(el_t *el){
  NUM_BINS = el->num_edges/2 + 1; // Can calculate num bins differently
  bins = (bin_t*)malloc(sizeof(bin_t)*(NUM_BINS));

  for(int i = 0; i<NUM_BINS; i++){
    bins[i].es = calloc(64, sizeof(edge_t));
    bins[i].entries = 0;
    bins[i].maxEnt = 64;
  }
}

void bin_fill(unsigned long edges, edge_t *e){
  int ind = 0;
  for(unsigned long i = 0; i < MAX_VTX; i+=MAX_VTX/NUM_BINS){
    if(e->dst <= (i+MAX_VTX/NUM_BINS))
      break;
    ind++;
  }
  bins[ind].es[bins[ind].entries].src = e->src;
  bins[ind].es[bins[ind].entries].dst = e->dst;
  bins[ind].entries++;
  if(bins[ind].entries == bins[ind].maxEnt){
    edge_t *new_es = calloc(bins[ind].maxEnt*2, sizeof(edge_t));
    memcpy(new_es, bins[ind].es, bins[ind].maxEnt*sizeof(edge_t));
    free(bins[ind].es);
    bins[ind].maxEnt = bins[ind].maxEnt*2;
    bins[ind].es = new_es;
  }

}

void bin_print(){
  unsigned long bin_edges = 0;
  for (unsigned long i = 0; i<NUM_BINS; i++){
    printf("Bin[%d] ", i);
    if(bins[i].es){
      bin_edges = bins[i].entries;
      for(int j = 0; j<bin_edges; j++)
        printf("Entry[%d]: src: %d, dst: %d\n", j, bins[i].es[j].src, bins[i].es[j].dst);
      
      printf("\n");
    }
  }

}

int pb_bin_EL(el_t *el){
  bin_init(el);
  if(!bins) return -1;
  unsigned long edges = el->num_edges;
  unsigned long s_or_d = (unsigned long) 0;
  edge_t *cur_edge = (edge_t *)calloc(1, sizeof(edge_t));
  unsigned long byte_count = 0;
  for(unsigned long i = 0; i<edges; i++){
    for(unsigned long j = 0; j<8; j++){
      s_or_d |= ((el->el[byte_count])&0xffUL) << (j*8);
      byte_count++;
    }
    cur_edge->src = s_or_d;
    s_or_d = (unsigned long) 0;
    for(unsigned long j = 0; j<8; j++){
      s_or_d |= ((el->el[byte_count])&0xffUL) << (j*8);
      byte_count++;
    }
    cur_edge->dst = s_or_d;
    bin_fill(el->num_edges, cur_edge);
    s_or_d = (unsigned long) 0;
  }
  return 0;

}


void *PB_CSR_count_neigh(el_t *el, csr_offset_t *oa){
  unsigned long bin_edges = 0;
  for (unsigned long i = 0; i<NUM_BINS; i++){
    if(bins[i].es){
      bin_edges = bins[i].entries;
      for(int j = 0; j<bin_edges; j++){
        vertex_t src = bins[i].es[j].src;
        oa[src]++;
      }
    }
  }
}

void *PB_CSR_neigh_pop(el_t *el, csr_t *csr){

  csr_offset_t *CSR_offset_array = csr->oa;
  csr_vertex_t *CSR_neigh_array = csr->na;

  unsigned long bin_edges = 0;

  for (unsigned long i = 0; i<NUM_BINS; i++){
    if(bins[i].es){
      bin_edges = bins[i].entries;
      for(int j = 0; j<bin_edges; j++){
        vertex_t src = bins[i].es[j].src;
        vertex_t dst = bins[i].es[j].dst;
        unsigned long neigh_ind = CSR_offset_array[src];
        CSR_neigh_array[neigh_ind] = dst;
        CSR_offset_array[src] = CSR_offset_array[src] + 1;
      }
    }
  }
   
}



/*Convert an input edge list in the file in argv[1] into a csr in the file in
 * argv[2]*/
int main(int argc, char *argv[]){

  if( argc != 3 ){
    fprintf(stderr,"Usage: pb <input edge list file> <output csr file>\n");
    exit(-1);
  }
  el_t *el = init_el_file(argv[1]);

  /*This is an argument to CSR_count_neigh*/  
  csr_offset_t *g_oa = CSR_alloc_offset_array(MAX_VTX);

  /*TODO: insert a call to your implementation of pb_bin_EL(el,...)
          which should populate your bins.
  */
 pb_bin_EL(el);
 bin_print();
  
  printf("Counting neighbors...");
  /*TODO: replace this call with a call to your PB_CSR_count_neigh(...)*/
  PB_CSR_count_neigh(el,g_oa);
  //CSR_EL_count_neigh(el,g_oa);
  printf("Done.\n");

  /*g_oa contains offsets counted from edge list*/

  /*Sequential accumulation round*/ 
  printf("Accumulating neighbor counts...");
  csr_t *csr = CSR_alloc();
  csr->na = CSR_alloc_neigh_array(el->num_edges);
  csr->oa = g_oa;

  /*Need a spare one of these for output later*/
  csr_offset_t *oa_out = CSR_alloc_offset_array(MAX_VTX);
  CSR_cumul_neigh_count(csr->oa, oa_out);
  printf("Done.\n");


  /*Done with bin read for neighbor count.  Now bin read for neighbor pop*/

  printf("Populating neighbors...");
  /*TODO: replace this call with a call to your PB_CSR_neigh_pop(...)*/
  PB_CSR_neigh_pop(el,csr);
  //CSR_EL_neigh_pop(el,csr);
  printf("Done.\n");
 
 
  /*Put the "output" OA into the CSR to use in output*/  
  csr->oa = oa_out;
  CSR_out(argv[2],el->num_edges,csr);
 
}
