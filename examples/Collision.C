#include "Main.decl.h"
#include "Paratreet.h"
#include "CollisionVisitor.h"
#include "GravityVisitor.h"
#include <climits>

extern bool verify;
extern int iter_start_collision;
extern CProxy_CollisionTracker collision_tracker;

namespace paratreet {

  void preTraversalFn(CProxy_Driver<CentroidData>& driver, CProxy_CacheManager<CentroidData>& cache) {
    //cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    driver.loadCache(CkCallbackResumeThread());
  }

  void traversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, CProxy_Subtree<CentroidData>&, int iter) {
    double start_time = CkWallTimer();
    part.template startDown<GravityVisitor>();
    if (iter >= iter_start_collision) {
      CkWaitQD();
      CkPrintf("Gravity step done, doing fixed ball traversal now: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      part.template startDown<CollisionVisitor>();
    }
  }

  void postTraversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    // Collision is a little funky because were going to edit the mass and position of particles after a collision
    // that means were going to set the mass and position to whatever we want
    // first get minimum distance of any two particles
    double start_time = CkWallTimer();
    part.callPerLeafFn(0, CkCallbackResumeThread());
    CkWaitQD();
    CkPrintf("Collision calculations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    start_time = CkWallTimer();
    part.callPerLeafFn(1, CkCallbackResumeThread());
    CkPrintf("Collision deletions: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
  }

  Real getFixedTimestep() {return 0.0025;}
  Real getTimestep(BoundingBox& universe, Real max_velocity) {
    return getFixedTimestep();
  }

  bool doesCollide(const Particle& a, const Particle& b) {
    auto dx = a.position - b.position;
    auto vRel = a.velocity - b.velocity;
    auto rdotv = dot(dx, vRel);
    Real dx2 = dx.lengthSquared(), vRel2 = vRel.lengthSquared();
    Real sr = 2 * (a.soft + b.soft);
    Real D = sqrt(1 - ((dx2 - (sr*sr))/(rdotv*rdotv))*vRel2);
    Real dt1 = -rdotv/vRel2*(1 + D);
    Real dt2 = -rdotv/vRel2*(1 - D);
    Real dt = std::numeric_limits<Real>::max();
    if (dt1 > 0 && dt1 < dt2) dt = dt1;
    else if (dt2 > 0 && dt2 < dt1) dt = dt2;
    return dt < getFixedTimestep();
  }

  void perLeafFn(int indicator, SpatialNode<CentroidData>& leaf) {
    auto ct = collision_tracker.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      if (part.mass == 0) continue;
      if (indicator == 0) {
        auto && fb = leaf.data.fixed_ball[pi];
        for (int fbi = 0; fbi < fb.size(); fbi++) {
          auto& fbp = fb[fbi];
          if (doesCollide(part, fbp)) {
            collision_tracker[ct->thisIndex].setShouldDelete(part.key);
            collision_tracker.setShouldDelete(fbp.key);
            break;
          }
        }
      }
      else if (indicator == 1) {
        if (ct->should_delete.find(part.key) != ct->should_delete.end()) {
          if (verify) CkPrintf("deleting particle key %" PRIx64" of mass %f, %dth in leaf\n", part.key, part.mass, pi);
          Particle copy_part = part;
          copy_part.mass = 0;
          leaf.changeParticle(pi, copy_part);
        }
      }
    }
  }

}
