#include "CentroidCalculator.h"
#include "TraversalManager.h"
extern CProxy_TraversalManager traversal_manager;

void CentroidCalculator::receiveCentroid (Vector3D<Real> momenti, Real sum_massi) {
  moment += momenti;
  sum_mass += sum_massi;
  if (received()) {
    traversal_manager.ckLocalBranch()->getNext(thisProxy, thisIndex, UP_ONLY);
  }
}
void CentroidCalculator::node(Key key) {
  thisProxy[key].exist(false); // needs to be called here and not
  // in TM due to inheritance / demand creation combination problems
  thisProxy[key].receiveCentroid(moment, sum_mass);
}
void CentroidCalculator::leaf() {
  // i dont think this is needed for upward-only passes
  // just call node() every time
}
