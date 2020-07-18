#ifndef PARATREET_COLLISIONVISITOR_H_
#define PARATREET_COLLISIONVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include <cmath>
#include <vector>
#include <queue>

struct CollisionVisitor {
// in leaf check for not same particle plz
public:
  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    // Optimization from ChaNGa to implement:
    // Create a bounding sphere around the target particles, test for intersect
    // between that sphere and the bounding box of the source. No intersect -> skip
    // the for loop

    // Check if any of the target balls intersect the source volume
    for (int i = 0; i < target.n_particles; i++) {
      Real ballSq = target.particles()[i].ball*target.particles()[i].ball;
      if(intersect(source.data.box, target.particles()[i].position, ballSq))
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
        if (dsq < rsq)
          target.data.fixed_ball[i].push_back(source.particles()[j]);
      }
    }
  }

private:
/*
 * Return true if a sphere centered at pos with radius^2 of rsq
 * intersects the bounding box of node.
 * This is being used instead of the Space intersect method so we can use
 * radius^2 for efficiency.
 * Copied from smooth.cpp in ChaNGa
 */
static inline bool
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
    }
};

#endif // PARATREET_COLLISIONVISITOR_H_
