#ifndef PARATREET_PRESSUREVISITOR_H_
#define PARATREET_PRESSUREVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include "NeighborListCollector.h"
#include <cmath>

extern CProxy_NeighborListCollector neighbor_list_collector;

struct PressureVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

public:
  static bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    Real r_bucket = target.data.size_sm + target.data.max_rad;
    if (!Space::intersect(source.data.box, target.data.box.center(), r_bucket*r_bucket))
      return false;

    // Check if any of the target balls intersect the source volume
    // Ball size is set by furthest neighbor found during density calculation
    for (int i = 0; i < target.n_particles; i++) {
      Real ballSq = target.data.neighbors[i][0].fKey;
      if(Space::intersect(source.data.box, target.particles()[i].position, ballSq))
        return true;
    }
    return false;
  }

  static void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  static void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    auto collector = neighbor_list_collector.ckLocalBranch();
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        Real dsq = (target.particles()[i].position - source.particles()[j].position).lengthSquared();
        Real rsq = target.data.neighbors[i][0].fKey;
        if (dsq < rsq) {
          Vector3D<Real> force_on_target = 0.; // TODO SPH math
          collector->addNeighbor(source.data.home_pe, target.particles()[i].key, source.particles()[j].key, force_on_target);
        }
      }
    }
  }
};

#endif // PARATREET_PRESSUREVISITOR_H_
