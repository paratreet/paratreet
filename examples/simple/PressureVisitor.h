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

private:
  // note gconst = 1
  static constexpr Real visc = 0.;
  static constexpr Real fDivv_Corrector = 1.; // corrects bias wrt the divergence of velocities // RTFORCE
  static constexpr Real rNorm = 0.;
  static constexpr Real aFac = 1.; // both of these are cosmology
  static constexpr Real vFac = 1.;
  static constexpr Real a = 1.;  // scale factor of the universe
  static constexpr Real H = 1.; // hubble constant //expansion of the universe, dont need it
  static constexpr Real gammam1 = 5.0 / 3.0 - 1.0;

  // poverrho2 = gammam1 / fDensity^2;
  // poverrho2 is pressure over density^2.
  // poverrho2 = gammam1 * p.uPred() / density;
  // poverrho2f is the geometric mean of the two densities
  // density is the sum of the masses (including myself) of the neighborhood
  // does density use the merged neighbor list -> no
  // absMu = measure of how quickly particles are approaching each other

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
      Real ph = 0.5 * rsq; // fBall is the smoothing length and also the search radius
      Real ih2 = 4. / (rsq * rsq); // invH2 in changa
      Real fNorm1 = 0.5 * M_1_PI * ih2 * ih2 / ph; // 1 over sum of all weights
      for (int j = 0; j < source.n_particles; j++) {
        Real dsq = (target.particles()[i].position - source.particles()[j].position).lengthSquared();
        const Particle& a = target.particles()[i], b = source.particles()[j];
        if (dsq < rsq) { // if within search radius
          // divvnorm is a factor that you have to weight
          Real rs1 = dkernelM4(dsq * ih2) * fNorm1 * fDivv_Corrector; // rsq / density
          Real rNorm = rs1 * b.mass; // normalized kernel value
          auto dx = b.position - a.position; // points from us to our neighbor
          auto dv = b.velocity_predicted - a.velocity_predicted;
          Real dvdotdr = vFac * dot(dv, dx) + dsq * H;
          Real PoverRho2 = gammam1 / (a.density * a.density);
          Real acc = rNorm * dvdotdr * (PoverRho2 + visc * 0.5);
          collector->addNeighbor(source.data.home_pe, a.key, b.key, acc);
        }
      }
    }
  }
};

#endif // PARATREET_PRESSUREVISITOR_H_
