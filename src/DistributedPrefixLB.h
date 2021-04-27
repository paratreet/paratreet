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
using LBCommon::LBCentroidRecord;
using LBCommon::LBCentroidCompare;

void CreateDistributedPrefixLB();

class DistributedPrefixLB : public CBase_DistributedPrefixLB {
public:
  DistributedPrefixLB(const CkLBOptions &);
  DistributedPrefixLB(CkMigrateMessage *m);
  static void initnodeFn(void);
  void reportPrefixInitDone(double);
  void prefixStep();
  void acknowledgeIncomingPrefixMigrations(int);
  void prefixPassValue(int, double);
  void donePrefix();
  void sendSummary(int, int);
  //void broadcastGlobalLoad(double);

  void sendPEParitionCentroids(int, std::vector<Vector3D<Real>>);
  void sendSubtreeMigrationDecisions(int);

private:
  int my_pe;
  LBMigrateMsg * final_migration_msg;
  bool lb_partition_term = true;

  // Only used in PE[0] for prefix summary
  int total_prefix_moves; // Number of migration moves
  int total_prefix_objects; // Number of objects
  int recv_prefix_summary_ct;
  int total_init_complete = 0;

  double global_load = 0.0;

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
  double prefix_sum;
  double my_prefix_load;
  bool prefix_done;
  double prefix_start_time;
  double prefix_end_time;
  std::vector<char> flag_buf;
  std::vector<double> value_buf;
  std::vector<char> update_tracker;
  std::vector<char> send_tracker;
  int st_particle_size_sum = 0;
  int pt_particle_size_sum = 0;
  int my_particle_sum; // local sum, initial value for prefix_sum
  int total_particle_size = 0; // total particles in universe
  double total_partition_load = 0.0;
  double total_subtree_load = 0.0;
  double total_pe_load = 0.0;
  double background_load = 0.0;
  double background_load_ratio = 1.0;


  // LB subtree related variables
  int nearestK = 20;/*{{{*/
  int recv_pe_centroids_ct = 0;
  int recv_incoming_subtree_counts = 1;
  int total_subtree_migrates;
  int incoming_subtree_migrations = 0;
  Vector3D<Real> pe_avg_partition_centroid;
  std::vector<Vector3D<Real>> local_partition_centroids;
  std::vector<LBCentroidRecord> global_partition_centroids;

  std::vector<int> subtree_migrate_out_ct;/*}}}*/

  // LB Average Smoothing related variables



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

  void subtreeLBInits();
  void subtreeLBCleanUp();
  void broadcastLocalPartitionCentroids();
  void makeSubtreeMoves();
  int calculateTargetPE(Vector3D<Real>, int);


  void PackAndMakeMigrateMsgs(int num_moves,int total_ct);
  void sendMigrateMsgs();
};


#endif /* _DistributedPref
          ixLB_H_ */
