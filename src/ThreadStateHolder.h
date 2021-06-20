#ifndef PARATREET_THREADSTATEHOLDER_H_
#define PARATREET_THREADSTATEHOLDER_H_

#include "paratreet.decl.h"
#include "common.h"

class ThreadStateHolder : public CBase_ThreadStateHolder {
public: // these need to be seen by other local chares
  unsigned long long n_part_ints = 0ull;
  unsigned long long n_node_ints = 0ull;
  unsigned long long n_opens     = 0ull;
  unsigned long long n_closes    = 0ull;
  unsigned n_partition_particles = 0u;
  unsigned n_subtree_particles   = 0u;

  BoundingBox universe;

public:
  void collectAndResetStats(CkCallback cb);
  void collectMetaData (const CkCallback & cb);

  void setUniverse(BoundingBox universe_) {
    universe = universe_;
  }

  void reset() {
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
};

#endif // PARATREET_THREADSTATEHOLDER_H_
