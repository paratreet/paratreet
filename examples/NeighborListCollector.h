#ifndef PARATREET_NEIGHBORLISTCOLLECTOR_H_
#define PARATREET_NEIGHBORLISTCOLLECTOR_H_

#include "common.h"
#include "Vector3D.h"
#include "paratreet.decl.h"

#include <cstring>
#include <cmath>

struct NeighborListCollector : public CBase_NeighborListCollector {
  std::unordered_set<Key> already_requested;
  std::unordered_map<Key, std::vector<int>> requested_to; // value = pe
  struct HomeState {
    int subtree_home_pe = -1;
    int partition_home_pe = -1;
    HomeState(int s, int p) : subtree_home_pe(s), partition_home_pe(p) {}
  };
  using RemoteParticleMap = std::map<Key, std::pair<HomeState, Particle> >;
  RemoteParticleMap remote_particles;

  // explanation: NLC is complicated because particles can be processed on one pe
  // but live in a Subtree on another pe
  // remote_particles map helps you map particles to their partition_pe
  // this way, you can send the flip-side forces to them for averaging.
  void reset(const CkCallback& cb) {
    already_requested.clear();
    requested_to.clear();
    remote_particles.clear();
    this->contribute(cb);
  }
  // only used in RTFORCE
  void forwardRequest(int pe_home, Particle part) {
    auto && pes_requested = requested_to[part.key];
    for (auto && forward_pe : pes_requested) {
      thisProxy[forward_pe].fillRequest(pe_home, part);
    }
  }
  void fillRequest(int pe_home, Particle part) {
    part.acceleration = Vector3D<Real>(0,0,0);
    part.pressure_dVolume = 0;
    HomeState home_state {-1, -1};
    remote_particles.emplace(part.key, std::make_pair(home_state, part));
  }
  void makeRequest(int pe, Key key) {
    if (already_requested.insert(key).second) {
      thisProxy[pe].addRequest(thisIndex, key);
    }
  }
  void addRequest(int pe, Key key) {
    requested_to[key].push_back(pe);
  }

  // in the simple Regime, we use densityFinished to forward densities from
  // the particles partition pe to its subtree pe, which is its leaf.home_pe
  void densityFinished(const Particle& part, const SpatialNode<CentroidData>& leaf) {
    // thisProxy[leaf.home_pe].forwardRequest(thisIndex, part); // needed for RTFORCE
    thisProxy[leaf.home_pe].savePartitionHome(thisIndex, part);
    // send density forward
  }
  void saveSubtreeHome(int subtree_home_pe, Particle part) {
    part.acceleration = Vector3D<Real>(0,0,0);
    part.pressure_dVolume = 0;
    HomeState state {subtree_home_pe, -1};
    remote_particles.emplace(part.key, std::make_pair(state, part));
  }
  void savePartitionHome(int partition_home_pe, Particle part) {
    part.acceleration = Vector3D<Real>(0,0,0);
    part.pressure_dVolume = 0;
    HomeState state {-1, partition_home_pe};
    auto out = remote_particles.emplace(part.key, std::make_pair(state, part));
    out.first->second.first.partition_home_pe = partition_home_pe;
  }
  void shareAccelerations() {
    for (auto && remote_part : remote_particles) {
      auto home_state = remote_part.second.first;
      if (home_state.partition_home_pe > -1) {
        if (home_state.partition_home_pe != thisIndex) { // dont want to double add
          thisProxy[home_state.partition_home_pe].addAcceleration(remote_part.second.second);
        }
      }
      else {
        thisProxy[home_state.subtree_home_pe].forwardAcceleration(remote_part.second.second);
      }
      // give it to someone else
      // then += all the remote contributions
      // then average
    }
  }
  void addAcceleration(Particle part) {
    auto it = remote_particles.find(part.key);
    CkAssert(it != remote_particles.end());
    it->second.second.acceleration += part.acceleration;
    it->second.second.pressure_dVolume += part.pressure_dVolume;
  }
  void forwardAcceleration(Particle part) {
    auto it = remote_particles.find(part.key);
    CkAssert(it != remote_particles.end());
    auto pHome = it->second.first.partition_home_pe;
    CkAssert(pHome > -1);
    thisProxy[pHome].addAcceleration(part);
  }

};

#endif
