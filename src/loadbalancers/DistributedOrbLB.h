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
using LBCommon::DorbPartitionRec;
using LBCommon::LBShortCmp;


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
  void loadBinning(DorbPartitionRec rec);
  void createPartitions(DorbPartitionRec rec);
  // void setPeSpliters(DorbPartitionRec rec);

  void moveTokenWithSplitPoint(DorbPartitionRec rec1, DorbPartitionRec rec2);
  void bCastSectionMoveTokenWithSplitPoint(DorbPartitionRec rec1, DorbPartitionRec rec2);
  void bCastSectionFinalPartitionStep(DorbPartitionRec rec, int curr_split_idx);
  void peDoneSpliting();
  void bCastSectionLoadBinning(DorbPartitionRec rec);
  void reportPostLBStates();

  void finalPartitionStep(DorbPartitionRec, int);
  void mergeFinalStepData(DorbPartitionRec, vector<LBShortCmp>);
  void mergeBinLoads(DorbPartitionRec rec, vector<float> bin_loads, vector<int> bin_sizes);
  void migrateObjects(std::vector<std::vector<Vector3D<Real>>> pe_splits);
  void acknowledgeIncomingMigrations(int count, float in_load);
  void sendFinalMigrations(int count);
  void finishedPartitionOneDim(DorbPartitionRec, float, float, int);
  void recv_tokens(DorbPartitionRec rec1, DorbPartitionRec rec2, vector<LBCentroidAndIndexRecord> objs);
  void doMigrations();
  void sendBackToken(LBCentroidAndIndexRecord obj);

  void bCastSectionGatherObjects(DorbPartitionRec rec);
  void mergeObjects(DorbPartitionRec rec);
  void addObjects(DorbPartitionRec rec, vector<LBCentroidAndIndexRecord> objs);
  void sendUpdatedTokens(vector<LBCentroidAndIndexRecord> tokens);
  void sendUpdatedLoad(float load);
  void waitForMigrations();

private:
  void InitLB(const CkLBOptions &);
  void Strategy(const DistBaseLB::LDStats* const stats);
  void initVariables();
  void parseLBData();
  void reportPerLBStates();
  int getDim(int dim, Vector3D<Real>& lower_coords, Vector3D<Real>& upper_coords);
  //void gatherBinLoads();
  bool inCurrentBox(Vector3D<Real> & centroid, Vector3D<Real> & lower_coords, Vector3D<Real> & upper_coords);
  int calNChildren(int pe, DorbPartitionRec & rec);
  int calSpinningTreeIdx(int root);
  // void bruteForceSolver(DorbPartitionRec rec);
  void processBinLoads(DorbPartitionRec rec, vector<float> bin_loads, vector<int> bin_sizes);
  void processFinalStepData(DorbPartitionRec rec);
  void findPeSplitPoints();
  void prepareToRecurse(DorbPartitionRec rec);
  void reset();


  void bruteForceSolverRecursiveHelper(DorbPartitionRec rec, int start_idx, int end_idx);
  void bruteForceSolver(DorbPartitionRec rec);




  int debug_l0 = 1;
  int debug_l1 = 2;
  int debug_l2 = 3;
  int lb_iter = 0;
  int bin_size = 16;
  int brute_force_size = 128;
  bool waited = false;
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
  float background_load, obj_load, bgload_per_obj, total_load, max_obj_load, min_obj_load, new_load;
  float post_lb_load;

  int n_partitions, n_particles, moved_partitions;
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
  vector <bool> partition_flags;
  vector <float> split_coords;
  int curr_depth;
  float curr_half_load, curr_left_load, curr_split_pt;
  int curr_left_nobjs, curr_right_nobjs;
  int recv_token_from_other_half;
  vector<LBCentroidAndIndexRecord> moved_in_tokens;
  int moved_in_pe_count;
  int section_member_count;
  vector<LBCentroidAndIndexRecord> section_obj_collection;
  vector<vector<LBCentroidAndIndexRecord>> section_updated_objects;
  vector<float> section_updated_load;
  int ready_to_recurse;
  bool went_right = true;
  bool use_longest_dim;
  Vector3D<Real> curr_lower_coords;
  Vector3D<Real> curr_upper_coords;

  // Partition results
  int recv_spliters = 0;
  vector<vector<Vector3D<Real>>> pe_split_coords;
  vector<float> pe_split_loads;
  vector<vector<float>> bin_loads_collection;
  vector<vector<int>> bin_sizes_collection;
  vector<vector<LBShortCmp>> fdata_collections;
  vector<int> child_counts;
  vector<int> child_received;
};


#endif /* _DistributedPref
          ixLB_H_ */
