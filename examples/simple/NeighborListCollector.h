#ifndef PARATREET_NEIGHBORLISTCOLLECTOR_H_
#define PARATREET_NEIGHBORLISTCOLLECTOR_H_

#include "common.h"
#include "Vector3D.h"
#include "paratreet.decl.h"

#include <cstring>
#include <cmath>

struct NeighborListCollector : public CBase_CountManager {
  std::map<Key, std::vector<Key>> local_neighbors;
  std::map<Key, std::vector<Key>> all_neighbors;

  void share(const CkCallback& cb) {
  }

  void addNeighbor(Key target, Key source) {
    local_neighbors[target].push_back(source);
  } 

};

#endif
