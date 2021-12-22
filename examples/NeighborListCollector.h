#ifndef PARATREET_NEIGHBORLISTCOLLECTOR_H_
#define PARATREET_NEIGHBORLISTCOLLECTOR_H_

#include "common.h"
#include "Vector3D.h"
#include "paratreet.decl.h"

#include <cstring>
#include <cmath>

struct NeighborListCollector : public CBase_NeighborListCollector {
  struct HomeState {
    int subtree_home_pe = -1;
    int partition_home_pe = -1;
    HomeState() = default;
    HomeState(int s, int p) : subtree_home_pe(s), partition_home_pe(p) {}
  };
  using RemoteParticleMap = std::map<Key, std::pair<HomeState, Particle> >;
  RemoteParticleMap remote_particles;

  // explanation: NLC is complicated because particles can be processed on one pe
  // but live in a Subtree on another pe
  // remote_particles map helps you map particles to their partition_pe
  // this way, you can send the flip-side forces to them for averaging.
  void reset(const CkCallback& cb) {
    remote_particles.clear();
    this->contribute(cb);
  }
  // only used in RTFORCE
  void savePartitionHome(int pe_home, Key key) {
    auto& rp = remote_particles[key];
    rp.first.partition_home_pe = pe_home;
    rp.second.key = key;
  }

  // in the simple Regime, we use densityFinished to forward densities from
  // the particles partition pe to its subtree pe, which is its leaf.home_pe
  void densityFinished(const Particle& part, const SpatialNode<CentroidData>& leaf) {
    if (leaf.home_pe != CkMyPe()) {
      thisProxy[leaf.home_pe].savePartitionHome(thisIndex, part.key); // needed for RTFORCE
    }
    // send density forward
  }
  void saveSubtreeHome(int subtree_home_pe, Particle part) {
    auto& rp = remote_particles[part.key];
    rp.first.subtree_home_pe = subtree_home_pe;
  }
  void shareAccelerations() {
    std::map<int, std::vector<Particle>> addAccelerations, forwardAccelerations;
    for (auto && remote_part : remote_particles) {
      auto home_state = remote_part.second.first;
      if (home_state.partition_home_pe > -1) {
        if (home_state.partition_home_pe != thisIndex) { // dont want to double add
          addAccelerations[home_state.partition_home_pe].push_back(remote_part.second.second);
        }
      }
      else {
        forwardAccelerations[home_state.subtree_home_pe].push_back(remote_part.second.second);
      }
      // give it to someone else
      // then += all the remote contributions
      // then average
    }
    for (auto && fa : forwardAccelerations) {
      thisProxy[fa.first].forwardAccelerations(fa.second);
    }
    for (auto && fa : addAccelerations) {
      thisProxy[fa.first].addAccelerations(fa.second);
    }
  }
  void addAccelerations(const std::vector<Particle>& parts) {
    for (auto& part : parts) {
      auto it = remote_particles.find(part.key);
      CkAssert(it != remote_particles.end());
      it->second.second.acceleration += part.acceleration;
      it->second.second.pressure_dVolume += part.pressure_dVolume;
    }
  }
  void forwardAccelerations(const std::vector<Particle>& parts) {
    std::map<int, std::vector<Particle>> forwardAccelerations;
    for (auto& part : parts) {
      auto it = remote_particles.find(part.key);
      CkAssert(it != remote_particles.end());
      auto pHome = it->second.first.partition_home_pe;
      CkAssert(pHome > -1);
      forwardAccelerations[pHome].push_back(part);
    }
    for (auto && fa : forwardAccelerations) {
      thisProxy[fa.first].addAccelerations(fa.second);
    }
  }
};

#endif
