#ifndef SIMPLE_DRIVER_H_
#define SIMPLE_DRIVER_H_

#include "simple.decl.h"
#include "common.h"
#include <algorithm>
#include <vector>

template <typename Data>
struct Comparator {
  bool operator() (const std::pair<Key, Data>& a, const std::pair<Key, Data>& b) {return a.first < b.first;}
};

template <typename Data>
class Driver : public CBase_Driver<Data> {
public:
  CProxy_CacheManager<Data> cache_manager;
  std::vector<std::pair<Key, Data>> storage;

  Driver(CProxy_CacheManager<Data> cache_manageri) : cache_manager(cache_manageri) {}
  void recvTE(std::pair<Key, Data> param) {
    storage.emplace_back(param);
  }
  void loadCache(int num_share_levels, CkCallback cb) {
    Comparator<Data> comp;
    std::sort(storage.begin(), storage.end(), comp);
    int send_size = storage.size();
    if (num_share_levels >= 0) {
      std::pair<Key, Data> to_search (1 << (LOG_BRANCH_FACTOR * num_share_levels), Data());
      send_size = std::lower_bound(storage.begin(), storage.end(), to_search, comp) - storage.begin();
    }
    cache_manager.recvStarterPack(storage.data(), send_size, cb);
  }
};

#endif // SIMPLE_DRIVER_H_
