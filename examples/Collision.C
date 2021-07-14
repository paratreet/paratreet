#include "Main.h"
#include "Paratreet.h"
#include "CollisionVisitor.h"
#include "GravityVisitor.h"
#include <climits>

extern bool verify;
extern int iter_start_collision;

  using namespace paratreet;

  void ExMain::preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
    //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
  }

  void ExMain::traversalFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    proxy_pack.partition.template startDown<GravityVisitor<0,0,0>>();
  }

  void ExMain::postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    FirstCollisionFn first;
    SecondCollisionFn second;
    proxy_pack.partition.callPerLeafFn(PerLeafRef(second), CkCallbackResumeThread());
    if (iter % 10000 == 0) paratreet::outputTipsy(universe, proxy_pack.partition);
    if (iter >= iter_start_collision) {
      proxy_pack.cache.resetCachedParticles(CkCallbackResumeThread());
      double start_time = CkWallTimer();
      proxy_pack.partition.template startDown<CollisionVisitor>();
      CkWaitQD();
      CkPrintf("Collision traversal: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
      // Collision is a little funky because were going to edit the mass and position of particles after a collision
      // that means were going to set the mass and position to whatever we want
      // first get minimum distance of any two particles
      start_time = CkWallTimer();
      proxy_pack.partition.callPerLeafFn(PerLeafRef(first), CkCallbackResumeThread());
      CkWaitQD();
      CkPrintf("Collision calculations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    }
  }

  Real ExMain::getTimestep(BoundingBox& universe, Real max_velocity) {
    return 0.01570796326;
  }

  #include "NullSPH.h"
