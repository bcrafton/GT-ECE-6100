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

typedef struct register_map{
  uint8_t inflight[32];
  //uint8_t inflight_count[32];
  Latch_Type stage[32];
  uint8_t pipe[32];
} register_map_t;

static register_map_t reg_map;

// could do this for wb, mem, ex, and id
// or could just do it once.
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

/*
void allocate_reg(Pipeline *p, )
{
  int i;
  for(i=0; i<PIPE_WIDTH; i++)
  {
    uint8_t dest_needed = p->pipe_latch[ID_LATCH][i].tr_entry.dest_needed;
    uint8_t dest =        p->pipe_latch[ID_LATCH][i].tr_entry.dest;
    uint8_t stall =       p->pipe_latch[ID_LATCH][ii].stall;

    if(!stall && dest_needed){
      reg_map.stage[dest] = ID_LATCH;
      reg_map.pipe[dest] = i;
    }
  }
}
*/

/*
bool reg_avail(Pipeline *p, uint8_t reg)
{
  if (!reg_map.inflight[reg])
  {
    return true;
  }
  
  Latch_Type stage = reg_map.stage[reg];
  uint8_t pipe = reg_map.pipe[reg];
  
  bool forward_ex = !p->pipe_latch[stage][pipe].tr_entry.cc_read && stage = EX_LATCH;
  bool forward_mem = stage >= MEM_LATCH;

  return forward_ex || forward_mem;

}
*/

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

static uint32_t stalls;

void pipe_cycle_ID(Pipeline *p){

  int ii;
  int j;

  bool src1_hazard;
  bool src2_hazard;

  uint8_t src1;
  uint8_t src1_needed;

  uint8_t src2;
  uint8_t src2_needed;

  uint8_t valid;

  uint8_t dest;
  uint8_t dest_needed;


  update_regmap();

  for(ii=0; ii<PIPE_WIDTH; ii++){
        
    if(!p->pipe_latch[ID_LATCH][ii].stall) {
      p->pipe_latch[ID_LATCH][ii]=p->pipe_latch[FE_LATCH][ii];

      // not sure if the right way to do this. how else to communicate we need to stall next cycle.
      // could say op_id of when we stalled.
      p->pipe_latch[FE_LATCH][ii].valid = 0;
    }
    src1 = p->pipe_latch[ID_LATCH][ii].tr_entry.src1_reg;
    src1_needed = p->pipe_latch[ID_LATCH][ii].tr_entry.src1_needed;

    src2 = p->pipe_latch[ID_LATCH][ii].tr_entry.src2_reg;
    src2_needed = p->pipe_latch[ID_LATCH][ii].tr_entry.src2_needed;

    //valid = p->pipe_latch[ID_LATCH][ii].valid;

    //dest = p->pipe_latch[ID_LATCH][ii].tr_entry.dest;
    //dest_needed = p->pipe_latch[ID_LATCH][ii].tr_entry.dest_needed;

    src1_hazard = reg_map.inflight[src1] && src1_needed;
    src2_hazard = reg_map.inflight[src2] && src2_needed;

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

    p->pipe_latch[ID_LATCH][ii].stall = src1_hazard || src2_hazard;

    if (p->pipe_latch[ID_LATCH][ii].valid && 
        p->pipe_latch[ID_LATCH][ii].tr_entry.dest_needed &&
        !p->pipe_latch[ID_LATCH][ii].stall) {

      reg_map.inflight[ p->pipe_latch[ID_LATCH][ii].tr_entry.dest ] = true;

      reg_map.stage[ p->pipe_latch[ID_LATCH][ii].tr_entry.dest ] = ID_LATCH;
      reg_map.pipe[ p->pipe_latch[ID_LATCH][ii].tr_entry.dest ] = ii;
    }

    //printf("%d %d\n", ENABLE_EXE_FWD, ENABLE_MEM_FWD);


  }
}

//--------------------------------------------------------------------//

void pipe_cycle_FE(Pipeline *p){
  int ii;
  Pipeline_Latch fetch_op;
  bool tr_read_success;

  Pipeline_Latch id;

  for(ii=0; ii<PIPE_WIDTH; ii++){

    // we should only pull an instruction if previous one was flopped forward
    // maybe we do need a better way to do this
    // not sure if valid NEEDs to be used for something else.

    //printf("valid = %d\n", p->pipe_latch[FE_LATCH][ii].valid);

    if (!p->pipe_latch[FE_LATCH][ii].valid) {
      
      id = p->pipe_latch[FE_LATCH][ii];

      pipe_get_fetch_op(p, &fetch_op);

/*
      if (id.tr_entry.dest_needed && 
          id.tr_entry.dest == fetch_op.tr_entry.src1_reg &&
          fetch_op.tr_entry.src1_needed &&
          id.tr_entry.mem_read){
            //printf("it happened!!!\n");
          stalls++;
          printf("%d\n", stalls);
      }
      else if (id.tr_entry.dest_needed && 
          id.tr_entry.dest == fetch_op.tr_entry.src2_reg &&
          fetch_op.tr_entry.src2_needed &&
          id.tr_entry.mem_read){
            //printf("it happened!!!\n");
          stalls++;
          printf("%d\n", stalls);
      }
*/

/*
      if (id.tr_entry.dest_needed && 
          id.tr_entry.dest == fetch_op.tr_entry.src1_reg &&
          fetch_op.tr_entry.src1_needed){
            //printf("it happened!!!\n");
          stalls+=2;
          printf("%d\n", stalls);
      }
      else if (id.tr_entry.dest_needed && 
          id.tr_entry.dest == fetch_op.tr_entry.src2_reg &&
          fetch_op.tr_entry.src2_needed){
            //printf("it happened!!!\n");
          stalls+=2;
          printf("%d\n", stalls);
      }
*/


/*
      if (id.tr_entry.mem_read)
      {
            printf("src %d needed %d src %d needed %d dest %d needed %d type %d \n", 
            fetch_op.tr_entry.src1_reg,
            fetch_op.tr_entry.src1_needed,
            fetch_op.tr_entry.src2_reg,
            fetch_op.tr_entry.src2_needed,
            id.tr_entry.dest,
            id.tr_entry.dest_needed,
            fetch_op.tr_entry.op_type
            );
      }
*/    

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

