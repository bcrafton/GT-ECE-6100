#include "bpred.h"

#define TAKEN   true
#define NOTTAKEN false

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

BPRED::BPRED(uint32_t policy) {
    this->policy = (BPRED_TYPE) policy;
    this->stat_num_branches = 0;
    this->stat_num_mispred = 0;
  
    int i;
    for (i = 0; i < PHT_SIZE; i++)
    {
      this->pht[i] = 2;
    }

    this->bht = 0;
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

bool BPRED::GetPrediction(uint32_t PC){

    if(this->policy == BPRED_ALWAYS_TAKEN){
      return true;
    }
    else if(this->policy == BPRED_GSHARE){
      uint32_t address = (PC & this->bht) & BHT_MASK;
      return this->pht[address] >= 2;
    }
    else {
      return true;
    }
}


/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

void BPRED::UpdatePredictor(uint32_t PC, bool resolveDir, bool predDir) {
  uint32_t address = (PC & this->bht) & BHT_MASK;
  this->bht = (this->bht << 1) | resolveDir;
  if (resolveDir)
  {
    this->pht[address] = this->pht[address] == 3 ? 3 : this->pht[address] + 1;
  }
  else 
  {
    this->pht[address] = this->pht[address] == 0 ? 0 : this->pht[address] - 1;
  }
}

/////////////////////////////////////////////////////////////
/////////////////////////////////////////////////////////////

