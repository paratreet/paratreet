#ifndef SIMPLE_GRAVITYVISITOR_H_
#define SIMPLE_GRAVITYVISITOR_H_

#include "simple.decl.h"
//#include "CentroidData.h"
#include "common.h"
#include <cmath>

extern CProxy_Driver<CentroidData> centroid_driver;

class GravityVisitor {
private:
  Real distsq(Vector3D<Real> p1, Vector3D<Real> p2) {
    Real dsq = (p1.x - p2.x) * (p1.x - p2.x);
    dsq += (p1.y - p2.y) * (p1.y - p2.y);
    dsq += (p1.z - p2.z) * (p1.z - p2.z);
    return dsq;
  }
  void addGravity(const Node<CentroidData>* source, Node<CentroidData>* target, std::vector<Vector3D<Real> >& sum_forces, bool isLeaf) {
    const Real gconst = 0.000000000066742; // very small
    sum_forces.resize(target->n_particles);
    for (int i = 0; i < target->n_particles; i++) {
      if (isLeaf) {
        for (int j = 0; j < source->n_particles; j++) {
          if (target->particles[i].key == target->particles[j].key) continue;
          Real rsq = distsq(source->particles[j].position, target->particles[i].position);
          sum_forces[i] += (source->particles[j].position - target->particles[i].position)
            * gconst * source->particles[j].mass * target->particles[i].mass / (rsq * std::sqrt(rsq));
        }
      }
      else {
        Real rsq = distsq(source->data.getCentroid(), target->particles[i].position);
        sum_forces[i] += (source->data.getCentroid() - target->particles[i].position)
          * gconst * source->data.sum_mass * target->particles[i].mass / (rsq * std::sqrt(rsq));
      }
    }
  }
public:
  GravityVisitor() {}
  void leaf(const Node<CentroidData>* source, Node<CentroidData>* target) { // we're calculating force from node X on node Y
    centroid_driver.countLeaf();
    addGravity(source, target, target->sum_forces, true);
  }
  bool node(const Node<CentroidData>* source, Node<CentroidData>* target) {
    const Real theta = .5, total_volume = 41050;
    Real s = std::pow(total_volume, 1/3.) * std::pow(2, -1 * Utility::getDepthFromKey(source->key));
    if (theta == 0 || distsq(source->data.getCentroid(), target->data.getCentroid()) < s * s / (theta * theta)) {
      return true;
    }
    addGravity(source, target, target->sum_forces, false);
    return false;
  }
  void pup(PUP::er& p) {}
};

#endif //SIMPLE_GRAVITYVISITOR_H_
