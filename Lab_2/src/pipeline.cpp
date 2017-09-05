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

Pipeline_Latch mem[8];
Pipeline_Latch ex[8];
Pipeline_Latch id[8];
Pipeline_Latch zero_pipe;

bool stalls[8];

bool hazard(int pipe_entry)
{
  if (ex[0].tr_entry.dest_needed && 
      ex[0].tr_entry.dest == id[0].tr_entry.src1_reg &&
      id[0].tr_entry.src1_needed &&
      ex[0].tr_entry.mem_read) {
        return true; //p->stat_num_cycle+=1;
  }
  else if (ex[0].tr_entry.dest_needed && 
      ex[0].tr_entry.dest == id[0].tr_entry.src2_reg &&
      id[0].tr_entry.src2_needed &&
      ex[0].tr_entry.mem_read) {
        return true; //p->stat_num_cycle+=1;
  }
  else {
    return false;  
  }
}

void set_false()
{
  int i;
  for(i=0; i<PIPE_WIDTH; i++)
  {
    stalls[i] = false;
  }
}

void pipe_cycle(Pipeline *p)
{

  bool stall = false;
  set_false();

  p->stat_num_cycle++;

  int i;
  for(i=0; i<PIPE_WIDTH; i++)
  {
    if(id[i].op_id >= p->halt_op_id){
      p->halt=true;
    }

    mem[i] = ex[i]; // set mem
    ex[i] = id[i]; // set ex

    // these are only done in first loop
    pipe_get_fetch_op(p, &id[i]); // set id
    p->stat_retired_inst++;
  }

  for(i=0; i<PIPE_WIDTH; i++)
  {
    if (i == 0) {
      stalls[i] = hazard(i);
    }
    else{
      stalls[i] = stalls[i-1] || hazard(i);
    }

    stall = stall || stalls[i];
  }

  while(stall){

    stall = false;

    p->stat_num_cycle++;

    for(i=0; i<PIPE_WIDTH; i++)
    {
      mem[i] = ex[i]; // set mem
      if(!stalls[i]) {
        ex[i] = id[i]; // set ex
        id[i] = zero_pipe;
      }
      else {
        ex[i] = zero_pipe;      
      }
    }

    set_false();

    for(i=0; i<PIPE_WIDTH; i++)
    {
      if (i == 0) {
        stalls[i] = hazard(i);
      }
      else{
        stalls[i] = stalls[i-1] || hazard(i);
      }

      stall = stall || stalls[i];
    }
  }
}

/*

void pipe_cycle(Pipeline *p)
{
    p->stat_num_cycle++;

    pipe_cycle_WB(p);
    pipe_cycle_MEM(p);
    pipe_cycle_EX(p);
    pipe_cycle_ID(p);
    pipe_cycle_FE(p);
	    
}

*/

/**********************************************************************
 * -----------  DO NOT MODIFY THE CODE ABOVE THIS LINE ----------------
 **********************************************************************/

typedef struct register_map{
  uint8_t inflight[32];
  //uint8_t inflight_count[32];
  Latch_Type stage[32];
  uint8_t pipe[32];
} register_map_t;

static register_map_t reg_map;

void update_regmap()
{
  int i;
  for(i=0; i<32; i++){
    if(reg_map.inflight[i]){

        switch (reg_map.stage[i]) {
        
          case FE_LATCH:
            // do nothing
            break;
          case ID_LATCH:
            reg_map.stage[i] = EX_LATCH;
            break;
          case EX_LATCH:
            reg_map.stage[i] = MEM_LATCH;
            break;
          case MEM_LATCH:
            reg_map.stage[i] = FE_LATCH;
            reg_map.inflight[i] = 0;
            break;
          default:
            fprintf(stderr, "Error: Should not get anything besides EX/MEM here. Got: %d\n", reg_map.stage[i]);
            assert(0);
      }
    }
  }
}

void pipe_cycle_WB(Pipeline *p){
  int ii;  

  for(ii=0; ii<PIPE_WIDTH; ii++){

    if(p->pipe_latch[MEM_LATCH][ii].valid){

      p->stat_retired_inst++;
      //printf("retired instructions: %lu\n", p->stat_retired_inst);

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

void pipe_cycle_ID(Pipeline *p){

  int ii;
  int j;

  bool src1_hazard;
  bool src2_hazard;

  uint8_t src1;
  uint8_t src1_needed;

  uint8_t src2;
  uint8_t src2_needed;

  update_regmap();

  for(ii=0; ii<PIPE_WIDTH; ii++){
        
    if(!p->pipe_latch[ID_LATCH][ii].stall) {
      p->pipe_latch[ID_LATCH][ii]=p->pipe_latch[FE_LATCH][ii];
      p->pipe_latch[FE_LATCH][ii].valid = 0;
    }

    src1 = p->pipe_latch[ID_LATCH][ii].tr_entry.src1_reg;
    src1_needed = p->pipe_latch[ID_LATCH][ii].tr_entry.src1_needed;

    src2 = p->pipe_latch[ID_LATCH][ii].tr_entry.src2_reg;
    src2_needed = p->pipe_latch[ID_LATCH][ii].tr_entry.src2_needed;

    src1_hazard = reg_map.inflight[src1] && src1_needed;
    src2_hazard = reg_map.inflight[src2] && src2_needed;

    bool forward_ex_src1 = false;
    bool forward_ex_src2 = false;
    bool forward_mem_src1 = false;
    bool forward_mem_src2 = false;

    // can definitely run into a problem if a load is in ex, and a value that shud not be forwarded is in mem.
    if(ENABLE_EXE_FWD){
      if (src1_hazard) {
        Latch_Type stage = reg_map.stage[src1];
        uint8_t pipe = reg_map.pipe[src1];
        bool forward_ex = !p->pipe_latch[stage][pipe].tr_entry.mem_read && (stage == EX_LATCH);
        
        if (forward_ex) src1_hazard = false; 
      }

      if (src2_hazard) {
        Latch_Type stage = reg_map.stage[src2];
        uint8_t pipe = reg_map.pipe[src2];
        bool forward_ex = !p->pipe_latch[stage][pipe].tr_entry.mem_read && (stage == EX_LATCH);

        if (forward_ex) src2_hazard = false;                
      }
    }

    if(ENABLE_MEM_FWD){
      if (src1_hazard) {
        Latch_Type stage = reg_map.stage[src1];
        bool forward_mem = stage == MEM_LATCH;

        if (forward_mem) src1_hazard = false;           
      }

      if (src2_hazard) {
        Latch_Type stage = reg_map.stage[src2];
        bool forward_mem = stage == MEM_LATCH;

        if (forward_mem) src2_hazard = false;             
      }
    }

    if (ii==0) {
      p->pipe_latch[ID_LATCH][ii].stall = src1_hazard || src2_hazard;
    } 
    else {
      p->pipe_latch[ID_LATCH][ii].stall = p->pipe_latch[ID_LATCH][ii-1].stall || src1_hazard || src2_hazard;
    }


    if (p->pipe_latch[ID_LATCH][ii].tr_entry.dest_needed &&
        !p->pipe_latch[ID_LATCH][ii].stall) {

      reg_map.inflight[ p->pipe_latch[ID_LATCH][ii].tr_entry.dest ] = 1;

      reg_map.stage[ p->pipe_latch[ID_LATCH][ii].tr_entry.dest ] = ID_LATCH;
      reg_map.pipe[ p->pipe_latch[ID_LATCH][ii].tr_entry.dest ] = ii;
    }

  }
}

//--------------------------------------------------------------------//

void pipe_cycle_FE(Pipeline *p){
  int ii;
  Pipeline_Latch fetch_op;
  bool tr_read_success;

  for(ii=0; ii<PIPE_WIDTH; ii++){

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

