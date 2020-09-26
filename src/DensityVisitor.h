#ifndef PARATREET_DENSITYVISITOR_H_
#define PARATREET_DENSITYVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include <cmath>
#include <vector>
#include <queue>

struct DensityVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

// in leaf check for not same particle plz
private:
  const int k = 32;
private:
  void prepNeighbors(SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
       Vector3D<Real> dr(0, 0, 0);
       pqSmoothNode pqNew;
       //pqNew.pl = target.particles()[i];
       pqNew.dx = dr;
       pqNew.fKey = dr.lengthSquared();
       CkVec<pqSmoothNode> *Q = &target.data.neighbors[i];
       Q->push_back(pqNew);
       //std::push_heap(&((*Q)[0]) + 0, &((*Q)[0]) + 1); 
    }
    target.data.neighborsInited = true;
  }

public:
  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    double r_bucket = target.data.size_sm + target.data.max_rad;
    if (!Space::intersect(source.data.box, target.data.box.center(), r_bucket*r_bucket))
      return false;

    // Check if any of the target balls intersect the source volume
    /*for (int i = 0; i < target.n_particles; i++) {
      // TODO: Is ball what we want here?
      Real ballSq = target.particles()[i].ball*target.particles()[i].ball;
      if(Space::intersect(source.data.box, target.particles()[i].position, ballSq))
        return true;
    }*/
    return false;
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    if (!target.data.neighborsInited) prepNeighbors(target);
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        // A particle cant be its own neighbor
        if (target.particles()[i].order == source.particles()[j].order)
          continue;
      }
    }
  }
};

#endif // PARATREET_DENSITYVISITOR_H_
