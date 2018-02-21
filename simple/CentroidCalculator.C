#include "CentroidCalculator.h"

extern CProxy_Main mainProxy;

CentroidCalculator::CentroidCalculator() {
  //CkPrintf("%d\n", thisIndex);
  // set data to initial
  CentroidData cd;
  wait_count = -1;
  v = CentroidVisitor();
}
void CentroidCalculator::receiveCentroid (CentroidData cdi, bool if_leafi) {
  if (wait_count == -1) wait_count = (if_leafi) ? 1 : 8;
  cd = cd + cdi;
  wait_count--;
  if (wait_count == 0) {
    v.node(cd, thisIndex);
  }
}
