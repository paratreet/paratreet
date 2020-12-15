#ifndef PARATREET_PRESSUREVISITOR_H_
#define PARATREET_PRESSUREVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include <cmath>

struct PressureVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

public:
  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
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

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        Real dsq = (target.particles()[i].position - source.particles()[j].position).lengthSquared();
        Real rsq = target.data.neighbors[i][0].fKey;
        if (dsq < rsq) target.data.fixed_ball[i].push_back(source.particles()[j]);
      }
    }
#if COUNT_INTERACTIONS
    centroid_resumer.ckLocalBranch()->countInts(target.n_particles);
#endif
  }
};

#endif // PARATREET_PRESSUREVISITOR_H_
