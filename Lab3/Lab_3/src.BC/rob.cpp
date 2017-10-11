#include <stdio.h>
#include <assert.h>

#include "rob.h"


extern int32_t NUM_ROB_ENTRIES;

static int rob_count;

/////////////////////////////////////////////////////////////
// Init function initializes the ROB
/////////////////////////////////////////////////////////////

ROB* ROB_init(void){
  int ii;
  ROB *t = (ROB *) calloc (1, sizeof (ROB));
  for(ii=0; ii<MAX_ROB_ENTRIES; ii++){
    t->ROB_Entries[ii].valid=false;
    t->ROB_Entries[ii].ready=false;
  }
  t->head_ptr=0;
  t->tail_ptr=0;
  return t;
}

/////////////////////////////////////////////////////////////
// Print State
/////////////////////////////////////////////////////////////
void ROB_print_state(ROB *t){
 int ii = 0;
  printf("Printing ROB \n");
  printf("Entry  Inst   Valid   ready\n");
  for(ii = 0; ii < 7; ii++) {
    printf("%5d ::  %d\t", ii, (int)t->ROB_Entries[ii].inst.inst_num);
    printf(" %5d\t", t->ROB_Entries[ii].valid);
    printf(" %5d\n", t->ROB_Entries[ii].ready);
  }
  printf("\n");
}

/////////////////////////////////////////////////////////////
//------- DO NOT CHANGE THE CODE ABOVE THIS LINE -----------
/////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////
// If there is space in ROB return true, else false
/////////////////////////////////////////////////////////////

bool ROB_check_space(ROB *t){
  // just going to maintain this var for now.
  // cant think of a better way to do this when they are all in same file
  return rob_count < MAX_ROB_ENTRIES;
}

/////////////////////////////////////////////////////////////
// insert entry at tail, increment tail (do check_space first)
/////////////////////////////////////////////////////////////

int ROB_insert(ROB *t, Inst_Info inst){
  assert( inst.dr_tag == -1);
  assert( ROB_check_space(t) );
  assert( !t->ROB_Entries[t->tail_ptr].valid );
  assert( !t->ROB_Entries[t->tail_ptr].ready );

  rob_count ++;

  t->ROB_Entries[t->tail_ptr].inst = inst;

  // we are naming this here.
  t->ROB_Entries[t->tail_ptr].inst.dr_tag = t->tail_ptr;
  t->ROB_Entries[t->tail_ptr].valid = true;

  int old_tail = t->tail_ptr;
  t->tail_ptr = t->tail_ptr+1 == NUM_ROB_ENTRIES ? 0 : t->tail_ptr+1;

  return old_tail;
}

/////////////////////////////////////////////////////////////
// Once an instruction finishes execution, mark rob entry as done
/////////////////////////////////////////////////////////////

void ROB_mark_ready(ROB *t, Inst_Info inst){
  assert( inst.dr_tag != -1);
  assert( !t->ROB_Entries[inst.dr_tag].ready );
  t->ROB_Entries[inst.dr_tag].ready = true;
}

/////////////////////////////////////////////////////////////
// Find whether the prf-rob entry is ready
/////////////////////////////////////////////////////////////

bool ROB_check_ready(ROB *t, int tag){
  assert( t->ROB_Entries[tag].valid );
  return t->ROB_Entries[tag].ready;
}


/////////////////////////////////////////////////////////////
// Check if the oldest ROB entry is ready for commit
/////////////////////////////////////////////////////////////

bool ROB_check_head(ROB *t){
  assert( t->ROB_Entries[t->head_ptr].valid );
  return ROB_check_ready( t, t->head_ptr ); 
}

/////////////////////////////////////////////////////////////
// Remove oldest entry from ROB (after ROB_check_head)
/////////////////////////////////////////////////////////////

Inst_Info ROB_remove_head(ROB *t){
  assert( t->ROB_Entries[t->head_ptr].valid );

  rob_count --;

  Inst_Info head = t->ROB_Entries[t->head_ptr].inst;
  t->head_ptr = t->head_ptr+1 == NUM_ROB_ENTRIES ? 0 : t->head_ptr+1;
  return head;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////
