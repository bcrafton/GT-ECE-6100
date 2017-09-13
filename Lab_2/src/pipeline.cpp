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

uint8_t order(Pipeline *p, uint8_t pipe, Latch_Type)
{
}

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

bool check_hazard(Pipeline *p, uint8_t pipe, uint8_t pipe2, Latch_Type stage2)
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
    return false;
  }

  if (op_id1+1 < op_id2)
  {
    assert(0);
  }

  if (op_id1 < op_id2)
  {
    return false;
  }

  if (src1_needed && dest_needed && (src1 == dest)) {
    if(ENABLE_EXE_FWD && (stage2 == EX_LATCH) && !mem_read2){
    }
    else if(ENABLE_MEM_FWD && (stage2 == MEM_LATCH)) {
    }
    else {
      //printf("src1 hazard\n");
      return true;
    }
  }

  if (src2_needed && dest_needed && (src2 == dest)) {
    if(ENABLE_EXE_FWD && (stage2 == EX_LATCH) && !mem_read2){
    }
    else if(ENABLE_MEM_FWD && (stage2 == MEM_LATCH)) {
    }
    else {
      //printf("src2 hazard\n");
      return true;
    }
  }

  if (cc_read && cc_write) {
    if(ENABLE_EXE_FWD && (stage2 == EX_LATCH)){
    }
    else if(ENABLE_MEM_FWD && (stage2 == MEM_LATCH)) {
    }
    else {
      //printf("cc hazard\n");
      return true;
    }
  }

  if (mem_read1 && mem_write2 && (mem_addr1 == mem_addr2)){
    if(ENABLE_MEM_FWD && (stage2 == MEM_LATCH)) {
    }
    else{
      //printf("Store - Load stall\n");
      return true;
    }
  }

  return false;

}

bool check_hazards(Pipeline *p, uint8_t pipe)
{
  int i;
  
  for(i=pipe-1; i>=0; i--) {
    if (check_hazard(p, pipe, i, ID_LATCH)) {
      return true;
    }
  }

  for(i=PIPE_WIDTH-1; i>=0; i--) {
    if (check_hazard(p, pipe, i, EX_LATCH)) {
      return true;    
    }
  }

  for(i=PIPE_WIDTH-1; i>=0; i--) {
    if (check_hazard(p, pipe, i, MEM_LATCH)) {
      return true;    
    }
  }
  return false;
}

bool pipeline_stalled(Pipeline *p, uint8_t pipe)
{
  bool stall = false;
  int i;
  for(i=pipe; i<PIPE_WIDTH; i++){
    stall = stall || p->pipe_latch[ID_LATCH][i].stall;
  }
  return stall;
}

void check_opid(Pipeline_Latch* l)
{
  static uint64_t op_id = 1;
  static uint64_t fuckups = 0;
  if (l->valid)
  {
    if (op_id == l->op_id)
    {
      op_id++;
    }
    else
    {
      assert(0);
      fprintf(stderr, "wrong op id: %lu %lu %lu\n", op_id, l->op_id, fuckups);
      fuckups++;
      op_id = l->op_id+1;
    }
  }
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
    }

  }
}

//--------------------------------------------------------------------//

void pipe_cycle_MEM(Pipeline *p){
  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){

    p->pipe_latch[MEM_LATCH][ii]=p->pipe_latch[EX_LATCH][ii];

    if(BPRED_POLICY){
      if (p->fetch_cbr_stall && p->pipe_latch[MEM_LATCH][ii].is_mispred_cbr) {
        p->fetch_cbr_stall = false;
      }
    }
  }
}

//--------------------------------------------------------------------//

bool stall(Pipeline *p, uint8_t pipe) {

  if (pipe == 0) {
    return (p->pipe_latch[ID_LATCH][0].stall) || ((p->pipe_latch[ID_LATCH][0].op_id > p->pipe_latch[ID_LATCH][1].op_id) && p->pipe_latch[ID_LATCH][1].stall);
  }

  else {
    return p->pipe_latch[ID_LATCH][1].stall;
  }
}

void pipe_cycle_EX(Pipeline *p){

  int ii;
  for(ii=0; ii<PIPE_WIDTH; ii++){

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
    if(!fetch_op->tr_entry.br_dir)
    {
      p->b_pred->stat_num_mispred++;      

      fetch_op->is_mispred_cbr = true;
      p->fetch_cbr_stall = true;
    }
  }
}


//--------------------------------------------------------------------//

