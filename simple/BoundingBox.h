#ifndef SIMPLE_BOUNDING_BOX_H_
#define SIMPLE_BOUNDING_BOX_H_

#include "OrientedBox.h"
#include "Vector3D.h"
#include "pup.h"

/*
 * BoundingBox:
 * Used to calculate the bounding box of all particles in the
 * simulation universe. It also keeps track of particle energy,
 * to see whether there is a drift in the total system energy 
 * through the simulation.
 */
struct BoundingBox {
  OrientedBox<Real> box;
  int n_particles;
  Real pe;
  Real ke;
  Real mass;

  BoundingBox();
  void pup(PUP::er &p);
  void expand(Real pad);
  void grow(const BoundingBox& other);
  void grow(const Vector3D<Real>& v);
  void reset();

  BoundingBox &operator+=(const BoundingBox& other){
    grow(other);
    return *this;
  }
};

#include <iostream>
using namespace std;
ostream &operator<<(ostream &os, const BoundingBox &bb);

#endif // SIMPLE_BOUNDING_BOX_H_
