#include "Main.h"
#include "GravityVisitor.h"

extern bool verify;
extern bool dual_tree;
extern bool periodic;
extern Real theta;

  using namespace paratreet;

  void ExMain::preTraversalFn(ProxyPack<CentroidData>& proxy_pack) {
    //proxy_pack.cache.startParentPrefetch(this->thisProxy, CkCallback::ignore); // MUST USE FOR UPND TRAVS
    //proxy_pack.cache.template startPrefetch<GravityVisitor>(this->thisProxy, CkCallback::ignore);
    proxy_pack.driver.loadCache(CkCallbackResumeThread());
  }

  void ExMain::traversalFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (dual_tree && periodic) CkAbort("Not sure about this -- dual_tree and periodic both set");
    if (dual_tree) proxy_pack.subtree.startDual<GravityVisitor>(GravityVisitor(Vector3D<Real>(0, 0, 0), theta));
    if (!periodic) {
      proxy_pack.partition.template startDown<GravityVisitor>(GravityVisitor(Vector3D<Real>(0, 0, 0), theta));
    } else {
      auto replicas = [&] (int N) {
        auto univ = thread_state_holder.ckLocalBranch()->universe.box.size();
        for (int X = -N; X <= N; X++) {
          for (int Y = -N; Y <= N; Y++) {
            for (int Z = -N; Z <= N; Z++) {
              Vector3D<Real> offset (X * univ.x, Y * univ.y, Z * univ.z);
              proxy_pack.partition.template startDown<GravityVisitor>(GravityVisitor(offset, theta));
            }
          }
        }
      };

      replicas(1); // one box around the box
    }
  }

  void ExMain::postIterationFn(BoundingBox& universe, ProxyPack<CentroidData>& proxy_pack, int iter) {
    if (iter == 0 && verify) {
      paratreet::outputParticleAccelerations(universe, proxy_pack.partition);
    }
    else if (periodic) {
      // do ewald
    }
  }

  Real ExMain::getTimestep(BoundingBox& universe, Real max_velocity) {
    Real universe_box_len = universe.box.greater_corner.x - universe.box.lesser_corner.x;
    return universe_box_len / max_velocity / std::cbrt(universe.n_particles);
  }
