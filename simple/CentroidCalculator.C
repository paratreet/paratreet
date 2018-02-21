#include "CentroidCalculator.h"

extern CProxy_Main mainProxy;

CentroidCalculator::CentroidCalculator() {
  //CkPrintf("%d\n", thisIndex);
  // set data to initial
  moment = Vector3D<Real> (0, 0, 0);
  sum_mass = 0;
  wait_count = -1;
  v = CentroidVisitor();
}
void CentroidCalculator::receiveCentroid (Vector3D<Real> momenti, Real sum_massi, bool if_leafi) {
  if (wait_count == -1) wait_count = (if_leafi) ? 1 : 8;
  moment += momenti;
  sum_mass += sum_massi;
  wait_count--;
  if (wait_count == 0) {
    v.node(moment, sum_mass, thisIndex);
  }
}
