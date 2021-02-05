#ifndef PARATREET_PRESSUREVISITOR_H_
#define PARATREET_PRESSUREVISITOR_H_
#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include "NeighborListCollector.h"
#include <cmath>

extern CProxy_NeighborListCollector neighbor_list_collector;

struct PressureVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

  static Real dkernelM4(Real ar2) {
    Real adk = sqrt(ar2);
    if (ar2 < 1.0) {
      adk = -3 + 2.25*adk;
    }
    else {
      adk = -0.75*(2.0-adk)*(2.0-adk)/adk;
    }
    return adk;
  }

public:
  static bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
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

  static void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  static void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    auto collector = neighbor_list_collector.ckLocalBranch();
    for (int i = 0; i < target.n_particles; i++) {
      Real rsq = target.data.neighbors[i][0].fKey; // farthest distance, ball radius
      for (int j = 0; j < source.n_particles; j++) {
        const Particle& a = target.particles()[i], b = source.particles()[j];
        auto dx = b.position - a.position; // points from us to our neighbor
        Real dsq = dx.lengthSquared();
        if (dsq < rsq) { // if within search radius
          doSPHCalc(target, i, b);
        }
      }
    }
  }
};

#endif // PARATREET_PRESSUREVISITOR_H_
