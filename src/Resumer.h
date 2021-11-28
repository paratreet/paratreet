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
  std::vector<std::vector<std::queue<Node<Data>*>>> all_resume_nodes;
  std::unordered_map<Key, std::vector<std::pair<int, int>>> waiting;
  bool use_subtree = false;

  void reset() {
#if DEBUG
    for (auto& trav : all_resume_nodes) {
      for (auto && rnq : trav) {
        if (!rnq.empty()) CkAbort("did not complete last traversal");
      }
    }
    CkAssert(waiting.empty()); // should have gotten rid of them
#endif
  }

  void process(Key key) {
    CkAssert(!all_resume_nodes.empty());
    auto node = cm_local->root->getDescendant(key);
    CkAssert(node && node->key == key);
    auto it = waiting.find(key);
    if (it == waiting.end()) return;
    for (auto pair : it->second) { // (trav_idx, part_idx)
      auto && resume_nodes = all_resume_nodes[pair.first][pair.second];
      bool should_resume = resume_nodes.empty();
      resume_nodes.push(node);
      if (should_resume) {
        if (use_subtree) subtree_proxy[pair.second].goDown(pair.first);
        else part_proxy[pair.second].goDown(pair.first);
      }
    }
    waiting.erase(it);
  }
};

#endif // PARATREET_RESUMER_H_
