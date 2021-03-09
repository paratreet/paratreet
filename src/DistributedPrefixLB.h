/**
 * Author: gplkrsh2@illinois.edu (Harshitha Menon)
 *
 * A distributed load balancer which consists of two phases.
 * 1. Information propagation
 * 2. Probabilistic transfer of load
 *
 * Information propagation is done using Gossip protocol where the information
 * about the underloaded processors in the system is spread to the overloaded
 * processors.
 * Once the overloaded processors receive the partial information about the
 * underloaded processors in the system, it does probabilistic load transfer.
 * The probability of a PE receiving load is inversely proportional to its
 * current load.
*/

#ifndef _DISTPREFIXLB_H_
#define _DISTPREFIXLB_H_

#include "DistBaseLB.h"
#include "DistributedPrefixLB.decl.h"

#include "ckheap.h"

#include <vector>

void CreateDistributedPrefixLB();

class DistributedPrefixLB : public CBase_DistributedPrefixLB {
public:
  DistributedPrefixLB(const CkLBOptions &);
  DistributedPrefixLB(CkMigrateMessage *m);
  static void initnodeFn(void);
  void prefixStep();
  void prefixPassValue(int, int);
  void donePrefix();

private:
  // Prefix related
  std::vector<LDObjStats> obj_map;
  std::vector<int> size_map;

  int total_iter;
  int prefix_stage;
  int prefix_sum;
  double prefix_start_time;
  double prefix_end_time;
  std::vector<char> flag_buf;
  std::vector<int> value_buf;
  int my_particle_size_sum = 0;
  int total_particle_size = 0;
  double total_partition_load = 0.0;
  double avg_size;
  int max_size;
  double size_ratio;
  int total_migrates;

  // Constant variables for configuring how the strategy works
  bool kUseAck;
  int kPartialInfoCount;
  int kMaxTrials;
  int kMaxGossipMsgCount;
  int kMaxObjPickTrials;
  int kMaxPhases;
  double kTargetRatio;
  double start_time;

  const DistBaseLB::LDStats* my_stats;

  void InitLB(const CkLBOptions &);
  void Strategy(const DistBaseLB::LDStats* const stats);
  void prefixReset();
  void prefixCleanup();
  void doPrefix();
  void PackAndSendMigrateMsgs();
};

#endif /* _DistributedPrefixLB_H_ */
