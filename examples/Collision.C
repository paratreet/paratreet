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
    if (iter % 100000 == 0) paratreet::outputTipsy(universe, part);
  }

  Real getTimestep(BoundingBox& universe, Real max_velocity) {
    return 0.01570796326;
  }

  void perLeafFn(int indicator, SpatialNode<CentroidData>& leaf) {
    auto ct = collision_tracker.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      if (part.mass == 0) continue;
      if (indicator == 0) {
        auto best_dt = leaf.data.best_dt[pi].first;
        if (best_dt < 0.01570796326) {
          auto& partB = leaf.data.best_dt[pi].second;
          auto& posA = part.position;
          auto& posB = partB.position;
          auto& velA = part.velocity;
          auto& velB = partB.velocity;
          CkPrintf("deleting particles of order %d and %d that collide at dt %lf. First has position (%lf, %lf, %lf) velocity (%lf, %lf, %lf). Second has position (%lf, %lf, %lf) velocity (%lf, %lf, %lf)\n", part.order, partB.order, best_dt, posA.x, posA.y, posA.z, velA.x, velA.y, velA.z, posB.x, posB.y, posB.z, velB.x, velB.y, velB.z);
          collision_tracker[ct->thisIndex].setShouldDelete(part.key);
          collision_tracker.setShouldDelete(partB.key);
        }
      }
      else if (indicator == 1) {
        if (ct->should_delete.find(part.key) != ct->should_delete.end()) {
          Particle copy_part = part;
          copy_part.mass = 0;
          leaf.changeParticle(pi, copy_part);
        }
      }
    }
  }

}
