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
  using RemoteParticleMap = std::map<Key, std::pair<int, Particle> >;
  RemoteParticleMap remote_particles; // value = (partition_pe, particle)

  void reset(const CkCallback& cb) {
    already_requested.clear();
    requested_to.clear();
    remote_particles.clear();
    this->contribute(cb);
  }
  void makeRequest(int pe, Key key) {
    if (already_requested.insert(key).second) {
      thisProxy[pe].addRequest(thisIndex, key);
    }
  }
  void addRequest(int pe, Key key) {
    requested_to[key].push_back(pe);
  }
  void densityFinished(const Particle& part, const SpatialNode<CentroidData>& leaf) {
    thisProxy[leaf.home_pe].forwardRequest(thisIndex, part);
    // send density, send neighbor list
  }
  void forwardRequest(int pe_home, Particle part) {
    auto && pes_requested = requested_to[part.key];
    for (auto && forward_pe : pes_requested) {
      thisProxy[forward_pe].fillRequest(pe_home, part);
    }
  }
  void fillRequest(int pe_home, Particle part) {
    part.acceleration = Vector3D<Real>(0,0,0);
    part.pressure_dVolume = 0;
    auto pPart = &(remote_particles.emplace(part.key, std::make_pair(pe_home, part)).first->second);
    // update density
  }
  void addAcceleration(Particle part) {
    auto it = remote_particles.find(part.key);
    CkAssert(it != remote_particles.end());
    it->second.second.acceleration += part.acceleration;
    it->second.second.pressure_dVolume += part.pressure_dVolume;
  }
  void shareAccelerations() {
    for (auto && remote_part : remote_particles) {
      if (remote_part.second.first != thisIndex) {
        addAcceleration(remote_part.second.second);
      }
      // give it to someone else
      // then += all the remote contributions
      // then average
    }
  }
};

#endif
