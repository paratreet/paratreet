
#ifndef _DIFFUSIONLB_H_
#define _DIFFUSIONLB_H_

#include "CentralLB.h"
#include "DiffusionLB.decl.h"
#include "LBCommon.h"

using std::vector;
using LBCommon::LBUserData;
using LBCommon::LBCentroidAndIndexRecord;

void CreateDiffusionLB();
BaseLB *AllocateDiffusionLB();

class DiffusionLB : public CBase_DiffusionLB {
protected:
  double start_time;

  // LB stats related
  int n_pes;
  LDStats * my_stats;
  int migration_ct;
  vector<float> pe_loads;
  vector<float> pe_bgload;
  vector<float> pe_bgload_share;
  vector<float> pe_obj_loads;

  vector<int> pe_nobjs;
  vector<int> pe_nparticles;
  vector<vector<LBCentroidAndIndexRecord>> pe_obj_records; //[i][j] is the centroid of obj[j] on pe [i]
  vector<Vector3D<Real>> pe_avg_centroids;

  // Diffusion algorithm related
  vector<float> pe_diffused_loads;
  vector<int> overload_pes;
  vector<int> underload_pes;
  vector<float> pe_delta_loads;
  //vector<vector<Vector3D>> obj_moves;

  // Load stats related
  double total_load, min_load, max_load, average_load;
  double max_avg_ratio, max_min_ratio;
  double post_lb_min_load, post_lb_max_load;
  double diffused_max_load;

  // Paricle size related
  int max_particle_size, min_particle_size;
  float average_particle_size, max_avg_particle_ratio;


public:
  DiffusionLB(const CkLBOptions &);
  DiffusionLB(CkMigrateMessage *m):CBase_DiffusionLB(m) { lbname = (char *)"DiffusionLB"; };
  void work(LDStats* stats);

  void initVariables();
  void collectPELoads();
  void collectUserData();
  void printParticlesSummary();
  void doDiffusion();
  void pairOverAndUnderloadedPEs();
  void calculateNearestPE(LBCentroidAndIndexRecord &);
  void pairNearestPE();
  void printSummary();
  void cleanUp();
};

#endif /* _DIFFUSIONLB_H_ */

/*@}*/
