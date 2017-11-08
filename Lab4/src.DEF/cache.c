#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "cache.h"


extern uns64 SWP_CORE0_WAYS; // Input Way partitions for Core 0       
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

/*
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
*/

// there exists line size and cache size / assoc globals in sim.cpp
Flag cache_access(Cache *c, Addr lineaddr, uns is_write, uns core_id){
  // Your Code Goes Here
  assert(c != NULL);

  // get the log2 of the linesize, then we shift the addr over by that many and get the index
  // then and with num_sets - 1, which should give us all the bits representing the index.
  // uint32_t index = (lineaddr >> log2_int(CACHE_LINESIZE)) & (c->num_sets-1);
  uint32_t index = lineaddr % c->num_sets;
  assert(index < c->num_sets);

  //uint32_t tag = (lineaddr >> (log2_int(CACHE_LINESIZE) + log2_int(c->num_sets)));
  uint32_t tag = lineaddr / c->num_sets;
  // printf("%x %x %x %u %u\n", lineaddr, index, tag, log2_int(CACHE_LINESIZE), log2_int(c->num_sets));

  if (is_write) {
    c->stat_write_access++;
  }
  else {
    c->stat_read_access++;
  }

  uint32_t i;
  for(i=0; i<c->num_ways; i++)
  {
    // not sure whether tag is the address or just tag bits of address.
    if(c->sets[index].line[i].valid && (c->sets[index].line[i].tag == tag)) // == lineaddr
    {
      if (is_write) {
        c->sets[index].line[i].dirty = 1;
      }
      c->sets[index].line[i].last_access_time = cycle;
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

/*
// there exists line size and cache size / assoc globals in sim.cpp
uint32_t find_lru(Cache *c, uint32_t index)
{
  int lru = -1;
  uint64_t oldest;

  int i;
  for(i=0; i<c->num_ways; i++)
  {
    if(!c->sets[index].line[i].valid)
    {
      return i;
    }
    else if(lru == -1 || (c->sets[index].line[i].last_access_time < oldest))
    {
      lru = i;
      oldest = c->sets[index].line[i].last_access_time;
    }
  }
  return lru;
}
*/

// there exists line size and cache size / assoc globals in sim.cpp
void cache_install(Cache *c, Addr lineaddr, uns is_write, uns core_id){

  // Your Code Goes Here

  // 16-1 = 15 = 1111 which is the mask we want.
  // uint32_t index = (lineaddr >> log2_int(CACHE_LINESIZE)) & (c->num_sets-1); 
  uint32_t index = lineaddr % c->num_sets;
  assert(index < c->num_sets);

  // uint32_t tag = (lineaddr >> (log2_int(CACHE_LINESIZE) + log2_int(c->num_sets)));
  uint32_t tag = lineaddr / c->num_sets;

  // Find victim using cache_find_victim
  uint32_t victim = cache_find_victim(c, index, core_id);
  assert(victim < c->num_ways);

  // we are evicting it, so check if it is dirty
  // Initialize the evicted entry
  if ( c->sets[index].line[victim].dirty ) {
    c->stat_dirty_evicts++;
  }
  c->last_evicted_line = c->sets[index].line[victim];

  // Initialize the victime entry
  c->sets[index].line[victim].valid = 1;
  c->sets[index].line[victim].dirty = is_write;
  c->sets[index].line[victim].tag = tag;
  c->sets[index].line[victim].core_id = core_id;
  c->sets[index].line[victim].last_access_time = cycle; // defined at top of file for this reason
}

////////////////////////////////////////////////////////////////////
// You may find it useful to split victim selection from install
////////////////////////////////////////////////////////////////////

// is there anything special about this for multiple cores?
uns cache_find_victim(Cache *c, uns set_index, uns core_id){
  uns victim=0;

  if (c->repl_policy == 0) {
    int lru = -1;
    uint64_t oldest;

    int i;
    for(i=0; i<c->num_ways; i++)
    {
      if(!c->sets[set_index].line[i].valid)
      {
        return i;
      }
      else if(lru == -1 || (c->sets[set_index].line[i].last_access_time < oldest))
      {
        lru = i;
        oldest = c->sets[set_index].line[i].last_access_time;
      }
    }

    victim = lru;
  }
  else if(c->repl_policy == 1) {
    victim = rand() % c->num_ways;
  }

  else if(c->repl_policy == 2) {
    int lru = -1;
    uint64_t oldest;

    // hopefully it is as easy as this. 
    int start, end;
    if (core_id == 0) {
      start = 0;
      end = SWP_CORE0_WAYS;
    }
    else {
      start = SWP_CORE0_WAYS;
      end = c->num_ways;
    }
    
    int i;
    for(i=start; i<end; i++)
    {
      if(!c->sets[set_index].line[i].valid)
      {
        return i;
      }
      else if(lru == -1 || (c->sets[set_index].line[i].last_access_time < oldest))
      {
        lru = i;
        oldest = c->sets[set_index].line[i].last_access_time;
      }
    }

    victim = lru;
  }
  else {
    assert(0);
    fprintf(stderr, "got policy did not expect\n");
  }

  return victim;
}

