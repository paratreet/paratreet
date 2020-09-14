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
    target.data.drMax2.resize(target.n_particles);
    for (int i = 0; i < target.n_particles; i++) {
      particle_comp c (target.particles()[i]);
      std::priority_queue<Particle, std::vector<Particle>, particle_comp> pq (c);
      target.data.neighbors.resize(target.n_particles, pq);

      // For now, just look at particles on this node
      // If n_particles < k, need to look at surrounding nodes
      int nMax = target.n_particles < k ? target.n_particles : k;
      Real drMax2 = 0.0;
      for (int j = 0; j < nMax; j++) {
        if (i == j) continue;
        Vector3D<Real> dr = target.particles()[i].position - target.particles()[j].position;
        if (dr.lengthSquared() > drMax2)
          drMax2 = dr.lengthSquared();
        target.data.neighbors[i].push(target.particles()[j]);
        }

      target.data.drMax2.at(i) = drMax2;
      if (drMax2 > target.data.max_rad)
        target.data.max_rad = sqrt(drMax2);
    }
    target.data.neighborsInited = true;
  }

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
    if (!target.data.neighborsInited) prepNeighbors(target);
    for (int i = 0; i < target.n_particles; i++) {
      // TODO: If particle outside node sphere, continue
      for (int j = 0; j < source.n_particles; j++) {
        // A particle cant be its own neighbor
        if (target.particles()[i].order == source.particles()[j].order)
          continue;

        Real dsq = (target.particles()[i].position - source.particles()[j].position).lengthSquared();
        /*Real rsq = target.data.neighbors[i].top().ball*target.data.neighbors[i].top().ball;
        if (dsq < rsq) {
          target.data.neighbors[i].push(source.particles()[j]);
          if (target.data.neighbors[i].size() >= k) {
            target.data.neighbors[i].pop();
          }
          target.data.neighbors[i].push(source.particles()[j]);
        }*/
      }
    }
  }
};

#endif // PARATREET_DENSITYVISITOR_H_
