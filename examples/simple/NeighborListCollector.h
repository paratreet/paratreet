#ifndef PARATREET_NEIGHBORLISTCOLLECTOR_H_
#define PARATREET_NEIGHBORLISTCOLLECTOR_H_

#include "common.h"
#include "Vector3D.h"
#include "paratreet.decl.h"

#include <cstring>
#include <cmath>

struct NeighborListCollector : public CBase_NeighborListCollector {
  using Neighborhood = std::vector<std::pair<Key, Vector3D<Real>>>;
  std::map<Key, Neighborhood> local_neighbors;
  std::map<Key, Neighborhood> remote_neighbors;

  void share(const CkCallback& cb) {
    // TODO copy local neighbors to remote neighbors. but how to tell which PE to send?
    this->contribute(cb);
  }
  void reset(const CkCallback& cb) {
    local_neighbors.clear();
    remote_neighbors.clear();
    this->contribute(cb);
  }

  void addNeighbor(Key target, Key source, const Vector3D<Real>& force) {
    local_neighbors[target].emplace_back(source, force);
  } 

};

#endif
