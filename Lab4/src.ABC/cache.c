#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cache.h"

extern uns64 CACHE_LINESIZE; // adding this here because need line size.
extern uns64 cycle; // You can use this as timestamp for LRU

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE INIT FUNCTION -----------
////////////////////////////////////////////////////////////////////

Cache  *cache_new(uns64 size, uns64 assoc, uns64 linesize, uns64 repl_policy){

   Cache *c = (Cache *) calloc (1, sizeof (Cache));
   c->num_ways = assoc;
   c->repl_policy = repl_policy;

   if(c->num_ways > MAX_WAYS){
     printf("Change MAX_WAYS in cache.h to support %llu ways\n", c->num_ways);
     exit(-1);
   }

   // determine num sets, and init the cache
   c->num_sets = size/(linesize*assoc);
   c->sets  = (Cache_Set *) calloc (c->num_sets, sizeof(Cache_Set));

   return c;
}

////////////////////////////////////////////////////////////////////
// ------------- DO NOT MODIFY THE PRINT STATS FUNCTION -----------
////////////////////////////////////////////////////////////////////

void    cache_print_stats    (Cache *c, char *header){
  double read_mr =0;
  double write_mr =0;

  if(c->stat_read_access){
    read_mr=(double)(c->stat_read_miss)/(double)(c->stat_read_access);
  }

  if(c->stat_write_access){
    write_mr=(double)(c->stat_write_miss)/(double)(c->stat_write_access);
  }

  printf("\n%s_READ_ACCESS    \t\t : %10llu", header, c->stat_read_access);
  printf("\n%s_WRITE_ACCESS   \t\t : %10llu", header, c->stat_write_access);
  printf("\n%s_READ_MISS      \t\t : %10llu", header, c->stat_read_miss);
  printf("\n%s_WRITE_MISS     \t\t : %10llu", header, c->stat_write_miss);
  printf("\n%s_READ_MISSPERC  \t\t : %10.3f", header, 100*read_mr);
  printf("\n%s_WRITE_MISSPERC \t\t : %10.3f", header, 100*write_mr);
  printf("\n%s_DIRTY_EVICTS   \t\t : %10llu", header, c->stat_dirty_evicts);

  printf("\n");
}



////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Return HIT if access hits in the cache, MISS otherwise 
// Also if is_write is TRUE, then mark the resident line as dirty
// Update appropriate stats
////////////////////////////////////////////////////////////////////

typedef unsigned int uint32_t;
typedef unsigned long uint64_t;

uint32_t log2_int(uint32_t num)
{
  uint32_t bit;
  for(bit = 31; bit > 0; bit--)
  {
    uint32_t mask = 1 << bit;
    if((mask & num) > 0)
    {
      return bit;
    }
  }
  // log2 of 0 = 0 ?
  return 0;
}

// there exists line size and cache size / assoc globals in sim.cpp
Flag cache_access(Cache *c, Addr lineaddr, uns is_write, uns core_id){
  // Your Code Goes Here
  c->stat_write_access++;
  c->stat_read_access++;

  // get the log2 of the linesize, then we shift the addr over by that many and get the index
  // then and with num_ways - 1, which should give us all the bits representing the index.
  uint32_t index = (lineaddr >> log2_int(CACHE_LINESIZE)) & (c->num_ways-1);
  uint32_t tag = (lineaddr >> (log2_int(CACHE_LINESIZE) + log2_int(c->num_ways)));

  uint32_t i;
  for(i=0; i<c->num_sets; i++)
  {
    // not sure whether tag is the address or just tag bits of address.
    if(c->sets[i].line[index].valid && c->sets[i].line[index].tag == tag) // == lineaddr
    {
      return HIT;
    }
  }
  
  if (is_write) {
    c->stat_write_miss++;
  }
  else {
    c->stat_read_miss++;
  }

  return MISS;
}

////////////////////////////////////////////////////////////////////
// Note: the system provides the cache with the line address
// Install the line: determine victim using repl policy (LRU/RAND)
// copy victim into last_evicted_line for tracking writebacks
////////////////////////////////////////////////////////////////////

// there exists line size and cache size / assoc globals in sim.cpp
uint32_t find_lru(Cache *c, uint32_t index)
{
  int lru = -1;
  uint64_t oldest;

  int i;
  for(i=0; i<c->num_sets; i++)
  {
    if(!c->sets[i].line[index].valid)
    {
      return i;
    }
    else if(lru == -1 || c->sets[i].line[index].last_access_time < oldest)
    {
      lru = i;
      oldest = c->sets[i].line[index].last_access_time;
    }
  }
  return lru;
}

// there exists line size and cache size / assoc globals in sim.cpp
void cache_install(Cache *c, Addr lineaddr, uns is_write, uns core_id){

  // Your Code Goes Here

  // 16-1 = 15 = 1111 which is the mask we want.
  uint32_t index = (lineaddr >> log2_int(CACHE_LINESIZE)) & (c->num_ways-1); 
  assert(index < c->num_ways);

  uint32_t tag = (lineaddr >> (log2_int(CACHE_LINESIZE) + log2_int(c->num_ways)) );

  // Find victim using cache_find_victim
  uint32_t lru = find_lru(c, index);
  assert(lru < c->num_sets);

  // we are evicting it, so check if it is dirty
  if ( c->sets[lru].line[index].dirty ) {
    c->stat_dirty_evicts++;
  }

  // Initialize the evicted entry
  c->sets[lru].line[index].valid = 1;
  c->sets[lru].line[index].dirty = 0;
  c->sets[lru].line[index].tag = tag;
  c->sets[lru].line[index].core_id = 0;
  c->sets[lru].line[index].last_access_time = cycle; // defined at top of file for this reason

  // Initialize the victime entry
  // what do here?
}

////////////////////////////////////////////////////////////////////
// You may find it useful to split victim selection from install
////////////////////////////////////////////////////////////////////


uns cache_find_victim(Cache *c, uns set_index, uns core_id){
  uns victim=0;

  // TODO: Write this using a switch case statement
  
  return victim;
}















