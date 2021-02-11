#include "Main.decl.h"
#include "Paratreet.h"
#include "SPHUtils.h"
//#include "PressureVisitor.h"
#include "DensityVisitor.h"

extern bool verify;

namespace paratreet {

  void preTraversalFn(CProxy_Driver<CentroidData>& driver, CProxy_CacheManager<CentroidData>& cache) {
    //cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    driver.loadCache(CkCallbackResumeThread());
  }

  void traversalFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    neighbor_list_collector.reset(CkCallbackResumeThread());
    part.template startUpAndDown<DensityVisitor>();
  }

  void postInteractionsFn(BoundingBox& universe, CProxy_Partition<CentroidData>& part, int iter) {
    // by now, all density requests have gone out
    double start_time = CkWallTimer();
    part.callPerLeafFn(0, CkCallbackResumeThread()); // calculates density, fills requests
    CkWaitQD();
    CkPrintf("Density calculations and sharing: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    start_time = CkWallTimer();
    part.callPerLeafFn(1, CkCallbackResumeThread()); // calculates pressure
    CkPrintf("Pressure calculations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    start_time = CkWallTimer();
    neighbor_list_collector.shareAccelerations();
    CkWaitQD();
    part.callPerLeafFn(2, CkCallbackResumeThread()); // averages pressure
    CkPrintf("Averaging pressures: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    if (iter == 0 && verify) {
      paratreet::outputParticles(universe, part);
    }
  }

  void perLeafFn(int indicator, SpatialNode<CentroidData>& leaf) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      auto& Q = leaf.data.neighbors[pi];
      auto rsq = Q.front().fKey, fBall = std::sqrt(rsq);
      if (indicator == 0) { // sum up the density. requires 0ing of densities
        Real density = 0.;
        for (int i = 0; i < Q.size(); i++) {
          density += Q[i].mass;
        }
        Real r_cubed = fBall * rsq;
        density /= (4.0 / 3.0 * M_PI * r_cubed);
        leaf.setDensity(pi, density);
        nlc->densityFinished(part, leaf);
      }
      else if (indicator == 1) {
        for (int i = 0; i < Q.size(); i++) {
          auto nbrIt = nlc->remote_particles.find(Q[i].pKey);
          CkAssert(nbrIt != nlc->remote_particles.end());
          doSPHCalc(leaf, pi, fBall, nbrIt->second.second);
        }
      }
      else {
        auto it = nlc->remote_particles.find(part.key);
        CkAssert(it != nlc->remote_particles.end());
        auto && otherAcc = it->second.second.acceleration;
        auto newAcc = (otherAcc + part.acceleration) / 2;
        leaf.setAcceleration(pi, newAcc);
        auto otherWork = it->second.second.pressure_dVolume;
        auto newWork = (otherWork + part.pressure_dVolume) / 2;
        leaf.setGasWork(pi, newWork);
      }
    }
  }
}
