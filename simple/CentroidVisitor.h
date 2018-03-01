#ifndef SIMPLE_CENTROIDVISITOR_H_
#define SIMPLE_CENTROIDVISITOR_H_

#include "simple.decl.h"
#include "CentroidData.h"
#include "common.h"

class CentroidVisitor {
public:
  void leaf(CProxy_TreeElement<CentroidVisitor, CentroidData> centroid_calculator, CentroidData cd, Key key) {
    centroid_calculator[key].receiveData(cd, true);
  }
  void node(CProxy_Main mainProxy, CProxy_TreeElement<CentroidVisitor, CentroidData> centroid_calculator, CentroidData cd, Key key) {
    if (key == 1) {
      //CkPrintf("%f %f %f %f\n", cd.sum_mass, cd.moment.x, cd.moment.y, cd.moment.x);
      mainProxy.doneTraversal();
    }
    centroid_calculator[key >> 3].receiveData(cd, false);
  }
  void pup(PUP::er& p) {}
};

#endif //SIMPLE_CENTROIDVISITOR_H_
