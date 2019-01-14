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
    const Real gconst = 0.1 ;// 0.000000000066742; // very small
    sum_forces.resize(target->n_particles, Vector3D<Real>(0,0,0));
    for (int i = 0; i < target->n_particles; i++) {
      if (isLeaf) {
        for (int j = 0; j < source->n_particles; j++) {
          if (target->particles[i].key == source->particles[j].key) continue;
          Real rsq = distsq(source->particles[j].position, target->particles[i].position);
          Vector3D<Real> curr_force = (source->particles[j].position - target->particles[i].position)
            * gconst * source->particles[j].mass * target->particles[i].mass / (rsq * std::sqrt(rsq));
          sum_forces[i] += curr_force;
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
    //centroid_driver.countLeaf();
    addGravity(source, target, target->sum_forces, true);
  }
  bool node(const Node<CentroidData>* source, Node<CentroidData>* target) {
    const Real theta = 0.5, total_volume = 41050;
    Real s = std::pow(total_volume, 1/3.) * std::pow(2, -1 * Utility::getDepthFromKey(source->key));
    Real soverd = s / std::sqrt(distsq(source->data.getCentroid(), target->data.getCentroid()));
    if (soverd > theta) {
      return true;
    }
    addGravity(source, target, target->sum_forces, false);
    return false;
  }
  bool cell(std::pair<Key, const CentroidData&> source, std::pair<Key, const CentroidData&> target) {
    // find the closest particle in target to source's center
    // check all eight corners of bounding box
    // return true if one corner is within total_volume of node
    // else return false
    const OrientedBox<Real>& box = target.second.box;
    for (int i = 0; i < 2; i++) {
      for (int j = 0; j < 2; j++) {
        for (int k = 0; k < 2; k++) {
          Vector3D<Real> corner;
          corner.x = (i) ? box.greater_corner.x : box.lesser_corner.x;
          corner.y = (j) ? box.greater_corner.y : box.lesser_corner.y;
          corner.z = (k) ? box.greater_corner.z : box.lesser_corner.z;
          const Real theta = .5, total_volume = 41050;
          Real s = std::pow(total_volume, 1/3.) * std::pow(2, -1 * Utility::getDepthFromKey(source.first));
          if (theta == 0 || distsq(source.second.getCentroid(), corner) < s * s / (theta * theta)) {
            return true;
          }
        }
      }
    }
  }
  void pup(PUP::er& p) {}
};

#endif //SIMPLE_GRAVITYVISITOR_H_
