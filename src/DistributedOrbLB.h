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
  void binaryLoadPartition(int dim, float load, Vector3D<Real> lower_coords, Vector3D<Real> upper_coords);
  void getSumBinaryLoads(CkReductionMsg * msg);
  void createPartitions(int dim, float load, Vector3D<Real> lower_coords, Vector3D<Real> upper_coords);


private:
  int my_pe;
  const DistBaseLB::LDStats* my_stats;
  double start_time;

  // Collect myPE load data
  float background_load, obj_load, bgload_per_obj, total_load, max_obj_load;

  int n_partitions, n_particles;
  int dim;
  vector<vector<float>> obj_coords; // corrodination limits at all directions
  vector<float> universe_coords; // size 6, min, max coord of 3 dimensions

  vector<LBCentroidAndIndexRecord> obj_collection;

  // Parition related data
  vector <float> binary_loads;

  // Collect global load data
  float global_load;
  float global_max_obj_load;
  float granularity;
  vector <float> global_binary_loads;

  vector <float> split_coords;
  int curr_dim, curr_depth;
  float curr_load;
  bool went_right = true;
  Vector3D<Real> curr_lower_coords;
  Vector3D<Real> curr_upper_coords;

  vector <Vector3D<Real>> lower_coords_col;
  vector <Vector3D<Real>> upper_coords_col;
  vector <float> load_col;
  vector <float> left_load_col;


  void InitLB(const CkLBOptions &);
  void Strategy(const DistBaseLB::LDStats* const stats);
  void initVariables();
  void parseLBData();
  void reportPerLBStates();
  void reportUniverseDimensions();
  void gatherBinaryLoads();
  void reset();
};


#endif /* _DistributedPref
          ixLB_H_ */
