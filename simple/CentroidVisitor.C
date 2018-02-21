#include "CentroidVisitor.h"
#include "CentroidCalculator.h"

extern CProxy_Main mainProxy;
extern CProxy_CentroidCalculator centroid_calculator;

void CentroidVisitor::leaf(CentroidData cd, Key key) {
  centroid_calculator[key].receiveCentroid(cd, true);
}

void CentroidVisitor::node(CentroidData cd, Key key) {
  if (key == 1) {
    //CkPrintf("%f %f %f %f\n", moment.x, moment.y, moment.z, sum_mass);
    mainProxy.doneTraversal();
  }
  centroid_calculator[key >> 3].receiveCentroid(cd, false);
}

