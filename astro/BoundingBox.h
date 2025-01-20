#ifndef ASTRO_BOUNDING_BOX_H_
#define ASTRO_BOUNDING_BOX_H_

#include "OrientedBox.h"
#include "common.h"

/*
 * BoundingBox:
 * Used to calculate the bounding box of all particles in the
 * simulation universe. It also keeps track of particle energy,
 * to see whether there is a drift in the total system energy
 * through the simulation.
 */
struct BoundingBox {
  const OrientedBox<Real>& boxCorners() const {return box;}
  int numParticles() const {return n_particles;}

  OrientedBox<Real> box;
  int n_particles = 0;
  int n_sph = 0;
  int n_dark = 0;
  int n_star = 0;
  Real pe = 0.;
  Real ke = 0.;
  Real mass = 0.;
  Real updated_time = 0.;
  static CkReduction::reducerType boxReducer;

  BoundingBox();
  void pup(PUP::er &p);
  void expand(Real pad);
  void grow(const BoundingBox& other);
  void grow(const Vector3D<Real>& v);
  void reset();
  void finalizeUniverse();

  BoundingBox &operator+=(const BoundingBox& other){
    grow(other);
    return *this;
  }

  static void registerReducer() {
    boxReducer = CkReduction::addReducer(reduceFn);
  }

  static CkReductionMsg* reduceFn(int n_msgs, CkReductionMsg** msgs);

  static CkReduction::reducerType reducer() {
    return boxReducer;
  }
};

#include <iostream>
using namespace std;
ostream &operator<<(ostream &os, const BoundingBox &bb);

CkReductionMsg *reduceBoxes(int n_msg, CkReductionMsg** msgs);

#endif // ASTRO_BOUNDING_BOX_H_
