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
  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    double r_bucket = target.data.size_sm + target.data.max_rad;
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

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        Real dsq = (target.particles()[i].position - source.particles()[j].position).lengthSquared();
        Real rsq = target.particles()[i].ball*target.particles()[i].ball;
        // A particle cant be its own neighbor
        if (target.particles()[i].order == source.particles()[j].order)
          continue;
        if (dsq < rsq) {
          target.data.fixed_ball[i].push_back(source.particles()[j]);
          // For now, just print out the orders of the neighbors to check correctness
          //CkPrintf("%d %d %g %g %g %g\n", target.particles()[i].order, source.particles()[j].order,
          //                                target.particles()[i].ball, source.particles()[j].ball, dsq, rsq); 
          }
      }
    }
#if COUNT_INTERACTIONS
    centroid_resumer.ckLocalBranch()->countInts(target.n_particles);
#endif
  }

private:
/*
 * Return true if a sphere centered at pos with radius^2 of rsq
 * intersects the bounding box of node.
 * This is being used instead of the Space intersect method so we can use
 * radius^2 for efficiency.
 * Copied from smooth.cpp in ChaNGa
 */
/*static inline bool
intersect(const OrientedBox<Real>& box, Vector3D<Real> pos, Real rsq) {
    Real dsq = 0.0;
    Real delta;
    
    if((delta = box.lesser_corner.x - pos.x) > 0)
	dsq += delta * delta;
    else if((delta = pos.x - box.greater_corner.x) > 0)
	dsq += delta * delta;
    if(rsq < dsq)
	return false;
    if((delta = box.lesser_corner.y - pos.y) > 0)
	dsq += delta * delta;
    else if((delta = pos.y - box.greater_corner.y) > 0)
	dsq += delta * delta;
    if(rsq < dsq)
	return false;
    if((delta = box.lesser_corner.z - pos.z) > 0)
	dsq += delta * delta;
    else if((delta = pos.z - box.greater_corner.z) > 0)
	dsq += delta * delta;
    return (dsq <= rsq);
    }*/
};

#endif // PARATREET_COLLISIONVISITOR_H_
