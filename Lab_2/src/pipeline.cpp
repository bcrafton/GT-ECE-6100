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

void print_instruction(Pipeline_Latch* i)
{
  if (i->valid) {
    printf("%lu | %d %d | %d %d | %d %d | %lx %d %d | %d | %d\n",

    i->op_id,

    i->tr_entry.src1_reg,
    i->tr_entry.src1_needed,

    i->tr_entry.src2_reg,
    i->tr_entry.src2_needed,

    i->tr_entry.dest,
    i->tr_entry.dest_needed,

    i->tr_entry.mem_addr,
    i->tr_entry.mem_write,
    i->tr_entry.mem_read,

    i->tr_entry.op_type,

    i->valid

    );
  }
}

/*

Pipeline_Latch mem[8];
Pipeline_Latch ex[8];
Pipeline_Latch id[8];
Pipeline_Latch zero_pipe;

bool stalls[8];

bool hazard(int pipe)
{

  int i;
  for(i=0; i<PIPE_WIDTH; i++) {

    if (ex[i].tr_entry.dest_needed && 
        ex[i].tr_entry.dest == id[pipe].tr_entry.src1_reg &&
        id[pipe].tr_entry.src1_needed &&
        ex[i].tr_entry.mem_read) {
          return true; //p->stat_num_cycle+=1;
    }
    else if (ex[i].tr_entry.dest_needed && 
        ex[i].tr_entry.dest == id[pipe].tr_entry.src2_reg &&
        id[pipe].tr_entry.src2_needed &&
        ex[i].tr_entry.mem_read) {
          return true; //p->stat_num_cycle+=1;
    }
    else if(i < pipe &&
        id[i].tr_entry.dest_needed && 
        id[i].tr_entry.dest == id[pipe].tr_entry.src1_reg &&
        id[pipe].tr_entry.src1_needed) {
          return true;
    }
    else if(i < pipe &&
        id[i].tr_entry.dest_needed && 
        id[i].tr_entry.dest == id[pipe].tr_entry.src2_reg &&
        id[pipe].tr_entry.src2_needed) {
          return true;
    }
    else {
      return false;  
    }

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
    print_instruction(&id[i]);
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
*/

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



uint64_t stat_optype_dyn[NUM_OP_TYPE] = { 0 };

void pipe_cycle(Pipeline *p)
{
    Pipeline_Latch fetch_op;
    pipe_get_fetch_op(p, &fetch_op); // set id

    p->stat_num_cycle++;

    print_instruction(&fetch_op);

    // count regardless if valid
    p->stat_retired_inst++;

    if (fetch_op.valid) {
      stat_optype_dyn[fetch_op.tr_entry.op_type]++;
    }    

    if(fetch_op.op_id >= p->halt_op_id){
      p->halt=true;
    }   

    int i;
    if (p->halt) {
      for(i=0; i<NUM_OP_TYPE; i++) {
        printf("%d : %lu\n", i, stat_optype_dyn[i]);
      }
    }
}

*/

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

typedef struct cc_map{
  uint8_t inflight;
  Latch_Type stage;
  uint8_t pipe;
} cc_map_t;

static register_map_t reg_map;
static cc_map_t cc_map;

bool check_hazard(Pipeline *p, uint8_t pipe, uint8_t pipe2, Latch_Type stage2)
{
  uint8_t src1 =        p->pipe_latch[ID_LATCH][pipe].tr_entry.src1_reg;
  uint8_t src1_needed = p->pipe_latch[ID_LATCH][pipe].tr_entry.src1_needed;
  uint8_t src2 =        p->pipe_latch[ID_LATCH][pipe].tr_entry.src2_reg;
  uint8_t src2_needed = p->pipe_latch[ID_LATCH][pipe].tr_entry.src2_needed;
  uint8_t cc_read =     p->pipe_latch[ID_LATCH][pipe].tr_entry.cc_read;
  uint8_t valid1 =      p->pipe_latch[ID_LATCH][pipe].valid;

  uint8_t dest =        p->pipe_latch[stage2][pipe2].tr_entry.dest;
  uint8_t dest_needed = p->pipe_latch[stage2][pipe2].tr_entry.dest_needed;
  uint8_t cc_write =    p->pipe_latch[stage2][pipe2].tr_entry.cc_write;
  uint8_t mem_read =    p->pipe_latch[stage2][pipe2].tr_entry.mem_read;
  uint8_t valid2 =      p->pipe_latch[stage2][pipe2].valid;

  if (!valid1 || !valid2) {
    return false;  
  }

  if (src1_needed && dest_needed && src1 == dest) {
    if(ENABLE_EXE_FWD && stage2 == EX_LATCH){
    }
    else if(ENABLE_MEM_FWD && stage2 == MEM_LATCH && !mem_read) {
    }
    else {
      return true;
    }
  }

  if (src2_needed && dest_needed && src2 == dest) {
    if(ENABLE_EXE_FWD && stage2 == EX_LATCH){
    }
    else if(ENABLE_MEM_FWD && stage2 == MEM_LATCH && !mem_read) {
    }
    else {
      return true;
    }
  }

  if (cc_read && cc_write) {
    if(ENABLE_EXE_FWD && stage2 == EX_LATCH){
    }
    else if(ENABLE_MEM_FWD && stage2 == MEM_LATCH) {
    }
    else {
      return true;
    }
  }

  return false;

}

bool check_hazards(Pipeline *p, uint8_t pipe)
{
  int i;
  for(i=0; i<pipe; i++) {
    if (check_hazard(p, pipe, i, ID_LATCH)) {
      return true;    
    }
  }

  for(i=0; i<PIPE_WIDTH; i++) {
    if (check_hazard(p, pipe, i, EX_LATCH)) {
      return true;    
    }
  }

  for(i=0; i<PIPE_WIDTH; i++) {
    if (check_hazard(p, pipe, i, MEM_LATCH)) {
      return true;    
    }
  }
  return false;
}

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

void update_ccmap()
{
  if (cc_map.inflight)
  {
      switch (cc_map.stage) {
      
        case FE_LATCH:
          // do nothing
          break;
        case ID_LATCH:
          cc_map.stage = EX_LATCH;
          break;
        case EX_LATCH:
          cc_map.stage = MEM_LATCH;
          break;
        case MEM_LATCH:
          cc_map.stage = FE_LATCH;
          cc_map.inflight = 0;
          break;
        default:
          fprintf(stderr, "Error: Should not get anything besides EX/MEM here. Got: %d\n", cc_map.stage);
          assert(0);
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

  for(ii=0; ii<PIPE_WIDTH; ii++){
        
    if(!p->pipe_latch[ID_LATCH][ii].stall) {
      p->pipe_latch[ID_LATCH][ii]=p->pipe_latch[FE_LATCH][ii];
      p->pipe_latch[FE_LATCH][ii].valid = 0;
    }

    if (ii==0) {
      p->pipe_latch[ID_LATCH][ii].stall = check_hazards(p, ii);
    }  
    else {
      p->pipe_latch[ID_LATCH][ii].stall = p->pipe_latch[ID_LATCH][ii-1].stall || check_hazards(p, ii);
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_FE(Pipeline *p) {
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

void pipe_check_bpred(Pipeline *p, Pipeline_Latch *fetch_op) {
  // call branch predictor here, if mispred then mark in fetch_op
  // update the predictor instantly
  // stall fetch using the flag p->fetch_cbr_stall
}


//--------------------------------------------------------------------//

