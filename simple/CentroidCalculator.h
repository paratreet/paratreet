#ifndef SIMPLE_CENTROIDCALCULATOR_H_
#define SIMPLE_CENTROIDCALCULATOR_H_

#include "simple.decl.h"
#include "CentroidVisitor.h"

class CentroidCalculator : public CBase_CentroidCalculator {
private:
  Vector3D<Real> moment;
  Real sum_mass;
  CentroidVisitor v;
  int wait_count;
public:
  CentroidCalculator();
  void receiveCentroid (Vector3D<Real>, Real, bool if_leafi);
  Vector3D<Real> centroid () {
    return moment / sum_mass;
  }
};

#endif
