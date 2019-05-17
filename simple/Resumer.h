#ifndef SIMPLE_RESUMER_H_
#define SIMPLE_RESUMER_H_

#include "simple.decl.h"
#include "common.h"
#include <unordered_map>
#include <vector>

extern CProxy_Driver<CentroidData> centroid_driver;

template <typename Data>
class Resumer : public CBase_Resumer<Data> {
public:
  CProxy_TreePiece<Data> tp_proxy;
  CacheManager<Data>* cache_local;
  int n_part_ints, n_node_ints;
  std::unordered_map<int, Node<Data>*> nodehash;
  std::unordered_map<Key, std::vector<int>> waiting;

  void destroy() {
#if COUNT_INTRNS
    int intrn_counts [2] = {n_node_ints, n_part_ints};
    CkCallback cb (CkReductionTarget(Driver<CentroidData>, countInts), centroid_driver);
    this->contribute(2 * sizeof(int), &intrn_counts, CkReduction::sum_int, cb);
#endif
    n_part_ints = n_node_ints = 0;
    nodehash.clear();
    waiting.clear();
  }

  Resumer() : n_part_ints(0), n_node_ints(0) {}

  void countInts(int n_ints) {
    if (n_ints > 0 ) n_part_ints += n_ints;
    else n_node_ints -= n_ints;
  }
 
  Node<Data>* fastNodeFind(Key key, bool insert = true) {
    Node<Data>* result = nodehash[key];
    if (result == nullptr && nodehash.count(key / BRANCH_FACTOR)) {
      result = nodehash[key / BRANCH_FACTOR]->findNode(key);
    }
    int bf_cubed = 1 << (LOG_BRANCH_FACTOR * 3);
    if (result == nullptr && nodehash.count(key / bf_cubed)) {
      result = nodehash[key / bf_cubed]->findNode(key);
    }
    if (result == nullptr) {
      result = cache_local->root->findNode(key);
    }
    if (insert) nodehash[key] = result;
    return result;
  }

  void process(Key key) {
    Node<Data>* node = fastNodeFind(key);
    auto it = waiting.find(key);
    if (it == waiting.end()) return;
    for (auto tp_index : it->second) {
      tp_proxy[tp_index].template goDown(key);
    }
    waiting.erase(it);
  }
};

#endif // SIMPLE_RESUMER_H_
