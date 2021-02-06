#ifndef PARATREET_NEIGHBORLISTCOLLECTOR_H_
#define PARATREET_NEIGHBORLISTCOLLECTOR_H_

#include "common.h"
#include "Vector3D.h"
#include "paratreet.decl.h"

#include <cstring>
#include <cmath>

struct NeighborListCollector : public CBase_NeighborListCollector {
  std::unordered_set<Key> already_requested;
  std::unordered_map<Key, std::vector<int>> requested_to;
  std::unordered_map<Key, std::map<Key, const Particle*>> our_neighbors;
  std::unordered_map<Key, Particle> remote_particles;

  void reset(const CkCallback& cb) {
    already_requested.clear();
    requested_to.clear();
    our_neighbors.clear();
    remote_particles.clear();
    this->contribute(cb);
  }

  void makeRequest(int pe, Key key) {
    if (pe != thisIndex && already_requested.insert(key).second) {
      thisProxy[pe].addRequest(thisIndex, key);
      requested_to[key].push_back(pe);
    }
  }

  void addRequest(int pe, Key key) {
    requested_to[key].push_back(pe);
  }

  void fillRequest(const SpatialNode<CentroidData>& leaf, int pi) {
    auto & part = leaf.particles()[pi];
    remote_particles.emplace(part.key, part);
    auto && pes_requested = requested_to[part.key];
    if (pes_requested.empty()) return;
    std::vector<Key> nbrs (leaf.data.neighbors[pi].size());
    for (int i = 0; i < leaf.data.neighbors[pi].size(); i++) {
      nbrs[i] = leaf.data.neighbors[pi][i].pKey;
    }
    for (auto && pe : pes_requested) {
      thisProxy[pe].fillRequest(part, nbrs);
      // send density, send neighbor list
    }
    pes_requested.clear();
  }
  void fillRequest(Particle part, const std::vector<Key>& neighbors) {
    auto && pes_requested = requested_to[part.key];
    for (auto && forward_pe : pes_requested) {
      thisProxy[forward_pe].fillRequest(part, neighbors);
    }
    pes_requested.clear();
    auto pPart = &(remote_particles.emplace(part.key, part).first->second);
    for (auto n : neighbors) {
      our_neighbors[n].emplace(part.key, pPart);
    }
    // update density, augment neighborhood of key
  }

};

#endif
