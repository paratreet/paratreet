/**
 * Author: simengl2@illinois.edu (Simeng Liu)
*/

#ifndef _DISTPREFIXLB_H_
#define _DISTPREFIXLB_H_

#include "DistBaseLB.h"
#include "DistributedOrbLB.decl.h"

#include "ckheap.h"
#include "LBCommon.h"

#include <vector>
#include <tuple>
#include <utility>
#include <map>

using std::vector;
using std::map;
using std::tuple;
using std::pair;
using std::get;
using LBCommon::LBUserData;
using LBCommon::LBCentroidAndIndexRecord;
using LBCommon::DorbPartitionRec;

void CreateDistributedOrbLB();

class DistributedOrbLB : public CBase_DistributedOrbLB {
public:
  DistributedOrbLB(const CkLBOptions &);
  DistributedOrbLB(CkMigrateMessage *m);
  static void initnodeFn(void);
  void perLBStates(CkReductionMsg * msg);
  void postLBStates(CkReductionMsg * msg);
  void endLB();
  void reportUniverseDimensions();
  void getUniverseDimensions(CkReductionMsg * msg);
  void recursiveLoadPartition();
  void binaryLoadPartitionWithBins(DorbPartitionRec rec);
  void getSumBinLoads(CkReductionMsg * msg);
  void createPartitions(DorbPartitionRec rec);
  void setPeSpliters(DorbPartitionRec rec);
  void reportBinLoads(DorbPartitionRec, vector<float>, vector<int>);
  void migrateObjects(std::vector<std::vector<Vector3D<Real>>> pe_splits);
  void acknowledgeIncomingMigrations(int count, float in_load);
  void sendFinalMigrations(int count);
  void finishedPartitionOneDim(const CkCallback & curr_cb);


private:
  int debug_l0 = 1;
  int debug_l1 = 2;
  int debug_l2 = 3;
  int lb_iter = 0;
  int bin_size = 16;
  double bin_size_double = 16.0;
  int my_pe, total_migrates, incoming_migrates, recv_ack;
  int recv_final, incoming_final;
  const DistBaseLB::LDStats* my_stats;
  double start_time;
  vector<MigrateInfo*> migrate_records;
  vector<int> send_nobjs_to_pes;
  vector<float> send_nload_to_pes;
  LBMigrateMsg * final_migration_msg;

  // Collect myPE load data
  float background_load, obj_load, bgload_per_obj, total_load, max_obj_load, min_obj_load;
  float post_lb_load;

  int n_partitions, n_particles;
  int dim;
  vector<vector<float>> obj_coords; // corrodination limits at all directions
  vector<float> universe_coords; // size 6, min, max coord of 3 dimensions

  vector<LBCentroidAndIndexRecord> obj_collection;

  // Collect global load data
  float global_load;
  float global_max_obj_load, global_min_obj_load;
  float granularity;
  vector <float> global_bin_loads;
  vector <int> global_bin_sizes;

  int recv_summary, total_move, total_objs;

  // Parition related data
  vector <float> octal_loads;
  vector <int> octal_sizes;
  CkCallbackResumeThread * curr_cb;
  vector <float> split_coords;
  int curr_dim, curr_depth;
  int left_idx, right_idx;
  double lower_split, upper_split;
  float curr_load, curr_left_load, curr_split_pt;
  bool went_right = true;
  bool use_longest_dim;
  Vector3D<Real> curr_lower_coords;
  Vector3D<Real> curr_upper_coords;

  // Bin data
  map<pair<int, int>,
    tuple<int, vector<float>, vector<int>>> bin_data_map;


  // Partition results
  int recv_spliters = 0;
  vector<vector<Vector3D<Real>>> pe_split_coords;
  vector<float> pe_split_loads;


  void InitLB(const CkLBOptions &);
  void Strategy(const DistBaseLB::LDStats* const stats);
  void initVariables();
  void parseLBData();
  void reportPerLBStates();
  void reportPostLBStates();
  int getDim(int dim, Vector3D<Real>& lower_coords, Vector3D<Real>& upper_coords);
  void gatherBinLoads();
  void findPeSplitPoints();
  void reset();
};


#endif /* _DistributedPref
          ixLB_H_ */
