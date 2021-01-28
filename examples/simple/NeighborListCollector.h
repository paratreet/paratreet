#ifndef PARATREET_NEIGHBORLISTCOLLECTOR_H_
#define PARATREET_NEIGHBORLISTCOLLECTOR_H_

#include "common.h"
#include "Vector3D.h"
#include "paratreet.decl.h"

#include <cstring>
#include <cmath>

struct NeighborListCollector : public CBase_NeighborListCollector {
  struct ToShare {
    std::vector<Key> keys_sources;
    std::vector<Key> keys_targets;
    std::vector<Real> works;
  };
  std::map<int, ToShare> to_send;
  using Neighborhood = std::map<Key, Real>;
  using NeighborhoodMap = std::map<Key, Neighborhood>;
  NeighborhoodMap neighbors;

  void share() {
    // copy local neighbors to remote neighbors
    for (auto && pe_send : to_send) {
      auto && s = pe_send.second;
      this->thisProxy[pe_send.first].shareNeighbors(
        s.keys_sources, s.keys_targets, s.works);
    }
    to_send.clear();
  }

  void shareNeighbors(std::vector<Key> keys_sources,
       std::vector<Key> keys_targets, std::vector<Real> works)
  {
    for (int i = 0; i < keys_sources.size(); i++) {
      neighbors[keys_sources[i]].emplace(keys_targets[i], works[i]);
    }
  }
  void reset(const CkCallback& cb) {
    neighbors.clear();
    this->contribute(cb);
  }

  void addNeighbor(int home_pe, Key target, Key source, Real work) {
    neighbors[target].emplace(source, work);
    if (home_pe != CkMyPe()) {
      auto && to_share = to_send[home_pe];
      to_share.keys_sources.push_back(source);
      to_share.keys_targets.push_back(target);
      to_share.works.push_back(work);
    }
  } 

};

#endif
