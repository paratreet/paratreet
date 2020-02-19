#ifndef PARATREET_DENSITYVISITOR_H_
#define PARATREET_DENSITYVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include <cmath>
#include <vector>
#include <queue>

struct DensityVisitor {
// in leaf check for not same particle plz
private:
  const int k = 32;
private:
  void prepNeighbors(TargetNode<CentroidData> target) {
    for (int i = 0; i < target.n_particles; i++) {
      particle_comp c (target.particles[i]);
      std::priority_queue<Particle, std::vector<Particle>, particle_comp> pq (c);
      target.data->neighbors.resize(target.n_particles, pq);
    }
  }

public:
  bool node(SourceNode<CentroidData> source, TargetNode<CentroidData> target) {
    if (target.data->neighbors[0].size() < k) return true; // they all fill first k at the same time
    Real dsq = (source.data->getCentroid() - target.data->getCentroid()).lengthSquared();
    Real rsq = (target.data->neighbors[0].top().position - source.data->getCentroid()).lengthSquared();
    // we need to look at the whole bounding box instead of just the centroid

    return (dsq < rsq);
  }

  void leaf(SourceNode<CentroidData> source, TargetNode<CentroidData> target) {
    if (!target.data->neighbors.size()) prepNeighbors(target);
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        target.data->neighbors[i].push(source.particles[j]);
      }
      while (target.data->neighbors[i].size() > k) {
        target.data->neighbors[i].pop();
      }
    }
  }
};

#endif // PARATREET_DENSITYVISITOR_H_
