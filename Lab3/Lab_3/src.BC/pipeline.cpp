/***********************************************************************
 * File         : pipeline.cpp
 * Author       : Moinuddin K. Qureshi
 * Date         : 19th February 2014
 * Description  : Out of Order Pipeline for Lab3 ECE 6100
 **********************************************************************/

#include "pipeline.h"
#include <cstdlib>
#include <cstring>


extern int32_t PIPE_WIDTH;
extern int32_t SCHED_POLICY;
extern int32_t LOAD_EXE_CYCLES;

/**********************************************************************
 * Support Function: Read 1 Trace Record From File and populate Fetch Inst
 **********************************************************************/

void pipe_fetch_inst(Pipeline *p, Pipe_Latch* fe_latch){

    static int halt_fetch = 0;
    uint8_t bytes_read = 0;
    Trace_Rec trace;
    if(halt_fetch != 1) {
      bytes_read = fread(&trace, 1, sizeof(Trace_Rec), p->tr_file);
      Inst_Info *fetch_inst = &(fe_latch->inst);
    // check for end of trace
    // Send out a dummy terminate op
      if( bytes_read < sizeof(Trace_Rec)) {
        p->halt_inst_num=p->inst_num_tracker;
        halt_fetch = 1;
        fe_latch->valid=true;
        fe_latch->inst.dest_reg = -1;
        fe_latch->inst.src1_reg = -1;
        fe_latch->inst.src1_reg = -1;
        fe_latch->inst.inst_num=-1;
        fe_latch->inst.op_type=4;
        return;
      }

    // got an instruction ... hooray!
      fe_latch->valid=true;
      fe_latch->stall=false;
      p->inst_num_tracker++;
      fetch_inst->inst_num=p->inst_num_tracker;
      fetch_inst->op_type=trace.op_type;

      fetch_inst->dest_reg=trace.dest_needed? trace.dest:-1;
      fetch_inst->src1_reg=trace.src1_needed? trace.src1_reg:-1;
      fetch_inst->src2_reg=trace.src2_needed? trace.src2_reg:-1;

      fetch_inst->dr_tag=-1;
      fetch_inst->src1_tag=-1;
      fetch_inst->src2_tag=-1;
      fetch_inst->src1_ready=false;
      fetch_inst->src2_ready=false;
      fetch_inst->exe_wait_cycles=0;
    } else {
      fe_latch->valid = false;
    }
    return; 
}


/**********************************************************************
 * Pipeline Class Member Functions 
 **********************************************************************/

Pipeline * pipe_init(FILE *tr_file_in){
    printf("\n** PIPELINE IS %d WIDE **\n\n", PIPE_WIDTH);

    // Initialize Pipeline Internals
    Pipeline *p = (Pipeline *) calloc (1, sizeof (Pipeline));
    
    p->pipe_RAT=RAT_init();
    p->pipe_ROB=ROB_init();
    p->pipe_REST=REST_init();
    p->pipe_EXEQ=EXEQ_init();
    p->tr_file = tr_file_in;
    p->halt_inst_num = ((uint64_t)-1) - 3;           
    int ii =0;
    for(ii = 0; ii < PIPE_WIDTH; ii++) {  // Loop over No of Pipes
      p->FE_latch[ii].valid = false;
      p->ID_latch[ii].valid = false;
      p->EX_latch[ii].valid = false;
      p->SC_latch[ii].valid = false;
    } 
    return p;
}


/**********************************************************************
 * Print the pipeline state (useful for debugging)
 **********************************************************************/

void pipe_print_state(Pipeline *p){
    std::cout << "--------------------------------------------" << std::endl;
    std::cout <<"cycle count : " << p->stat_num_cycle << " retired_instruction : " << p->stat_retired_inst << std::endl;
    uint8_t latch_type_i = 0;
    uint8_t width_i      = 0;
   for(latch_type_i = 0; latch_type_i < 4; latch_type_i++) {
        switch(latch_type_i) {
        case 0:
            printf(" FE: ");
            break;
        case 1:
            printf(" ID: ");
            break;
        case 2:
            printf(" SCH: ");
            break;
        case 3:
            printf(" EX: ");
            break;
        default:
            printf(" -- ");
          }
    }
   printf("\n");
   for(width_i = 0; width_i < PIPE_WIDTH; width_i++) {
       if(p->FE_latch[width_i].valid == true) {
         printf("  %d  ", (int)p->FE_latch[width_i].inst.inst_num);
       } else {
         printf(" --  ");
       }
       if(p->ID_latch[width_i].valid == true) {
         printf("  %d  ", (int)p->ID_latch[width_i].inst.inst_num);
       } else {
         printf(" --  ");
       }
       if(p->SC_latch[width_i].valid == true) {
         printf("  %d  ", (int)p->SC_latch[width_i].inst.inst_num);
       } else {
         printf(" --  ");
       }
       if(p->EX_latch[width_i].valid == true) {
         for(int ii = 0; ii < MAX_BROADCASTS; ii++) {
            if(p->EX_latch[ii].valid)
	      printf("  %d  ", (int)p->EX_latch[ii].inst.inst_num);
         }  
       } else {
         printf(" --  ");
       }
        printf("\n");
     }
     printf("\n");
      
     RAT_print_state(p->pipe_RAT);
     REST_print_state(p->pipe_REST);
     EXEQ_print_state(p->pipe_EXEQ);
     ROB_print_state(p->pipe_ROB);
}


/**********************************************************************
 * Pipeline Main Function: Every cycle, cycle the stage 
 **********************************************************************/

void pipe_cycle(Pipeline *p)
{
    p->stat_num_cycle++;
  
    pipe_cycle_commit(p);
    pipe_cycle_broadcast(p);
    pipe_cycle_exe(p);
    pipe_cycle_schedule(p);
    pipe_cycle_rename(p);
    pipe_cycle_decode(p);
    pipe_cycle_fetch(p);

    // ROB_print_state(p->pipe_ROB);
    // REST_print_state(p->pipe_REST);
}

//--------------------------------------------------------------------//

void pipe_cycle_fetch(Pipeline *p){
  int ii = 0;
  Pipe_Latch fetch_latch;

  for(ii=0; ii<PIPE_WIDTH; ii++) {
    if((p->FE_latch[ii].stall) || (p->FE_latch[ii].valid)) {   // Stall 
        continue;

    } else {  // No Stall and Latch Empty
        pipe_fetch_inst(p, &fetch_latch);
        // copy the op in FE LATCH
        p->FE_latch[ii]=fetch_latch;
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_decode(Pipeline *p){
  int ii = 0;

  int jj = 0;

  static uint64_t start_inst_id = 1;

  // Loop Over ID Latch
  for(ii=0; ii<PIPE_WIDTH; ii++)
  { 
    if((p->ID_latch[ii].stall == 1) || (p->ID_latch[ii].valid)) // Stall
    { 
      continue;  
    } 
    else // No Stall & there is Space in Latch
    {  
      for(jj = 0; jj < PIPE_WIDTH; jj++) // Loop Over FE Latch
      { 
        if(p->FE_latch[jj].valid) 
        {
          if(p->FE_latch[jj].inst.inst_num == start_inst_id) // In Order Inst Found
          { 
            p->ID_latch[ii]        = p->FE_latch[jj];
            p->ID_latch[ii].valid  = true;
            p->FE_latch[jj].valid  = false;
            start_inst_id++;
            break;
          }
        }
      }
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_exe(Pipeline *p){

  // printf("%lu\n", p->SC_latch[0].inst.inst_num);
  // printf("%lu\n", p->EX_latch[0].inst.inst_num);

  int ii;
  //If all operations are single cycle, simply copy SC latches to EX latches
  if(LOAD_EXE_CYCLES == 1) {
    for(ii=0; ii<PIPE_WIDTH; ii++){
      if(p->SC_latch[ii].valid) {
        p->EX_latch[ii]=p->SC_latch[ii];
        p->EX_latch[ii].valid = true;
        p->SC_latch[ii].valid = false; 
        // printf("%lu\n", p->SC_latch[0].inst.inst_num);
        // printf("%lu\n", p->EX_latch[0].inst.inst_num);
      }
      // supposedly this isnt supposed to be here.
      return;
    }
  }
  
  //---------Handling exe for multicycle operations is complex, and uses EXEQ
  
  // All valid entries from SC get into exeq  
  
  for(ii = 0; ii < PIPE_WIDTH; ii++) {
    if(p->SC_latch[ii].valid) {
      EXEQ_insert(p->pipe_EXEQ, p->SC_latch[ii].inst);
      p->SC_latch[ii].valid = false;
    }
  }
  
  // Cycle the exeq, to reduce wait time for each inst by 1 cycle
  EXEQ_cycle(p->pipe_EXEQ);
  
  // Transfer all finished entries from EXEQ to EX_latch
  int index = 0;
  
  while(1) {
    if(EXEQ_check_done(p->pipe_EXEQ)) {
      p->EX_latch[index].valid = true;
      p->EX_latch[index].stall = false;
      p->EX_latch[index].inst  = EXEQ_remove(p->pipe_EXEQ);
      index++;
    } else { // No More Entry in EXEQ
      break;
    }
  }
}



/**********************************************************************
 * -----------  DO NOT MODIFY THE CODE ABOVE THIS LINE ----------------
 **********************************************************************/

void pipe_cycle_rename(Pipeline *p){
  int i;
  for(i=0; i<PIPE_WIDTH; i++) {

    // these should be equal
    assert( ROB_check_space( p->pipe_ROB ) == REST_check_space( p->pipe_REST ) );

    // get the instruction from id latch.
    // probably need to check for stalling 
    // this is really just if we are full or not.
    if ( p->ID_latch[i].valid && ROB_check_space( p->pipe_ROB ) ) {
      Inst_Info id_inst = p->ID_latch[i].inst;

/*
      printf("cycle #%lu\n", p->stat_num_cycle);
      printf("inst #%lu\n", p->stat_retired_inst);
      printf("%lu %d %d %d %d %d %d\n", 
        id_inst.inst_num, 
        id_inst.src1_reg, 
        id_inst.src2_reg, 
        id_inst.src1_tag, 
        id_inst.src2_tag, 
        id_inst.src1_ready, 
        id_inst.src2_ready
      );
*/

      // todo: If src1/src2 is remapped set src1tag, src2tag
      // check RAT
      // where do we do this operatrion? REST?

      if ( (id_inst.src1_reg != -1) && (RAT_get_remap( p->pipe_RAT, id_inst.src1_reg ) != -1) ) {
        id_inst.src1_tag = RAT_get_remap( p->pipe_RAT, id_inst.src1_reg );
        // printf("remapped to %d\n", RAT_get_remap( p->pipe_RAT, id_inst.src1_reg ));
      }
      else {
        id_inst.src1_tag = -1;
      }
      if ( (id_inst.src2_reg != -1) && (RAT_get_remap( p->pipe_RAT, id_inst.src2_reg ) != -1) ) {
        id_inst.src2_tag = RAT_get_remap( p->pipe_RAT, id_inst.src2_reg );
        // printf("remapped to %d\n", RAT_get_remap( p->pipe_RAT, id_inst.src2_reg ));
      }
      else {
        id_inst.src2_tag = -1;
      }

      // doing these 3 out of order
      // todo: If src1/src2 is not remapped marked as src ready
      // todo: If src1/src2 remapped and the ROB (tag) is ready then mark src ready
      // fixme: If there is stall, we should not do rename and ROB alloc twice
      // doing these 3 out of order

      if(id_inst.src1_tag == -1 || ROB_check_ready(p->pipe_ROB, id_inst.src1_tag) ) {
        id_inst.src1_ready = true;
      }
      if(id_inst.src2_tag == -1 || ROB_check_ready(p->pipe_ROB, id_inst.src2_tag) ) {
        id_inst.src2_ready = true;
      }

      // printf("%d %d\n", ROB_check_space( p->pipe_ROB ), REST_check_space( p->pipe_REST ));
      // we never stall, we just overwrite ?

      // we mark it false if we can place it in the REST
      p->ID_latch[i].valid = false;

      // todo: Find space in ROB and set drtag as such if successful
      if ( ROB_check_space( p->pipe_ROB ) ) {
        int tag = ROB_insert( p->pipe_ROB, id_inst );
        id_inst.dr_tag = tag;
      }

      // todo: Find space in REST and transfer this inst (valid=1, sched=0)
      if ( REST_check_space( p->pipe_REST ) ) {
        REST_insert( p->pipe_REST, id_inst );
      }

      // we place in the instruction in rest and change our rat
      if (id_inst.dest_reg != -1) {
        RAT_set_remap( p->pipe_RAT, id_inst.dest_reg, id_inst.dr_tag );
      }
    }
  }
}

//--------------------------------------------------------------------//

extern int32_t NUM_REST_ENTRIES;

typedef struct oldest {
  Inst_Info inst;
  bool valid;
} oldest_t;

oldest_t oldest(REST* t)
{
  oldest_t o;
  o.valid = false;

  int i;
  for(i=0; i<NUM_REST_ENTRIES; i++)
  {
    if (o.valid == false && t->REST_Entries[i].valid && !t->REST_Entries[i].scheduled) {
      o.valid = true;
      o.inst = t->REST_Entries[i].inst;
    }
    else if (t->REST_Entries[i].valid && (o.inst.inst_num > t->REST_Entries[i].inst.inst_num) && !t->REST_Entries[i].scheduled) {
      o.inst = t->REST_Entries[i].inst;
    }
  }
  return o; 
}

oldest_t oldest_and_ready(REST* t)
{
  oldest_t o;
  o.valid = false;

  int i;
  for(i=0; i<NUM_REST_ENTRIES; i++)
  {
    if (o.valid == false && 
        t->REST_Entries[i].valid && 
        !t->REST_Entries[i].scheduled &&
        t->REST_Entries[i].inst.src1_ready &&
        t->REST_Entries[i].inst.src2_ready) {

      o.valid = true;
      o.inst = t->REST_Entries[i].inst;
    }
    else if (t->REST_Entries[i].valid && 
             (o.inst.inst_num > t->REST_Entries[i].inst.inst_num) && 
             !t->REST_Entries[i].scheduled &&
             t->REST_Entries[i].inst.src1_ready &&
             t->REST_Entries[i].inst.src2_ready) {

      o.inst = t->REST_Entries[i].inst;
    }
  }
  return o;
}

void pipe_cycle_schedule(Pipeline *p){

  // todo: Implement two scheduling policies (SCHED_POLICY: 0 and 1)

  if(SCHED_POLICY==0){
    // inorder scheduling
    // Find all valid entries, if oldest is stalled then stop
    // Else send it out and mark it as scheduled

    int i;
    for(i=0; i<PIPE_WIDTH; i++){
      oldest_t o = oldest(p->pipe_REST);

      if (o.valid) {
        int tag = o.inst.dr_tag;

        if (p->pipe_REST->REST_Entries[tag].inst.src1_ready && p->pipe_REST->REST_Entries[tag].inst.src2_ready ) {
          p->pipe_REST->REST_Entries[tag].scheduled = true;

          p->SC_latch[i].inst = p->pipe_REST->REST_Entries[tag].inst;
          p->SC_latch[i].valid = true;
          p->SC_latch[i].stall = false;
        }
      }
    }
  }

/*
  printf("cycle #%lu\n", p->stat_num_cycle);
  int i;
  for(i=0; i<NUM_REST_ENTRIES; i++)
  {
    printf("%lu %d %d %d %d %d %d\n", 
      p->pipe_REST->REST_Entries[i].inst.inst_num, 
      p->pipe_REST->REST_Entries[i].inst.src1_reg, 
      p->pipe_REST->REST_Entries[i].inst.src2_reg, 
      p->pipe_REST->REST_Entries[i].inst.src1_tag, 
      p->pipe_REST->REST_Entries[i].inst.src2_tag, 
      p->pipe_REST->REST_Entries[i].inst.src1_ready, 
      p->pipe_REST->REST_Entries[i].inst.src2_ready
    );
  }
*/

  if(SCHED_POLICY==1){
    int i;
    for(i=0; i<PIPE_WIDTH; i++){
      oldest_t o = oldest_and_ready(p->pipe_REST);

      if (o.valid) {
        int tag = o.inst.dr_tag;

        // this check unnecessary I believe.
        if (p->pipe_REST->REST_Entries[tag].inst.src1_ready && p->pipe_REST->REST_Entries[tag].inst.src2_ready ) {
          p->pipe_REST->REST_Entries[tag].scheduled = true;

          p->SC_latch[i].inst = p->pipe_REST->REST_Entries[tag].inst;
          p->SC_latch[i].valid = true;
          p->SC_latch[i].stall = false;
        }
      }
    }
  }

}


//--------------------------------------------------------------------//

void pipe_cycle_broadcast(Pipeline *p){

  // todo: Go through all instructions out of EXE latch
  // todo: Broadcast it to REST (using wakeup function)
  // todo: Remove entry from REST (using inst_num)
  // todo: Update the ROB, mark ready, and update Inst Info in ROB
 
  int i;
  for(i=0; i<MAX_BROADCASTS; i++) { // changing this because exeq stage just MAX_BROADCASTS?
    if (p->EX_latch[i].valid) {

      Inst_Info ex_inst = p->EX_latch[i].inst;
      REST_wakeup(p->pipe_REST, ex_inst.dr_tag);
      ROB_mark_ready(p->pipe_ROB, ex_inst);

      p->EX_latch[i].valid = false;
      p->EX_latch[i].stall = false;
    }
  }

}


//--------------------------------------------------------------------//


void pipe_cycle_commit(Pipeline *p) {
  static uint32_t last = 1;

  int ii = 0;

  // todo: check the head of the ROB. If ready commit (update stats)
  // todo: Deallocate entry from ROB
  // todo: Update RAT after checking if the mapping is still valid

  for(ii=0; ii<PIPE_WIDTH; ii++){

    if(p->FE_latch[ii].valid){

      if(p->FE_latch[ii].inst.inst_num >= p->halt_inst_num){
        p->halt=true;
      }

    }

    if ( ROB_check_head(p->pipe_ROB) ) {

      // printf("cycle #%lu\n", p->stat_num_cycle);
      // printf("inst #%lu\n", p->stat_retired_inst);

      p->stat_retired_inst++;
      Inst_Info commit_inst = ROB_remove_head(p->pipe_ROB);
      REST_remove(p->pipe_REST, commit_inst);
      if (p->pipe_RAT->RAT_Entries[commit_inst.dest_reg].prf_id == commit_inst.dr_tag) {
        RAT_reset_entry( p->pipe_RAT, commit_inst.dest_reg );
      }
    }

    if(SCHED_POLICY==0){
      // assert(p->FE_latch[ii].inst.inst_num <= last);
      last++;
    }

  }
}
  
//--------------------------------------------------------------------//




