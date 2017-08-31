/***********************************************************************
 * File         : pipeline.cpp
 * Author       : Soham J. Desai 
 * Date         : 14th January 2014
 * Description  : Superscalar Pipeline for Lab2 ECE 6100
 **********************************************************************/

#include "pipeline.h"
#include <cstdlib>

extern int32_t PIPE_WIDTH;
extern int32_t ENABLE_MEM_FWD;
extern int32_t ENABLE_EXE_FWD;
extern int32_t BPRED_POLICY;

/**********************************************************************
 * Support Function: Read 1 Trace Record From File and populate Fetch Op
 **********************************************************************/

void pipe_get_fetch_op(Pipeline *p, Pipeline_Latch* fetch_op){
    uint8_t bytes_read = 0;
    bytes_read = fread(&fetch_op->tr_entry, 1, sizeof(Trace_Rec), p->tr_file);

    // check for end of trace
    if( bytes_read < sizeof(Trace_Rec)) {
      fetch_op->valid=false;
      p->halt_op_id=p->op_id_tracker;
      return;
    }

    // got an instruction ... hooray!
    fetch_op->valid=true;
    fetch_op->stall=false;
    fetch_op->is_mispred_cbr=false;
    p->op_id_tracker++;
    fetch_op->op_id=p->op_id_tracker;
    
    return; 
}


/**********************************************************************
 * Pipeline Class Member Functions 
 **********************************************************************/

Pipeline * pipe_init(FILE *tr_file_in){
    printf("\n** PIPELINE IS %d WIDE **\n\n", PIPE_WIDTH);

    // Initialize Pipeline Internals
    Pipeline *p = (Pipeline *) calloc (1, sizeof (Pipeline));

    p->tr_file = tr_file_in;
    p->halt_op_id = ((uint64_t)-1) - 3;           

    // Allocated Branch Predictor
    if(BPRED_POLICY){
      p->b_pred = new BPRED(BPRED_POLICY);
    }

    return p;
}


/**********************************************************************
 * Print the pipeline state (useful for debugging)
 **********************************************************************/

void pipe_print_state(Pipeline *p){
    std::cout << "--------------------------------------------" << std::endl;
    std::cout <<"cycle count : " << p->stat_num_cycle << " retired_instruction : " << p->stat_retired_inst << std::endl;

    uint8_t latch_type_i = 0;   // Iterates over Latch Types
    uint8_t width_i      = 0;   // Iterates over Pipeline Width
    for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
        switch(latch_type_i) {
            case 0:
                printf(" FE: ");
                break;
            case 1:
                printf(" ID: ");
                break;
            case 2:
                printf(" EX: ");
                break;
            case 3:
                printf(" MEM: ");
                break;
            default:
                printf(" ---- ");
        }
    }
    printf("\n");
    for(width_i = 0; width_i < PIPE_WIDTH; width_i++) {
        for(latch_type_i = 0; latch_type_i < NUM_LATCH_TYPES; latch_type_i++) {
            if(p->pipe_latch[latch_type_i][width_i].valid == true) {
	      printf(" %6u ",(uint32_t)( p->pipe_latch[latch_type_i][width_i].op_id));
            } else {
                printf(" ------ ");
            }
        }
        printf("\n");
    }
    printf("\n");

}


/**********************************************************************
 * Pipeline Main Function: Every cycle, cycle the stage 
 **********************************************************************/

void pipe_cycle(Pipeline *p)
{
    p->stat_num_cycle++;

    pipe_cycle_WB(p);
    pipe_cycle_MEM(p);
    pipe_cycle_EX(p);
    pipe_cycle_ID(p);
    pipe_cycle_FE(p);
	    
}
/**********************************************************************
 * -----------  DO NOT MODIFY THE CODE ABOVE THIS LINE ----------------
 **********************************************************************/

void pipe_cycle_WB(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    if(p->pipe_latch[MEM_LATCH][ii].valid){
      p->stat_retired_inst++;
      if(p->pipe_latch[MEM_LATCH][ii].op_id >= p->halt_op_id){
	      p->halt=true;
      }
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_MEM(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[MEM_LATCH][ii]=p->pipe_latch[EX_LATCH][ii];
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_EX(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){

    // if the prev stage is stalled, whatever here is invalid.
    if(p->pipe_latch[ID_LATCH][ii].stall) {
      p->pipe_latch[EX_LATCH][ii].valid = false;
    }
    else {
      p->pipe_latch[EX_LATCH][ii]=p->pipe_latch[ID_LATCH][ii];
    }

  }
}

//--------------------------------------------------------------------//

static uint32_t stalls;

bool check_data_dependence(Pipeline_Latch* l1, Pipeline_Latch* l2)
{
  bool src1_hazard = (l1->tr_entry.src1_reg == l2->tr_entry.dest) &&
                     l1->tr_entry.src1_needed &&
                     l2->tr_entry.dest_needed &&
                     l1->valid &&
                     l2->valid;

  bool src2_hazard = (l1->tr_entry.src2_reg == l2->tr_entry.dest) &&
                     l1->tr_entry.src2_needed &&
                     l2->tr_entry.dest_needed &&
                     l1->valid &&
                     l2->valid;


  if (src1_hazard || src2_hazard) {

    // this number can not be incremented here because of double stalls.
    //stalls++;

/*
    printf("stall count= %d current: %lx %x %x %x %x %x %x %x stalled by: %lx %x %x %x %x %x %x %x\n", 
      stalls,

      l1->op_id,  
      l1->tr_entry.op_type,
      l1->tr_entry.dest,
      l1->tr_entry.dest_needed,
      l1->tr_entry.src1_reg,
      l1->tr_entry.src1_needed,
      l1->tr_entry.src2_reg,
      l1->tr_entry.src2_needed,

      l2->op_id,  
      l2->tr_entry.op_type,
      l2->tr_entry.dest,
      l2->tr_entry.dest_needed,
      l2->tr_entry.src1_reg,
      l2->tr_entry.src1_needed,
      l2->tr_entry.src2_reg,
      l2->tr_entry.src2_needed
      );
*/

  }


  return src1_hazard || src2_hazard;
  
  //if(l1->tr_entry.op_type == OP_ALU){}
}

// is it fine to to stage by stage and pipe by pipe?
// rather than pipe by pipe and stage by stage.

void pipe_cycle_ID(Pipeline *p){

  int ii;
  int j;

  for(ii=0; ii<PIPE_WIDTH; ii++){
        
    if(!p->pipe_latch[ID_LATCH][ii].stall) {
      p->pipe_latch[ID_LATCH][ii]=p->pipe_latch[FE_LATCH][ii];
      // not sure if the right way to do this. how else to communicate we need to stall next cycle.
      // could say op_id of when we stalled.
      p->pipe_latch[FE_LATCH][ii].valid = 0;
    }

    // this will need to account for instructions ahead, but in other pipes.
    // will use op_id.
    p->pipe_latch[ID_LATCH][ii].stall = false;
    for(j=0; j<PIPE_WIDTH; j++) {
    
      // account for super scalar. 
      // using > so that it cannot be the same pipe, and it cannot be less than because then it wouldnt be dependent.
      // although this pipeline isnt ooo, so maybe op_id is not important. 
      // and could just use the pipeline iteration number

      // we dont want to double count stalls here. and we want to make our numbers equal

      if (p->pipe_latch[ID_LATCH][ii].op_id > p->pipe_latch[ID_LATCH][j].op_id) {
        p->pipe_latch[ID_LATCH][ii].stall |= check_data_dependence( &(p->pipe_latch[ID_LATCH][ii]), &(p->pipe_latch[ID_LATCH][j]) );
      }

      p->pipe_latch[ID_LATCH][ii].stall |= check_data_dependence( &(p->pipe_latch[ID_LATCH][ii]), &(p->pipe_latch[EX_LATCH][j]) );
      p->pipe_latch[ID_LATCH][ii].stall |= check_data_dependence( &(p->pipe_latch[ID_LATCH][ii]), &(p->pipe_latch[MEM_LATCH][j]) );
    }

    if (p->pipe_latch[ID_LATCH][ii].stall) {  
      stalls++;
      //printf("exp %ld curr %ld\n", p->stat_num_cycle, stalls + p->stat_retired_inst);
    }

    if(ENABLE_MEM_FWD){
      // todo
    }

    if(ENABLE_EXE_FWD){
      // todo
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_FE(Pipeline *p){
  int ii;
  Pipeline_Latch fetch_op;
  bool tr_read_success;

  for(ii=0; ii<PIPE_WIDTH; ii++){

    // we should only pull an instruction if previous one was flopped forward
    // maybe we do need a better way to do this
    // not sure if valid NEEDs to be used for something else.

    //printf("valid = %d\n", p->pipe_latch[FE_LATCH][ii].valid);

    if (!p->pipe_latch[FE_LATCH][ii].valid) {
      pipe_get_fetch_op(p, &fetch_op);


      if(BPRED_POLICY){
        pipe_check_bpred(p, &fetch_op);
      }
      
      // copy the op in FE LATCH
      p->pipe_latch[FE_LATCH][ii]=fetch_op;
    }
  }
  
}


//--------------------------------------------------------------------//

void pipe_check_bpred(Pipeline *p, Pipeline_Latch *fetch_op){
  // call branch predictor here, if mispred then mark in fetch_op
  // update the predictor instantly
  // stall fetch using the flag p->fetch_cbr_stall
}


//--------------------------------------------------------------------//

