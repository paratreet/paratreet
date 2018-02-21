#ifndef SIMPLE_CENTROIDDATA_H_
#define SIMPLE_CENTROIDDATA_H_

#include "common.h"

struct CentroidData {
  Vector3D<Real> moment;
  Real sum_mass;

  CentroidData() : 
  moment(Vector3D<Real> (0, 0, 0)),
  sum_mass(0) {}
  const CentroidData& operator+ (const CentroidData& cd) { // needed for upward traversal
    moment += cd.moment;
    sum_mass += cd.sum_mass;
    return *this;
  }
  /*const CentroidData& operator= (const CentroidData& cd) {
    moment = cd.moment;
    sum_mass = cd.sum_mass;
  }*/
  void pup(PUP::er& p) {
    p | moment;
    p | sum_mass;
  }
  Vector3D<Real> getCentroid() {
    return moment / sum_mass;
  }
};

#endif // SIMPLE_CENTROID_H_
