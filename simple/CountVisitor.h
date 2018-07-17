#ifndef SIMPLE_COUNTVISITOR_H_
#define SIMPLE_COUNTVISITOR_H_

#include "simple.decl.h"
#include "CountManager.h"
#include "common.h"
#include <cmath>
#include <vector>
#include <queue>

class CountVisitor {
private:
  Real dist(Vector3D<Real> p1, Vector3D<Real> p2) {
    return (p1-p2).length();
  }

  Vector3D<Real> center(Node<CentroidData>* node) {
    return node->data.box.center();
  }

  Real radius(Node<CentroidData>* node) {
    return node->data.box.size().length()/2;
  }

  int findBin(Node<CentroidData>* from, Node<CentroidData>* on) {
    Real d = dist(center(from), center(on));
    Real r1 = radius(from), r2 = radius(on);
    return count_manager.ckLocalBranch()->findBin(d - r1 - r2, d + r1 + r2);
  }

public:
  bool node(Node<CentroidData>* from, Node<CentroidData>* on) {
    if (from->data.count == 0 || from->data.count == 0) {
      return false;
    }
    CountManager* countManager = count_manager.ckLocalBranch();
    int idx = findBin(from, on);
    if (idx < 0) {
      return idx == -1;
    } else {
      countManager->bins[idx] += from->data.count * on->data.count;
      return false;
    }
  }

  void leaf(Node<CentroidData>* from, Node<CentroidData>* on) {
    CountManager* countManager = count_manager.ckLocalBranch();
    int idx = findBin(from, on);
    if (idx < 0) {
      for (int i = 0; i < from->n_particles; i++) {
        for (int j = 0; j < on->n_particles; j++) {
          const Vector3D<Real>& p1 = from->particles[i].position;
          const Vector3D<Real>& p2 = on->particles[j].position;
          countManager->count(dist(p1, p2));
        }
      }
    } else {
      countManager->bins[idx] += from->n_particles * on->n_particles;
    }
  }
};

#endif // SIMPLE_DENSITYVISITOR_H_
