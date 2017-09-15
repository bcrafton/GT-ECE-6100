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
    std::cout <<"cycle count : " << p->stat_num_cycle << " retired_instruction : " << p->stat_retired_inst << " retired inst number : " << p->stat_retired_inst << std::endl;

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
  if (1) {
    printf("%lu | %d %d | %d %d | %d %d | %lx %d %d | %d | %d %d %d %d | %d %d\n",

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

    i->tr_entry.cc_read,
    i->tr_entry.cc_write,
    i->tr_entry.br_dir,
    i->is_mispred_cbr,

    i->stall,
    i->valid

    );
  }
}

void pipe_cycle(Pipeline *p)
{

    p->stat_num_cycle++;

    pipe_cycle_WB(p);
    pipe_cycle_MEM(p);
    pipe_cycle_EX(p);
    pipe_cycle_ID(p);
    pipe_cycle_FE(p);

/*
    print_instruction(&p->pipe_latch[MEM_LATCH][0]);
    print_instruction(&p->pipe_latch[MEM_LATCH][1]);
    print_instruction(&p->pipe_latch[EX_LATCH][0]);
    print_instruction(&p->pipe_latch[EX_LATCH][1]);

    print_instruction(&p->pipe_latch[ID_LATCH][0]);
    print_instruction(&p->pipe_latch[ID_LATCH][1]);

    pipe_print_state(p);
*/


	    
}

/**********************************************************************
 * -----------  DO NOT MODIFY THE CODE ABOVE THIS LINE ----------------
 **********************************************************************/

typedef struct hazard {
  uint64_t op_id;
  bool hazard;
  bool forward;
} hazard_t; 

typedef struct hazards {
  hazard_t src1;
  hazard_t src2;
  hazard_t cc_read;
  hazard_t mem;
} hazards_t;

uint8_t order(Pipeline *p, uint8_t pipe, Latch_Type latch)
{
  uint8_t order = 0;
  
  if (p->pipe_latch[latch][pipe].op_id == 0)
  {
    return pipe;
  }

  if (!p->pipe_latch[latch][pipe].valid) {
    return PIPE_WIDTH-1;
  }

  int i;
  for(i=0; i<PIPE_WIDTH; i++) {
    if ( (p->pipe_latch[latch][pipe].op_id > p->pipe_latch[latch][i].op_id) && p->pipe_latch[latch][i].valid)
    {
      order++;
    }
  }
  return order;
}

uint8_t r_order(Pipeline *p, uint8_t index, Latch_Type latch)
{
  int i;
  for(i=0; i<PIPE_WIDTH; i++) {
    if (order(p, i, latch) == index){
      return i;
    }
  }
  return 0;
}

void check_hazard(Pipeline *p, uint8_t pipe, uint8_t pipe2, Latch_Type stage2, hazards_t* hazards)
{

  // younger

  uint8_t src1 =        p->pipe_latch[ID_LATCH][pipe].tr_entry.src1_reg;
  uint8_t src1_needed = p->pipe_latch[ID_LATCH][pipe].tr_entry.src1_needed;
  uint8_t src2 =        p->pipe_latch[ID_LATCH][pipe].tr_entry.src2_reg;
  uint8_t src2_needed = p->pipe_latch[ID_LATCH][pipe].tr_entry.src2_needed;
  uint8_t cc_read =     p->pipe_latch[ID_LATCH][pipe].tr_entry.cc_read;

  uint8_t valid1 =      p->pipe_latch[ID_LATCH][pipe].valid;
  uint8_t mem_read1 =   p->pipe_latch[ID_LATCH][pipe].tr_entry.mem_read;
  uint64_t mem_addr1 =  p->pipe_latch[ID_LATCH][pipe].tr_entry.mem_addr;

  uint64_t op_id1 =  p->pipe_latch[ID_LATCH][pipe].op_id;

  // older

  uint8_t dest =        p->pipe_latch[stage2][pipe2].tr_entry.dest;
  uint8_t dest_needed = p->pipe_latch[stage2][pipe2].tr_entry.dest_needed;
  uint8_t cc_write =    p->pipe_latch[stage2][pipe2].tr_entry.cc_write;
  
  uint8_t valid2 =      p->pipe_latch[stage2][pipe2].valid;
  uint8_t mem_read2 =   p->pipe_latch[stage2][pipe2].tr_entry.mem_read;
  uint64_t mem_addr2 =  p->pipe_latch[stage2][pipe2].tr_entry.mem_addr;
  uint8_t mem_write2 =  p->pipe_latch[stage2][pipe2].tr_entry.mem_write;
  uint64_t op_id2 =     p->pipe_latch[stage2][pipe2].op_id;

  // check

  if (!valid1 || !valid2) {
    return;
  }

  if (op_id1 <= op_id2)
  {
    return;
  }

  if ( ((op_id1+1 < op_id2) || (op_id2+1 < op_id1)) && (stage2 == ID_LATCH) )
  {
    //fprintf(stderr, "id1 id2: %lu %lu\n", op_id1, op_id2);
    //assert(0);
  }

  if (src1_needed && dest_needed && (src1 == dest)) {
    bool forward = false;
    bool hazard = false;
    if (ENABLE_EXE_FWD && (stage2 == EX_LATCH) && !mem_read2){
      forward = true;
      hazard = false;
    }
    else if(ENABLE_MEM_FWD && (stage2 == MEM_LATCH)) {
      forward = true;
      hazard = false;
    }
    else {
      forward = false;
      hazard = true;
    }

    if (((hazards->src1.hazard || hazards->src1.forward) && (op_id2 > hazards->src1.op_id)) || (!hazards->src1.hazard && !hazards->src1.forward))
    {
      hazards->src1.forward = forward;
      hazards->src1.hazard = hazard;
      hazards->src1.op_id = op_id2;
    }
  }

  if (src2_needed && dest_needed && (src2 == dest)) {
    bool forward = false;
    bool hazard = false;
    if(ENABLE_EXE_FWD && (stage2 == EX_LATCH) && !mem_read2){
      forward = true;
      hazard = false;
    }
    else if(ENABLE_MEM_FWD && (stage2 == MEM_LATCH)) {
      forward = true;
      hazard = false;
    }
    else {
      forward = false;
      hazard = true;
    }

    if (((hazards->src2.hazard || hazards->src2.forward) && (op_id2 > hazards->src2.op_id)) || (!hazards->src2.hazard && !hazards->src2.forward))
    {
      hazards->src2.forward = forward;
      hazards->src2.hazard = hazard;
      hazards->src2.op_id = op_id2;
    }
  }

  if (cc_read && cc_write) {

    bool forward = false;
    bool hazard = false;
    if(ENABLE_EXE_FWD && (stage2 == EX_LATCH) && !mem_read2){
      forward = true;
      hazard = false;
    }
    else if(ENABLE_MEM_FWD && (stage2 == MEM_LATCH)) {
      forward = true;
      hazard = false;
    }
    else {
      forward = false;
      hazard = true;
    }

    if (((hazards->cc_read.hazard || hazards->cc_read.forward) && (op_id2 > hazards->cc_read.op_id)) || (!hazards->cc_read.hazard && !hazards->cc_read.forward))
    {
      hazards->cc_read.forward = forward;
      hazards->cc_read.hazard = hazard;
      hazards->cc_read.op_id = op_id2;
    }
  }

  if (mem_read1 && mem_write2 && (mem_addr1 == mem_addr2)){
    bool forward = false;
    bool hazard = false;
    if(ENABLE_MEM_FWD && (stage2 == MEM_LATCH)) {
      forward = true;
      hazard = false;
    }
    else{
      forward = false;
      hazard = true;
    }

    if (((hazards->mem.hazard || hazards->mem.forward) && (op_id2 > hazards->mem.op_id)) || (!hazards->mem.hazard && !hazards->mem.forward))
    {
      hazards->mem.forward = forward;
      hazards->mem.hazard = hazard;
      hazards->mem.op_id = op_id2;
    }
  }
}

bool check_hazards(Pipeline *p, uint8_t pipe)
{
  int i;
  hazards_t hazards = {0,};  

  for(i=PIPE_WIDTH-1; i>=0; i--) {
    check_hazard(p, pipe, i, ID_LATCH, &hazards);
  }

  for(i=PIPE_WIDTH-1; i>=0; i--) {
    check_hazard(p, pipe, i, EX_LATCH, &hazards);
  }

  for(i=PIPE_WIDTH-1; i>=0; i--) {
    check_hazard(p, pipe, i, MEM_LATCH,  &hazards);
  }

  return hazards.src1.hazard || hazards.src2.hazard || hazards.cc_read.hazard || hazards.mem.hazard;
}

void pipe_cycle_WB(Pipeline *p){
  int ii;  

  for(ii=0; ii<PIPE_WIDTH; ii++){

    if(p->pipe_latch[MEM_LATCH][ii].valid){

      //check_opid(&p->pipe_latch[MEM_LATCH][ii]);

      p->stat_retired_inst++;
      //printf("retired instructions: %lu\n", p->stat_retired_inst);

      if(p->pipe_latch[MEM_LATCH][ii].op_id >= p->halt_op_id){
	      p->halt=true;
      }

      // no idea where the branch finally resolves ... i guess it is here.
      if(BPRED_POLICY){
        if (p->fetch_cbr_stall && p->pipe_latch[MEM_LATCH][ii].is_mispred_cbr) {
          p->fetch_cbr_stall = false;
        }
      }

    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_MEM(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){

    //print_instruction(&p->pipe_latch[EX_LATCH][ii]);

    p->pipe_latch[MEM_LATCH][ii]=p->pipe_latch[EX_LATCH][ii];
    p->pipe_latch[EX_LATCH][ii].valid = 0;

    if(BPRED_POLICY){
      if (p->fetch_cbr_stall && p->pipe_latch[MEM_LATCH][ii].is_mispred_cbr) {
        //p->fetch_cbr_stall = false;
      }
    }

  }
}

//--------------------------------------------------------------------//

void pipe_cycle_EX(Pipeline *p){

  int ii;

  for(ii=0; ii<PIPE_WIDTH; ii++){

    //print_instruction(&p->pipe_latch[ID_LATCH][ii]);

    if(p->pipe_latch[ID_LATCH][ii].stall) {
      p->pipe_latch[EX_LATCH][ii].valid = 0;
    }

    else {
      p->pipe_latch[EX_LATCH][ii]=p->pipe_latch[ID_LATCH][ii];
      p->pipe_latch[ID_LATCH][ii].valid = 0;
    }

  }
}

//--------------------------------------------------------------------//

void pipe_cycle_ID(Pipeline *p){

  int ii;
  int j;

  for(ii=0; ii<PIPE_WIDTH; ii++){

    //print_instruction(&p->pipe_latch[FE_LATCH][ii]);

    if(!p->pipe_latch[ID_LATCH][ii].stall) {
      p->pipe_latch[ID_LATCH][ii]=p->pipe_latch[FE_LATCH][ii];
      p->pipe_latch[FE_LATCH][ii].valid = 0;
    }
  }

  for(ii=0; ii<PIPE_WIDTH; ii++){
    p->pipe_latch[ID_LATCH][ii].stall = check_hazards(p, ii);
    for(j=0; j<PIPE_WIDTH; j++){
      if (p->pipe_latch[ID_LATCH][ii].op_id > p->pipe_latch[ID_LATCH][j].op_id)
      {
        p->pipe_latch[ID_LATCH][ii].stall = p->pipe_latch[ID_LATCH][ii].stall || check_hazards(p, j);
      }
    }
  }
}

//--------------------------------------------------------------------//

void pipe_cycle_FE(Pipeline *p) {
  int ii;
  Pipeline_Latch fetch_op;
  bool tr_read_success;

  // i do not think these must always be equal.
  // assert(p->pipe_latch[FE_LATCH][0].valid == p->pipe_latch[FE_LATCH][1].valid);

  for(ii=0; ii<PIPE_WIDTH; ii++){

    if (!p->pipe_latch[FE_LATCH][ii].valid && !p->fetch_cbr_stall) {
      
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

  // if (BPRED_ALWAYS_TAKEN)

  // if our prediction was wrong
  if (fetch_op->tr_entry.op_type == OP_CBR)
  {
    p->b_pred->stat_num_branches++;
    bool pred = p->b_pred->GetPrediction(fetch_op->tr_entry.inst_addr);
    if(pred != fetch_op->tr_entry.br_dir)
    {
      p->b_pred->stat_num_mispred++;      

      fetch_op->is_mispred_cbr = true;
      p->fetch_cbr_stall = true;
    }
    // may have to change order or pred and br.
    p->b_pred->UpdatePredictor(fetch_op->tr_entry.inst_addr, fetch_op->tr_entry.br_dir, pred);
  }
}


//--------------------------------------------------------------------//

