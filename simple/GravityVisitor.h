#ifndef SIMPLE_GRAVITYVISITOR_H_
#define SIMPLE_GRAVITYVISITOR_H_

#include "simple.decl.h"
#include "CentroidData.h"
#include "common.h"
#include <cmath>

extern CProxy_Main mainProxy;

class GravityVisitor {
private:
  DataInterface<CentroidVisitor, CentroidData> d;
  Particle part;
  Vector3D<Real> sum_force;

  Real distsq(Vector3D<Real> p1, Vector3D<Real> p2) {
    Real dsq = (p1.x - p2.x) * (p1.x - p2.x);
    dsq += (p1.y - p2.y) * (p1.y - p2.y);
    dsq += (p1.z - p2.z) * (p1.z - p2.z);
    return dsq;
  }
  void addGravity(Real mass, Vector3D<Real> position) {
    const Real gconst = 0.000000000066742;
    Real rsq = distsq(part.position, position);
    sum_force += gconst * part.mass * mass / rsq;
  }
public:
  GravityVisitor() {}
  GravityVisitor(DataInterface<CentroidVisitor, CentroidData> di, Particle parti) : 
  d(di), part(parti), sum_force(Vector3D<Real> (0,0,0)) {}
  void leaf(Key key, int n_particles, Particle* particles) {
    for (int i = 0; i < n_particles; i++) {
      addGravity(particles[i].mass, particles[i].position);
    }
  }
  bool node(CentroidData cd, Key key) {
    // need to put all final data into map
    const Real theta = .5;
    Real s = std::pow(2, -1 * Utility::getDepthFromKey(key)); // is this right?
    //CentroidData cd = d.get(key);
    if (distsq(part.position, cd.moment) < (s * s) / (theta * theta)) {
      return true;
    }
    else {
      addGravity(cd.sum_mass, cd.moment);
      return false;
    }
  }
  void pup(PUP::er& p) {
    p | d;
    p | part;
    p | sum_force;
  }
};

#endif //SIMPLE_GRAVITYVISITOR_H_
