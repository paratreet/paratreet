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
#include "LBCommon.h"

#include <vector>
using LBCommon::LBUserData;
using LBCommon::LBCompareStats;

void CreateDistributedPrefixLB();

class DistributedPrefixLB : public CBase_DistributedPrefixLB {
public:
  DistributedPrefixLB(const CkLBOptions &);
  DistributedPrefixLB(CkMigrateMessage *m);
  static void initnodeFn(void);
  void getOtherPECentroids(std::vector<Vector3D<Real>>data, Vector3D<Real> pe_centroid_, int in_pe);
  void send_migrate_cts(int, int, int);
  void prefixStep();
  void prefixPassValue(int, int);
  void donePrefix();
  void sendPrefixSummary(int, int);

private:
  int my_pe;
  bool balance_partition = false;
  LBMigrateMsg * final_migration_msg;

  int total_prefix_moves = 0;
  int total_prefix_base = 0;
  int recv_prefix_summary_ct = 0;

  int recv_pe_ct;
  int recv_pe_migrates_ct;
  int global_pt_ct; // Track number of Partition centroids received so far
  Vector3D<Real> pe_pt_centroid; // Average centroid of Partitions on this PE
  std::vector<Vector3D<Real>> local_pt_centroids; // A collection of all Partition centroids
  std::vector<std::vector<Vector3D<Real>>> global_pt_centroids; // A collection of Partition centroids on each PE;
  std::vector<Vector3D<Real>> global_pe_centroids; // A collection of average Partition centroid of each PE

  // Migrate counts to move out
  std::vector<int> closest_pe_migrates_ct;
  std::vector<int> closest_pt_migrates_ct;

  // Migrate counts to move in
  std::vector<int> recv_closest_pe_migrates_ct;
  std::vector<int> recv_closest_pt_migrates_ct;

  // total move out counts
  int closest_pt_migrates;
  int closest_pe_migrates;

  // Prefix related
  std::vector<LBCompareStats> st_obj_map;
  std::vector<LBCompareStats> pt_obj_map;
  std::vector<LBCompareStats> obj_map_to_balance;

  int st_ct; // Subtree object count
  int pt_ct; // Partition object count

  int total_iter;
  int prefix_stage;
  int prefix_sum;
  bool prefix_done;
  double prefix_start_time;
  double prefix_end_time;
  std::vector<char> flag_buf;
  std::vector<int> value_buf;
  std::vector<char> update_tracker;
  std::vector<char> send_tracker;
  int st_particle_size_sum = 0;
  int pt_particle_size_sum = 0;
  int my_particle_sum; // local sum, initial value for prefix_sum
  int total_particle_size = 0;
  double total_partition_load = 0.0;

  // Other
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
  void initVariables();
  void createObjMaps();
  void broadcastPECentroid();
  void makeSubtreeMoves();
  void broadcastSubtreeMigrateCts();
  int findPEWithClosestTP(Vector3D<Real> c);
  int findPEWithClosestAvgCentroid(Vector3D<Real> c);
  void moveSubtreeToClosestPartition();
  void moveSubtreeToClosestPE();
  void subtreeMigrationSummary();
  void prefixReset();
  void cleanUp();
  void doSubtreeMove();
  void doPrefix();
  void makePrefixMoves();
  void PackAndMakeMigrateMsgs(int num_moves,int total_ct, std::vector<MigrateInfo*> moves);
  void sendMigrateMsgs();
};

#endif /* _DistributedPrefixLB_H_ */
