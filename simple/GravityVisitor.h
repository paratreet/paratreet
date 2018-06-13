#ifndef SIMPLE_GRAVITYVISITOR_H_
#define SIMPLE_GRAVITYVISITOR_H_

#include "simple.decl.h"
//#include "CentroidData.h"
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
  void addGravity(Node<CentroidData>* from, Node<CentroidData>* on, std::vector<Vector3D<Real> >& sum_forces, bool isLeaf) {
    const Real gconst = 0.000000000066742; // very small
    sum_forces.resize(on->n_particles);
    for (int i = 0; i < on->n_particles; i++) {
      if (isLeaf) {
        for (int j = 0; j < from->n_particles; j++) {
          Real rsq = distsq(from->particles[j].position, on->particles[i].position);
          sum_forces[i] += (from->particles[j].position - on->particles[i].position)
            * gconst * from->particles[j].mass * on->particles[i].mass / (rsq * std::sqrt(rsq));
        }
      } 
      else {
        Real rsq = distsq(from->data.getCentroid(), on->particles[i].position);
        sum_forces[i] += (from->data.getCentroid() - on->particles[i].position)
          * gconst * from->data.sum_mass * on->particles[i].mass / (rsq * std::sqrt(rsq));
      }
    }
  }
public:
  GravityVisitor() {}
  void leaf(Node<CentroidData>* from, Node<CentroidData>* on) { // we're calculating force from node X on node Y
    addGravity(from, on, on->data.sum_forces, true);
  }
  bool node(Node<CentroidData>* from, Node<CentroidData>* on) {
    if (from->data.sum_mass < .00001) return false;
    const Real theta = .5, total_volume = 3290.05;
    Real s = std::pow(total_volume, 1/3.) * std::pow(2, -1 * Utility::getDepthFromKey(from->key));
    if (theta == 0 || distsq(from->data.getCentroid(), on->data.getCentroid()) < s * s / (theta * theta)) {
      return true;
    }
    addGravity(from, on, on->data.sum_forces, false);
    return false;
  }
  void pup(PUP::er& p) {}
};

#endif //SIMPLE_GRAVITYVISITOR_H_
