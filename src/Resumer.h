#ifndef PARATREET_RESUMER_H_
#define PARATREET_RESUMER_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Partition.h"
#include <unordered_map>
#include <vector>

template <typename Data>
class Resumer : public CBase_Resumer<Data> {
public: // these need to be seen by other local chares
  CProxy_Partition<Data> part_proxy;
  CacheManager<Data>* cm_local;
  std::vector<std::queue<Node<Data>*>> resume_nodes_per_part;
  std::unordered_map<Key, std::vector<int>> waiting;

private: // stats
  unsigned long long n_part_ints = 0ull;
  unsigned long long n_node_ints = 0ull;
  unsigned long long n_opens     = 0ull;
  unsigned long long n_closes    = 0ull;
  unsigned n_partition_particles = 0u;
  unsigned n_subtree_particles   = 0u;

public:
  void collectAndResetStats(CkCallback cb) {
#if COUNT_INTERACTIONS
     CkPrintf("%lu particles on pe %d\n", n_partition_particles, CkMyPe());
     unsigned long long intrn_counts [4] = {n_node_ints, n_part_ints, n_opens, n_closes};
     CkPrintf("on PE %d: %llu node-particle interactions, %llu bucket-particle interactions %llu node opens, %llu node closes\n", CkMyPe(), intrn_counts[0], intrn_counts[1], intrn_counts[2], intrn_counts[3]);
     this->contribute(4 * sizeof(unsigned long long), &intrn_counts, CkReduction::sum_ulong_long, cb);
#endif
     reset();
  }

  void collectMetaData (const CkCallback & cb) {
    int nParticles = n_subtree_particles;
    const size_t numTuples = 1;
    CkReduction::tupleElement tupleRedn[] = {
      CkReduction::tupleElement(sizeof(nParticles), &nParticles, CkReduction::max_int)
    };
    CkReductionMsg * msg = CkReductionMsg::buildFromTuple(tupleRedn, numTuples);
    msg->setCallback(cb);
    this->contribute(msg);
  }

  void reset() {
#if DEBUG
    for (auto && rnq : resume_nodes_per_part) {
      if (!rnq.empty()) CkAbort("did not complete last traversal");
    }
    CkAssert(waiting.empty()); // should have gotten rid of them
#endif
    n_part_ints = n_node_ints = n_opens = n_closes = 0ull;
    n_partition_particles = n_subtree_particles = 0u;
  }

  void countLeafInts(int n_ints) {
    n_part_ints += n_ints;
  }

  void countNodeInts(int n_ints) {
    n_node_ints += n_ints;
  }

  void countOpen(bool should_open) {
    should_open ? n_opens++ : n_closes++;
  }

  void countPartitionParticles(int n_parts) {
    n_partition_particles += n_parts;
  }

  void countSubtreeParticles(int n_parts) {
    n_subtree_particles += n_parts;
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
      if (should_resume) part_proxy[part_index].goDown();
    }
    waiting.erase(it);
  }
};

#endif // PARATREET_RESUMER_H_
