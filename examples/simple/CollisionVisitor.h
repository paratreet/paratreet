#ifndef PARATREET_COLLISIONVISITOR_H_
#define PARATREET_COLLISIONVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include <cmath>
#include <vector>
#include <queue>

struct CollisionVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

// in leaf check for not same particle plz
public:
  static bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    Real r_bucket = target.data.size_sm + target.data.max_rad;
    if (!Space::intersect(source.data.box, target.data.box.center(), r_bucket*r_bucket))
      return false;

    // Check if any of the target balls intersect the source volume
    for (int i = 0; i < target.n_particles; i++) {
      Real ballSq = target.particles()[i].ball*target.particles()[i].ball;
      if(Space::intersect(source.data.box, target.particles()[i].position, ballSq))
        return true;
    }
    return false;
  }

  static void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  static void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        Real dsq = (target.particles()[i].position - source.particles()[j].position).lengthSquared();
        Real rsq = target.particles()[i].ball*target.particles()[i].ball;
        if (dsq < rsq) target.data.fixed_ball[i].push_back(&(source.particles()[j]));
      }
    }
  }

private:
};

#endif // PARATREET_COLLISIONVISITOR_H_
