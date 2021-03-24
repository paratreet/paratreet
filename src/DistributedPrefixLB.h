/**
 * Author: simengl2@illinois.edu (Simeng Liu)
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
  void reportInitDone();
  void prefixStep();
  void acknowledgeIncomingPrefixMigrations(int);
  void prefixPassValue(int, int);
  void donePrefix();
  void sendPrefixSummary(int, int);

private:
  int my_pe;
  LBMigrateMsg * final_migration_msg;

  // Only used in PE[0] for prefix summary
  int total_prefix_moves; // Number of migration moves
  int total_prefix_objects; // Number of objects
  int recv_prefix_summary_ct;
  int total_init_complete = 0;

  // Prefix related
  std::vector<LBCompareStats> st_obj_map;
  std::vector<LBCompareStats> pt_obj_map;
  std::vector<LBCompareStats> obj_map_to_balance;
  std::vector<int> prefix_migrate_out_ct;

  int total_incoming_prefix_migrations;
  int recv_prefix_move_pes;

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
  int total_particle_size = 0; // total particles in universe
  double total_partition_load = 0.0;

  // Other
  double avg_size;
  int max_size;
  double size_ratio;
  int total_migrates;
  std::vector<MigrateInfo*> migrate_records;

  // Constant variables for configuring how the strategy works
  double start_time;

  const DistBaseLB::LDStats* my_stats;

  void InitLB(const CkLBOptions &);
  void Strategy(const DistBaseLB::LDStats* const stats);
  void initVariables();
  void createObjMaps();
  void prefixInit();
  void cleanUp();
  void sendOutPrefixDecisions();
  void makePrefixMoves();
  void PackAndMakeMigrateMsgs(int num_moves,int total_ct);
  void sendMigrateMsgs();
};

#endif /* _DistributedPrefixLB_H_ */
