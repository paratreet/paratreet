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
    target.data.neighbors.resize(target.n_particles);

    for (int i = 0; i < target.n_particles; i++) {
      particle_comp c (target.particles()[i]);
      std::priority_queue<Particle, std::vector<Particle>, particle_comp> pq (c);

      // For now, just look at particles on this node
      // If n_particles < k, need to look at surrounding nodes
      int nMax = target.n_particles < k ? target.n_particles : k;
      Real drMax2 = 0.001;
      for (int j = 0; j < nMax; j++) {
        if (i == j) continue;
        Vector3D<Real> dr = target.particles()[i].position - target.particles()[j].position;
        
        if (dr.lengthSquared() > drMax2)
          drMax2 = dr.lengthSquared();
        pq.push(target.particles()[j]);
        }

      target.data.drMax2[i] = drMax2;
      if (drMax2 > target.data.max_rad)
        target.data.max_rad = sqrt(drMax2);
      target.data.neighbors[i] = pq;
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
      // TODO: Is ball what we want here?
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

      // For now, skip over buckets with only 1 particle
      // This will be fixed once we construct PQs correctly
      if (target.n_particles <= 1) continue;

      // If particle outside node sphere, continue
      Real r_bucket = source.data.size_sm + source.data.max_rad;
      Vector3D<Real> drBucket = source.data.box.center() - target.particles()[i].position;
      if (r_bucket < drBucket.length())
        continue;
      for (int j = 0; j < source.n_particles; j++) {
        // A particle cant be its own neighbor
        if (target.particles()[i].order == source.particles()[j].order)
          continue;
        Real rOld2 = (target.data.neighbors[i].top().position - source.particles()[j].position).lengthSquared();
        Vector3D<Real> dr = target.particles()[i].position - source.particles()[j].position;
       
        if (dr.lengthSquared() < rOld2) {
          if (target.data.neighbors[i].size() >= k) target.data.neighbors[i].pop();
          target.data.neighbors[i].push(source.particles()[j]);
          target.data.drMax2[i] = (target.data.neighbors[i].top().position - source.particles()[j].position).lengthSquared();
          }
      }
    }
  }
};

#endif // PARATREET_DENSITYVISITOR_H_
