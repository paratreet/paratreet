#ifndef PARATREET_RESUMER_H_
#define PARATREET_RESUMER_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Partition.h"

#include <unordered_map>
#include <vector>
#include <queue>

template <typename Data>
class Resumer : public CBase_Resumer<Data> {
public: // these need to be seen by other local chares
  CProxy_Partition<Data> part_proxy;
  CProxy_Subtree<Data> subtree_proxy;
  CacheManager<Data>* cm_local;
  std::vector<std::queue<Node<Data>*>> resume_nodes_per_part;
  std::unordered_map<Key, std::vector<int>> waiting;
  bool use_subtree = false;

  void reset() {
#if DEBUG
    for (auto && rnq : resume_nodes_per_part) {
      if (!rnq.empty()) CkAbort("did not complete last traversal");
    }
    CkAssert(waiting.empty()); // should have gotten rid of them
#endif
  }

  void process(Key key) {
    CkAssert(!resume_nodes_per_part.empty());
    auto node = cm_local->root->getDescendant(key);
    CkAssert(node && node->key == key);
    auto it = waiting.find(key);
    if (it == waiting.end()) return;
    for (auto part_index : it->second) {
      auto && resume_nodes = resume_nodes_per_part[part_index];
      bool should_resume = resume_nodes.empty();
      resume_nodes.push(node);
      if (should_resume) {
        if (use_subtree) subtree_proxy[part_index].goDown();
        else part_proxy[part_index].goDown();
      }
    }
    waiting.erase(it);
  }
};

#endif // PARATREET_RESUMER_H_
