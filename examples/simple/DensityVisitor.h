#ifndef PARATREET_DENSITYVISITOR_H_
#define PARATREET_DENSITYVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include "NeighborListCollector.h"
#include <cmath>
#include <vector>
#include <queue>

extern CProxy_NeighborListCollector neighbor_list_collector;

struct DensityVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

// in leaf check for not same particle plz
private:
  static constexpr const int k = 32;

public:
  static bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    Real r_bucket = target.data.size_sm + target.data.max_rad;
    if (!Space::intersect(source.data.box, target.data.box.center(), r_bucket*r_bucket))
      return false;
    // Check if any of the target balls intersect the source volume
    for (int i = 0; i < target.n_particles; i++) {
      if(Space::intersect(source.data.box, target.particles()[i].position, target.data.neighbors[i][0].fKey))
        return true;
    }
    return false;
  }

  static void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  static void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int i = 0; i < target.n_particles; i++) {
      CkVec<pqSmoothNode> &Q = target.data.neighbors[i];
      for (int j = 0; j < source.n_particles; j++) {
        const auto& sp = source.particles()[j]; //source particle
        Vector3D<Real> dr = target.particles()[i].position - sp.position;
        // Remove the most distant neighbor if this one is closer and the list is full
        if (Q.size() == k) {
          if (Q[0].fKey > dr.lengthSquared()) {
            std::pop_heap(&(Q[0]) + 0, &(Q)[0] + k);
            Q.resize(Q.size()-1); // pop off last
          }
        }
        // Add the particle to the neighbor list if it isnt filled up
        if (Q.size() < k) {
          nlc->makeRequest(source.data.home_pe, sp.key);
          pqSmoothNode pqNew;
          pqNew.mass = sp.mass;
          pqNew.fKey = dr.lengthSquared();
          pqNew.pKey = sp.key;
          pqNew.pPtr = &sp;
          Q.push_back(pqNew);
          std::push_heap(&(Q)[0] + 0, &(Q)[0] + Q.size());

          Real max_rad = dr.length();
          if (max_rad > target.data.max_rad)
            target.data.max_rad = max_rad;
        }
      }
    }
  }
};

#endif // PARATREET_DENSITYVISITOR_H_
