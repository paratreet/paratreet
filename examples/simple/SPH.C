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
    part.callPerLeafFn(CkCallbackResumeThread()); // calculates density, fills requests
    CkWaitQD();
    CkPrintf("Density calculations and sharing: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    start_time = CkWallTimer();
    part.callPerLeafFn(CkCallbackResumeThread()); // calculates pressure
    CkPrintf("Pressure calculations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    if (iter == 0 && verify) {
      paratreet::outputParticles(universe, part);
    }
  }

  void perLeafFn(SpatialNode<CentroidData>& leaf) {
    auto nlc = neighbor_list_collector.ckLocalBranch();
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      if (leaf.particles()[pi].density == 0) { // sum up the density. requires 0ing of densities
        auto& Q = leaf.data.neighbors[pi];
        Real density = 0.;
        auto& part = leaf.particles()[pi];
        auto& nlc_neighbors = nlc->our_neighbors[part.key];
        auto& rsq = Q.front().fKey;
        auto ih2 = 4.0/rsq;  // 1/h^2
        for (int i = 0; i < Q.size(); i++) {
          auto& fDist2 = Q[i].fKey;
          auto r2 = fDist2*ih2;
          auto rs = kernelM4(r2);
          density += rs*Q[i].mass;
          nlc_neighbors.emplace(Q[i].pKey, nullptr);
          nlc->our_neighbors[Q[i].pKey].emplace(part.key, &part);
        }
        Real r_cubed = rsq * sqrt(rsq);
        density /= (0.125 * M_PI * r_cubed);
        leaf.setDensity(pi, density);
        nlc->densityFinished(leaf, pi);
        Q.resize(1); // we moved the memory to nlc
      }
      else {
        auto key = leaf.particles()[pi].key;
        auto nlc = neighbor_list_collector.ckLocalBranch();
        Real fBall = std::sqrt(leaf.data.neighbors[pi][0].fKey);
        for (auto && neighbor : nlc->our_neighbors[key]) {
          if (neighbor.second == nullptr) {
            auto nbrIt = nlc->remote_particles.find(neighbor.first);
            CkAssert(nbrIt != nlc->remote_particles.end());
            neighbor.second = &nbrIt->second;
          }
          doSPHCalc(leaf, pi, fBall, *neighbor.second);
        }
      }
    }
  }
}
