#ifndef SIMPLE_PRESSUREVISITOR_H_
#define SIMPLE_PRESSUREVISITOR_H_

#include "simple.decl.h"
#include "common.h"
#include <cmath>

struct PressureVisitor {
// in leaf check for not same particle plz
private:
  Real distsq(Vector3D<Real> p1, Vector3D<Real> p2) {
    Real dsq = (p1.x - p2.x) * (p1.x - p2.x);
    dsq += (p1.y - p2.y) * (p1.y - p2.y);
    dsq += (p1.z - p2.z) * (p1.z - p2.z);
    return dsq;
  }

public:
  bool node(Node<CentroidData>* from, Node<CentroidData>* on) {
    const Real radius = 1, total_volume = 3290.05;
    if (from->data.sum_mass < .000001) {CkPrintf("%d\n", from->key); return false;}
    Real dist = std::sqrt(distsq(from->data.getCentroid(), on->data.getCentroid()));
    Real s_from = std::pow(total_volume, 1/3.) * std::pow(2, -1 * Utility::getDepthFromKey(from->key));
    Real s_on = std::pow(total_volume, 1/3.) * std::pow(2, -1 * Utility::getDepthFromKey(on->key));
    if (dist - std::sqrt(2) * (s_from + s_on) > radius) return false;
    for (int i = 0; i < on->n_particles; i++) {
      Real subdist = std::sqrt(distsq(from->data.getCentroid(), on->particles[i].position));
      if (subdist - std::sqrt(2) * s_from < radius) return true;
    }
    return false;
  }

  void leaf(Node<CentroidData>* from, Node<CentroidData>* on) {
    const Real radius = 1;
    on->data.nearby.resize(on->n_particles);
    for (int i = 0; i < on->n_particles; i++) {
      for (int j = 0; j < from->n_particles; j++) {
        if (distsq(on->particles[i].position, from->particles[j].position) < radius * radius) {
          on->data.nearby[i].push_back(from->particles[j]);
        }
      }
    }
  }
};

#endif // SIMPLE_PRESSUREVISITOR_H_
