#ifndef SIMPLE_GRAVITYVISITOR_H_
#define SIMPLE_GRAVITYVISITOR_H_

#include "simple.decl.h"
#include "CentroidData.h"
#include "common.h"
#include <cmath>

class GravityVisitor {
private:
  Real distsq(Vector3D<Real> p1, Vector3D<Real> p2) {
    Real dsq = (p1.x - p2.x) * (p1.x - p2.x);
    dsq += (p1.y - p2.y) * (p1.y - p2.y);
    dsq += (p1.z - p2.z) * (p1.z - p2.z);
    return dsq;
  }
  void addGravity(Real mass, Vector3D<Real> position, Particle& p, Vector3D<Real>& sum_force) {
    const Real gconst = 0.000000000066742; // small, maybe you should just do sum_accel
    Real rsq = distsq(p.position, position);
    sum_force += (position - p.position) * gconst * p.mass * mass / rsq;
  }
public:
  GravityVisitor() {}
  void leaf(Node<CentroidData>* node, Particle& p, Vector3D<Real>& sum_force) {
    for (int i = 0; i < node->n_particles; i++) {
      if (node->particles[i] == p) continue;
      addGravity(node->particles[i].mass, node->particles[i].position, p, sum_force);
    }
  }
  bool node(Node<CentroidData>* node, Particle& p, Vector3D<Real>& sum_force ) {
    const Real theta = .5, total_volume = 3290.05;
    Real s = std::pow(total_volume, 1/3.) * std::pow(2, -1 * Utility::getDepthFromKey(node->key));
    if (theta == 0 || distsq(p.position, node->data.moment) < s * s / (theta * theta)) {
      return true;
    }
    else {
      addGravity(node->data.sum_mass, node->data.moment, p, sum_force);
      return false;
    }
  }
  void pup(PUP::er& p) {}
};

#endif //SIMPLE_GRAVITYVISITOR_H_
