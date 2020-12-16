#ifndef PARATREET_RESUMER_H_
#define PARATREET_RESUMER_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Partition.h"
#include <unordered_map>
#include <vector>

extern CProxy_Driver<CentroidData> centroid_driver;

template <typename Data>
class Resumer : public CBase_Resumer<Data> {
public:
  CProxy_Partition<Data> part_proxy;
  CacheManager<Data>* cm_local;
  unsigned long long n_part_ints, n_node_ints, n_opens, n_closes;
  std::vector<std::queue<Node<Data>*>> resume_nodes_per_part;
  std::unordered_map<Key, std::vector<int>> waiting;

  void destroy() {
#if DEBUG
    for (auto && rnq : resume_nodes_per_part) {
      if (!rnq.empty()) CkAbort("did not complete last traversal");
    }
#endif
#if COUNT_INTERACTIONS
    unsigned long long intrn_counts [4] = {n_node_ints, n_part_ints, n_opens, n_closes};
    CkCallback cb (CkReductionTarget(Driver<CentroidData>, countInts), centroid_driver);
    this->contribute(4 * sizeof(unsigned long long), &intrn_counts, CkReduction::sum_ulong_long, cb);
#endif
    n_part_ints = n_node_ints = 0;
    waiting.clear();
  }

  Resumer() : n_part_ints(0), n_node_ints(0) {}

  void countInts(int n_ints) {
    if (n_ints > 0 ) n_part_ints += n_ints;
    else n_node_ints -= n_ints;
  }

  void countOpen(bool should_open) {
    should_open ? n_opens++ : n_closes++;
  }

  void process(Key key) {
    CkAssert(!resume_nodes_per_part.empty());
    auto node = cm_local->root->getDescendant(key);
    auto it = waiting.find(key);
    if (it == waiting.end()) return;
    for (auto part_index : it->second) {
      auto && resume_nodes = resume_nodes_per_part[part_index];
      bool should_resume = resume_nodes.empty();
      resume_nodes.push(node);
      if (should_resume) part_proxy[part_index].goDown(key);
    }
    waiting.erase(it);
  }
};

#endif // PARATREET_RESUMER_H_
