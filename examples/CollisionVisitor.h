#ifndef PARATREET_COLLISIONVISITOR_H_
#define PARATREET_COLLISIONVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include <cmath>
#include <vector>
#include <queue>
#include "CollisionTracker.h"

extern CProxy_CollisionTracker collision_tracker;

struct CollisionVisitor {
public:
  static constexpr const bool CallSelfLeaf = true;

  static Real getCollideTime(const Particle& a, const Particle& b) {
    auto dx = a.position - b.position;
    auto vRel = a.velocity - b.velocity + (0.01570796326 / 2.) * (a.acceleration - b.acceleration); // this is kinda wrong cause accelerations are not updated properly
    auto rdotv = dot(dx, vRel);
    Real dx2 = dx.lengthSquared(), vRel2 = vRel.lengthSquared();
    Real sr = 2 * (a.soft + b.soft);
    auto dist = (dx2 - sr * sr);
    Real inside = dist/(rdotv*rdotv) * vRel2;
    Real dt = std::numeric_limits<Real>::max();
    //CkPrintf("rdotv is %lf, inside is %lf, dx2 is %lf, vRel2 is %lf, sr is %lf\n", rdotv, inside, dx2, vRel2, sr);
    if (inside <= 1) {
      Real D = sqrt(1 - inside);
      Real dt1 = -rdotv/vRel2*(1 + D);
      Real dt2 = -rdotv/vRel2*(1 - D);
      //CkPrintf("D is %lf, dt1 is %lf, dt2 is %lf\n", D, dt1, dt2);
      if (dt1 > 0 && dt1 < dt2) dt = dt1;
      else if (dt2 > 0 && dt2 < dt1) dt = dt2;
    }
    return dt;
  }

// in leaf check for not same particle plz
public:
  static bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    Real r_bucket = target.data.size_sm + target.data.max_rad;
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

  static void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  static void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    for (int i = 0; i < target.n_particles; i++) {
      for (int j = 0; j < source.n_particles; j++) {
        auto& sp = source.particles()[j];
        auto& tp = target.particles()[i];
        Real dsq = (tp.position - sp.position).lengthSquared();
        Real rsq = tp.ball * tp.ball;
        if (dsq < rsq && sp.order != tp.order) {
          Real dt = getCollideTime(tp, sp);
          if (dt < target.data.best_dt[i].first) {
            target.data.best_dt[i] = std::make_pair(dt, sp);
          }
        }
      }
    }
  }

private:
};

#endif // PARATREET_COLLISIONVISITOR_H_
