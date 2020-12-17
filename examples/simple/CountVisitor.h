#ifndef PARATREET_COUNTVISITOR_H_
#define PARATREET_COUNTVISITOR_H_

#include <cmath>
#include <vector>
#include <queue>

#include "paratreet.decl.h"
#include "CountManager.h"
#include "common.h"

/* readonly */ CProxy_CountManager count_manager;

class CountVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

private:
  static Real dist(Vector3D<Real> p1, Vector3D<Real> p2) {
    return (p1-p2).length();
  }

  static Vector3D<Real> center(const CentroidData& data) {
    return data.box.center();
  }

  static Real radius(const CentroidData& data) {
    return data.box.size().length()/2;
  }

  static int findBin(const CentroidData& from, const CentroidData& on) {
    Real d = dist(center(from), center(on));
    Real r1 = radius(from), r2 = radius(on);
    return count_manager.ckLocalBranch()->findBin(d - r1 - r2, d + r1 + r2);
  }

public:
  static bool open(const SpatialNode<CentroidData>& from, SpatialNode<CentroidData>& on) {
    if (from.data.count == 0 || on.data.count == 0) {
      return false;
    }
    CountManager* countManager = count_manager.ckLocalBranch();
    int idx = findBin(from.data, on.data);
    if (idx < 0) {
      return idx == -1;
    } else {
      countManager->bins[idx] += from.data.count * on.data.count;
      return false;
    }
  }

  static void node(const SpatialNode<CentroidData>& from, SpatialNode<CentroidData>& on) {}

  static bool cell(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    return open(source, target);
  }

  static void leaf(const SpatialNode<CentroidData>& from, SpatialNode<CentroidData>& on) {
    CountManager* countManager = count_manager.ckLocalBranch();
    int idx = findBin(from.data, on.data);
    if (idx < 0) {
      for (int i = 0; i < from.n_particles; i++) {
        for (int j = 0; j < on.n_particles; j++) {
          const Vector3D<Real>& p1 = from.particles()[i].position;
          const Vector3D<Real>& p2 = on.particles()[j].position;
          countManager->count(dist(p1, p2));
        }
      }
    } else {
      countManager->bins[idx] += from.n_particles * on.n_particles;
    }
  }
};

#endif // PARATREET_COUNTVISITOR_H_
