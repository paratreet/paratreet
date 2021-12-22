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

  void pup(PUP::er& p) {}

public:
  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    Real r_bucket = target.data.size_sm + target.data.max_rad + 2 * std::numeric_limits<float>::epsilon();
    if (!Space::intersect(source.data.box, target.data.box.center(), r_bucket*r_bucket))
      return false;

    // Check if any of the target balls intersect the source volume
    for (int i = 0; i < target.n_particles; i++) {
      if(Space::intersect(source.data.box, target.particles()[i].position, target.data.pps[i].sphBallSq))
        return true;
    }
    return false;
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      auto rsq = target.data.pps[i].sphBallSq;
      for (int j = 0; j < source.n_particles; j++) {
        const Particle& a = target.particles()[i];
        const Particle& b = source.particles()[j];
        auto dx = b.position - a.position; // points from us to our neighbor
        Real dsq = dx.lengthSquared();
        if (dsq <= rsq) { // if within search radius
          pqSmoothNode pqNew;
          pqNew.pPtr = &b;
          pqNew.fKey = dsq;
          target.data.pps[i].neighbors.push_back(pqNew);
          if (b.density <= 0) {
            CkPrintf("b has key %" PRIx64 " and leaf has type %d\n", b.key, dynamic_cast<const Node<CentroidData>*>(&source)->type);
            CkAbort("b has 0 density");
          }
        }
      }
    }
  }
};

#endif // PARATREET_PRESSUREVISITOR_H_
