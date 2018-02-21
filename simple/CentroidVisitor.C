#include "CentroidVisitor.h"
#include "CentroidCalculator.h"

extern CProxy_Main mainProxy;
extern CProxy_CentroidCalculator centroid_calculator;

void CentroidVisitor::leaf(Vector3D<Real> moment, Real sum_mass, Key key) {
  centroid_calculator[key].receiveCentroid(moment, sum_mass, true);
}

void CentroidVisitor::node(Vector3D<Real> moment, Real sum_mass, Key key) {
  if (key == 1) {
    //CkPrintf("%f %f %f %f\n", moment.x, moment.y, moment.z, sum_mass);
    mainProxy.doneCentroid();
  }
  centroid_calculator[key >> 3].receiveCentroid(moment, sum_mass, false);
}
