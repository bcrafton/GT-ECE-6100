#include <stdio.h>
#include <assert.h>

#include "rest.h"

extern int32_t NUM_REST_ENTRIES;

// do we really need separate vars for rat, rt, rob
static int rest_count = 0;
// static int rd = 0;
// static int wr = 0;

/////////////////////////////////////////////////////////////
// Init function initializes the Reservation Station
/////////////////////////////////////////////////////////////

REST* REST_init(void){
  int ii;
  REST *t = (REST *) calloc (1, sizeof (REST));
  for(ii=0; ii<MAX_REST_ENTRIES; ii++){
    t->REST_Entries[ii].valid=false;
  }
  assert(NUM_REST_ENTRIES<=MAX_REST_ENTRIES);
  return t;
}

////////////////////////////////////////////////////////////
// Print State
/////////////////////////////////////////////////////////////
void REST_print_state(REST *t){
 int ii = 0;
  printf("Printing REST \n");
  printf("Entry  Inst Num  S1_tag S1_ready S2_tag S2_ready  Vld Scheduled\n");
  for(ii = 0; ii < NUM_REST_ENTRIES; ii++) {
    printf("%5d ::  \t\t%d\t", ii, (int)t->REST_Entries[ii].inst.inst_num);
    printf("%5d\t\t", t->REST_Entries[ii].inst.src1_tag);
    printf("%5d\t\t", t->REST_Entries[ii].inst.src1_ready);
    printf("%5d\t\t", t->REST_Entries[ii].inst.src2_tag);
    printf("%5d\t\t", t->REST_Entries[ii].inst.src2_ready);
    printf("%5d\t\t", t->REST_Entries[ii].valid);
    printf("%5d\n", t->REST_Entries[ii].scheduled);
    }
  printf("\n");
}

/////////////////////////////////////////////////////////////
//------- DO NOT CHANGE THE CODE ABOVE THIS LINE -----------
/////////////////////////////////////////////////////////////


/////////////////////////////////////////////////////////////
// If space return true else return false
/////////////////////////////////////////////////////////////

bool  REST_check_space(REST *t){

  // we are double doing this
  // need to compile separately.
  return rest_count < MAX_REST_ENTRIES;

/*
  int i;
  for(i=0; i<MAX_REST_ENTRIES; i++) {
    if (t->REST_Entries[i].valid) {
      return true;
    }
  }
  return false;
*/

}

/////////////////////////////////////////////////////////////
// Insert an inst in REST, must do check_space first
/////////////////////////////////////////////////////////////

void  REST_insert(REST *t, Inst_Info inst){

  REST_print_state(t);

  assert( REST_check_space(t) );

  // putting in assertions to maintain invariants.
  assert( inst.dr_tag != -1);
  assert( !t->REST_Entries[inst.dr_tag].scheduled );
  
  if (t->REST_Entries[inst.dr_tag].valid) {
    // int i; for(i=0;i<NUM_REST_ENTRIES;i++){printf("tag: %d valid %d\n", i, t->REST_Entries[i].valid);}
    fprintf(stderr, "tag: %d valid %d\n", inst.dr_tag, t->REST_Entries[inst.dr_tag].valid);
    assert( !t->REST_Entries[inst.dr_tag].valid );
  }

  t->REST_Entries[inst.dr_tag].inst = inst;
  t->REST_Entries[inst.dr_tag].valid = true;
  rest_count++;
}

/////////////////////////////////////////////////////////////
// When instruction finishes execution, remove from REST
/////////////////////////////////////////////////////////////

void  REST_remove(REST *t, Inst_Info inst){
  // assert it is valid before removing
  assert( inst.dr_tag != -1);
  assert( t->REST_Entries[inst.dr_tag].valid );
  t->REST_Entries[inst.dr_tag].valid = false;
  t->REST_Entries[inst.dr_tag].scheduled = false;
  rest_count--;
}

/////////////////////////////////////////////////////////////
// For broadcast of freshly ready tags, wakeup waiting inst
/////////////////////////////////////////////////////////////

void  REST_wakeup(REST *t, int tag){
  int i;
  for(i=0; i<MAX_REST_ENTRIES; i++) {
    if(t->REST_Entries[i].valid) {
      if (t->REST_Entries[i].inst.src1_tag != -1 && t->REST_Entries[i].inst.src1_tag == tag) {
        t->REST_Entries[i].inst.src1_tag = -1;
      }
      if (t->REST_Entries[i].inst.src2_tag != -1 && t->REST_Entries[i].inst.src2_tag == tag) {
        t->REST_Entries[i].inst.src2_tag = -1;
      }
    }
  }
}

/////////////////////////////////////////////////////////////
// When an instruction gets scheduled, mark REST entry as such
/////////////////////////////////////////////////////////////

void  REST_schedule(REST *t, Inst_Info inst){
  assert( inst.dr_tag != -1);
  assert( t->REST_Entries[inst.dr_tag].valid );
  assert( !t->REST_Entries[inst.dr_tag].scheduled );
  t->REST_Entries[inst.dr_tag].scheduled = true;
}
















