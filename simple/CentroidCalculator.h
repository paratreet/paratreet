#ifndef SIMPLE_CENTROIDCALCULATOR_H_
#define SIMPLE_CENTROIDCALCULATOR_H_

#include "simple.decl.h"
#include "TreeElements.h"

extern CProxy_Main mainProxy;

class CentroidCalculator : public CBase_CentroidCalculator {
private:
  Vector3D<Real> moment;
  Real sum_mass;
public:
  CentroidCalculator() {
    moment = Vector3D<Real> (0, 0, 0);
    sum_mass = 0;
  }
  virtual void exist (bool if_leafi) {
    TreeElements::exist(if_leafi);
  }
  void receiveCentroid (Vector3D<Real> momenti, Real sum_massi);
  Vector3D<Real> centroid () {
    return moment / sum_mass;
  }
  virtual void node(Key);
  virtual void leaf();
  /*void printCentroid() {
    if (thisIndex == 1) {CkPrintf("[CC %d] mass %f, moment (", thisIndex, sum_mass);
    CkPrintf("%f, %f, %f)\n", moment.x, moment.y, moment.z);}
  }*/
};

#endif
