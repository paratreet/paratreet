
#ifndef _PREFIXLB_H_
#define _PREFIXLB_H_

#include "CentralLB.h"
#include "PrefixLB.decl.h"

class minheap;
class maxheap;

void CreatePrefixLB();
BaseLB *AllocatePrefixLB();

class PrefixLB : public CBase_PrefixLB {
protected:
  computeInfo *computes;
  processorInfo *processors;
  minHeap *pes;
  maxHeap *computesHeap;
  int P;
  int numComputes;
  double averageLoad;

  double overLoad;
  double start_time;

public:
  PrefixLB(const CkLBOptions &);
  PrefixLB(CkMigrateMessage *m):CBase_PrefixLB(m) { lbname = (char *)"PrefixLB"; };
  void work(LDStats* stats);
private:
  bool QueryBalanceNow(int step) { return true; }

};

#endif /* _REFINELB_H_ */

/*@}*/
