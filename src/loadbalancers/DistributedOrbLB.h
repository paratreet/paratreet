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
using std::vector;
using LBCommon::LBUserData;
using LBCommon::LBCentroidAndIndexRecord;

void CreateDistributedOrbLB();

class DistributedOrbLB : public CBase_DistributedOrbLB {
public:
  DistributedOrbLB(const CkLBOptions &);
  DistributedOrbLB(CkMigrateMessage *m);
  static void initnodeFn(void);
  void perLBStates(CkReductionMsg * msg);\
  void getUniverseDimensions(CkReductionMsg * msg);
  void recursiveLoadPartition();
  void binaryLoadPartitionWithOctalBins(int dim, float load, int left, int right, float low, float high, Vector3D<Real> lower_coords, Vector3D<Real> upper_coords, const CkCallback &);
  void getSumOctalLoads(CkReductionMsg * msg);
  void createPartitions(int dim, float load, int left, int right, Vector3D<Real> lower_coords, Vector3D<Real> upper_coords);
  void finishedPartitionOneDim(const CkCallback & curr_cb);
  void migrateObjects(std::vector<std::vector<Vector3D<Real>>> pe_splits);
  void acknowledgeIncomingMigrations(int count);
  void sendFinalMigrations(int count);
  void summary(int nmove, int total);


private:
  int debug_l0 = 1;
  int debug_l1 = 2;
  int debug_l2 = 3;
  int my_pe, total_migrates, incoming_migrates, recv_ack;
  int recv_final, incoming_final;
  const DistBaseLB::LDStats* my_stats;
  double start_time;
  vector<MigrateInfo*> migrate_records;
  vector<int> send_nobjs_to_pes;
  LBMigrateMsg * final_migration_msg;

  // Collect myPE load data
  float background_load, obj_load, bgload_per_obj, total_load, max_obj_load, min_obj_load;

  int n_partitions, n_particles;
  int dim;
  vector<vector<float>> obj_coords; // corrodination limits at all directions
  vector<float> universe_coords; // size 6, min, max coord of 3 dimensions

  vector<LBCentroidAndIndexRecord> obj_collection;

  // Collect global load data
  float global_load;
  float global_max_obj_load, global_min_obj_load;
  float granularity;
  vector <float> global_octal_loads;
  vector <int> global_octal_sizes;

  int recv_summary, total_move, total_objs;

  // Parition related data
  vector <float> octal_loads;
  vector <int> octal_sizes;
  CkCallbackResumeThread * curr_cb;
  vector <float> split_coords;
  int curr_dim, curr_depth;
  int left_idx, right_idx;
  float lower_split, upper_split;
  float curr_load, curr_left_load, curr_split_pt;
  bool went_right = true;
  bool use_longest_dim;
  Vector3D<Real> curr_lower_coords;
  Vector3D<Real> curr_upper_coords;

  vector <Vector3D<Real>> lower_coords_col;
  vector <Vector3D<Real>> upper_coords_col;

  // Partition results
  vector<vector<Vector3D<Real>>> pe_split_coords;
  vector<float> pe_split_loads;


  void InitLB(const CkLBOptions &);
  void Strategy(const DistBaseLB::LDStats* const stats);
  void initVariables();
  void parseLBData();
  void reportPerLBStates();
  int getDim(int dim, Vector3D<Real>& lower_coords, Vector3D<Real>& upper_coords);
  void reportUniverseDimensions();
  void gatherOctalLoads();
  void findPeSplitPoints();
  void reset();
};


#endif /* _DistributedPref
          ixLB_H_ */
