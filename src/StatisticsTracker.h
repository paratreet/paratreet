#ifndef PARATREET_STATISTICSTRACKER_H_
#define PARATREET_STATISTICSTRACKER_H_

#include "paratreet.decl.h"
#include "common.h"

class StatisticsTracker : public CBase_StatisticsTracker{
public: // these need to be seen by other local chares
  unsigned long long n_part_ints = 0ull;
  unsigned long long n_node_ints = 0ull;
  unsigned long long n_opens     = 0ull;
  unsigned long long n_closes    = 0ull;
  unsigned n_partition_particles = 0u;
  unsigned n_subtree_particles   = 0u;
  unsigned n_ps_copies           = 0u;
  unsigned n_ps_shares           = 0u;

public:
  void collectAndResetStats(CkCallback cb);
  void collectMetaData (const CkCallback & cb);

  void reset() {
    n_part_ints = n_node_ints = n_opens = n_closes = 0ull;
    n_partition_particles = n_subtree_particles = 0u;
    n_ps_copies = n_ps_shares = 0;
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

  void countCopiesAndShares(int n_c, int n_s) {
    n_ps_copies += n_c;
    n_ps_shares += n_s;
  }
};

#endif // PARATREET_STATISTICSTRACKER_H_
