#include "Main.h"
#include "Paratreet.h"
#include "SPHUtils.h"
#include "PressureVisitor.h"
#include "DensityVisitor.h"

extern bool verify;
extern Real max_timestep;
using namespace paratreet;

PARATREET_REGISTER_PER_LEAF_FN(DensityFn, CentroidData, (
  [](SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {
    leaf.data.max_rad = 0;
    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      auto& Q = leaf.data.pps[pi].neighbors;
      auto rsq = Q[0].fKey, fBall = std::sqrt(rsq);
      // sum up the density. requires 0ing of densities
      Real density = 0.;
      auto ih2 = 4.0 / rsq;  // 1/h^2
      for (int i = 0; i < Q.size(); i++) {
        auto& fDist2 = Q[i].fKey;
        auto r2 = fDist2 * ih2;
        auto rs = kernelM4(r2);
        density += rs * Q[i].pPtr->mass;
      }
      Real r_cubed = rsq * fBall;
      density /= (0.125 * M_PI * r_cubed);
      auto copy_part = part;
      copy_part.density = density;
      leaf.changeParticle(pi, copy_part);
      leaf.data.pps[pi].sphBallSq = rsq;
      leaf.data.max_rad = std::max(leaf.data.max_rad, fBall);
      Q.clear();
    }
  }));

PARATREET_REGISTER_PER_LEAF_FN(ForceFn, CentroidData, (
  [](SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {
    auto tls = thread_state_holder.ckLocalBranch();

    for (int pi = 0; pi < leaf.n_particles; pi++) {
      auto& part = leaf.particles()[pi];
      auto& Q = leaf.data.pps[pi].neighbors; // this might have k+1 or even k+2
      auto rsq = leaf.data.pps[pi].sphBallSq, fBall = std::sqrt(rsq);
      // Calculate divv correction
      auto ih2 = 4.0/rsq;  // 1/h^2
      double divvi = 0;
      double divvj = 0;
      for (int i = 0; i < Q.size(); i++) {
        auto& fDist2 = Q[i].fKey;
        auto r2 = fDist2*ih2;
        Real rs1 = dkernelM4(r2);
        auto& q = *(Q[i].pPtr);
        rs1 *= fDist2*q.mass;
        divvi += rs1;
        divvj += rs1/q.density;
      }
      divvi /= part.density;
      auto fDivv_Corrector = (divvj != 0.0 ? divvi/divvj : 1.0);

      for (int i = 0; i < Q.size(); i++) {
        doSPHCalc(leaf, pi, fBall, tls, *Q[i].pPtr, fDivv_Corrector);
      }
    }
  }));

  void ExMain::preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
    //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
  }

  void ExMain::traversalFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    double start_time = CkWallTimer();
    proxy_pack.partition.template startUpAndDown<DensityVisitor>(DensityVisitor());
    CkWaitQD();
    CkPrintf("K-nearest neighbors traversal: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    start_time = CkWallTimer();
    // by now, all density requests have gone out
    proxy_pack.partition.callPerLeafFn(
      PARATREET_PER_LEAF_FN(DensityFn, CentroidData), // calculates density, fills requests
      CkCallbackResumeThread()
    );
    CkWaitQD();
    CkPrintf("Density calculations and sharing: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    proxy_pack.cache.resetCachedParticles(proxy_pack.partition);
    CkWaitQD();
    proxy_pack.partition.template startDown<PressureVisitor>(PressureVisitor());
    CkWaitQD();
    start_time = CkWallTimer();
    proxy_pack.partition.callPerLeafFn(
      PARATREET_PER_LEAF_FN(ForceFn, CentroidData),  // calculates pressure
      CkCallbackResumeThread()
    );
    CkPrintf("Pressure calculations: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
    start_time = CkWallTimer();
    thread_state_holder.applyAccumulatedOpposingEffects<CentroidData>(proxy_pack.partition);
    CkWaitQD();
    CkPrintf("Averaging pressures: %.3lf ms\n", (CkWallTimer() - start_time) * 1000);
  }

  void ExMain::postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticleAccelerations(universe, proxy_pack.partition);
    }
  }

  Real ExMain::getTimestep(BoundingBox& universe, Real max_velocity) {
    return max_timestep;
  }
