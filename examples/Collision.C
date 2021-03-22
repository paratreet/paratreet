#include "Main.decl.h"
#include "Paratreet.h"
#include "CollisionVisitor.h"
#include "GravityVisitor.h"
#include <climits>

extern bool verify;
extern int iter_start_collision;

namespace paratreet {

  void preTraversalFn(CProxy_Driver<CentroidData>& driver, CProxy_CacheManager<CentroidData>& cache) {
    //cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    driver.loadCache(CkCallbackResumeThread());
  }

  void traversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, CProxy_Subtree<CentroidData>&, int iter) {
    double start_time = CkWallTimer();
    part.template startDown<GravityVisitor>();
    CkWaitQD();
    CkPrintf("Gravity step: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    if (iter >= iter_start_collision) {
      collision_tracker.reset(CkCallback::ignore);
      start_time = CkWallTimer();
      part.template startDown<CollisionVisitor>();
      CkWaitQD();
      CkPrintf("Collision traversal: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      // Collision is a little funky because were going to edit the mass and position of particles after a collision
      // that means were going to set the mass and position to whatever we want
      // first get minimum distance of any two particles
      start_time = CkWallTimer();
      part.callPerLeafFn(0, CkCallbackResumeThread());
      CkWaitQD();
      CkPrintf("Collision calculations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      start_time = CkWallTimer();
      part.callPerLeafFn(1, CkCallbackResumeThread());
      CkPrintf("Collision deletions: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    }
  }

  void postIterationFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    if (iter == 800000 - 1) paratreet::outputTipsy(universe, part);
  }

  Real getFixedTimestep() {return 0.01570796326;}
  Real getTimestep(BoundingBox& universe, Real max_velocity) {
    return getFixedTimestep();
  }

  Real getCollideTime(const Particle& a, const Particle& b) {
    auto dx = a.position - b.position;
    auto vRel = a.velocity - b.velocity + getFixedTimestep() * (a.acceleration - b.acceleration);
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

  void perLeafFn(int indicator, SpatialNode<CentroidData>& leaf) {
    auto ct = collision_tracker.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      if (part.mass == 0) continue;
      if (indicator == 0) {
        auto && fb = leaf.data.fixed_ball[pi];
        Real best_dt = getFixedTimestep();
        int best_dt_i = -1;
        for (int fbi = 0; fbi < fb.size(); fbi++) {
          auto& fbp = fb[fbi];
          auto dt = getCollideTime(part, fbp);
          if (dt < best_dt) {
            best_dt = dt;
            best_dt_i = fbi;
          }
        }
        if (best_dt_i != -1) {
          collision_tracker[ct->thisIndex].setShouldDelete(part.key);
          collision_tracker.setShouldDelete(fb[best_dt_i].key);
        }
      }
      else if (indicator == 1) {
        if (ct->should_delete.find(part.key) != ct->should_delete.end()) {
          CkPrintf("deleting particle order %d of mass %f, %dth in leaf\n", part.order, part.mass, pi);
          Particle copy_part = part;
          copy_part.mass = 0;
          leaf.changeParticle(pi, copy_part);
        }
      }
    }
  }

}
