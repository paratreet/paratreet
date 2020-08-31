#ifndef PARATREET_PRESSUREVISITOR_H_
#define PARATREET_PRESSUREVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include <cmath>
#include <vector>
#include <queue>

struct PressureVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

private:
  void prepNeighbors(SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      particle_comp c (target.particles()[i]);
      std::priority_queue<Particle, std::vector<Particle>, particle_comp> pq (c);
      target.data.neighbors.resize(target.n_particles, pq);
    }
  }

public:
  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target)  {
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
    if (!target.data.neighbors.size()) prepNeighbors(target);
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        Real dsq = (target.particles()[i].position - source.particles()[j].position).lengthSquared();
        Real rsq = target.particles()[i].ball*target.particles()[i].ball;
        if (dsq < rsq)
          target.data.neighbors[i].push(source.particles()[j]);
      }
    }

    // For now, just print out the orders of the neighbors to check correctness
    //CkPrintf("%d %d %g %g %g %g\n", target.particles()[i].order, source.particles()[j].order,
    //                                target.particles()[i].ball, source.particles()[j].ball, dsq, rsq); 
#if COUNT_INTERACTIONS
    centroid_resumer.ckLocalBranch()->countInts(target.n_particles);
#endif
  }
};

#endif // PARATREET_PRESSUREVISITOR_H_
