
#ifndef _ORB3DLB_H_
#define _ORB3DLB_H_

#include "CentralLB.h"
#include "OrbLB.decl.h"
#include "LBCommon.h"

using std::vector;
using LBCommon::LBUserData;
using LBCommon::LBCentroidAndIndexRecord;

void CreateOrbLB();
BaseLB *AllocateOrbLB();

class OrbLB : public CBase_OrbLB {
protected:

    bool use_longest_dim = true;
    bool balance_partitions = true;
    float start_time;
    float total_load;// = obj load + bg_load
    float max_load, min_load, average_load;
    int max_particle_size, min_particle_size;
    float average_particle_size;
    float post_lb_max_load, post_lb_min_load, post_lb_average_load;

    float max_avg_ratio, post_lb_max_avg_ratio, max_min_ratio, post_lb_max_min_ratio;
    float max_avg_particle_ratio;
    int n_pes;
    int migration_ct;
    float migrate_ratio;
    //int pt_ct, st_ct, nonpt_ct, migration_ct;
    int post_lb_max_size, post_lb_min_size;
    float post_lb_size_max_avg_ratio;

    LDStats * my_stats;

    vector<float> pe_loads; // non_idle loads for each PE
    vector<int> pe_obj_size;
    vector<float> pe_bgload;
    vector<float> pe_objload;
    vector<float> pe_bgload_share;
    vector<float> pre_lb_pe_loads;
    vector<float> post_lb_pe_loads; // non_idle loads for each PE after LB
    vector<LBCentroidAndIndexRecord> centroids; // centroid for each PE
    vector<int> pe_particle_size; // inital particle size for each PE
    vector<int> post_lb_pe_particle_size; // post lb particle size for each PE
    vector<int> map_sorted_idx_to_pe; // @i = a, @i+1 = b centroids[a to (b-1)] map to PE[i]


public:
  OrbLB(const CkLBOptions &);
  OrbLB(CkMigrateMessage *m):CBase_OrbLB(m) { lbname = (char *)"OrbLB"; };
  void work(LDStats* stats);
private:
  bool QueryBalanceNow(int step) { return true;}

  void collectUserData();
  void collectPELoads();
  int findLongestDimInCentroids(int, int);
  // assign centroids[idx].to_pe = to_pe
  void assign(int idx, int to_pe);
  // orb the centroids[start_cent to (end_cent - 1)] for PE[start_pe to (end_pe -1) ]
  void orbCentroidsForEvenLoadRecursiveHelper(int start_pe, int end_pe, int start_cent, int end_cent, int dim);
  void orbCentroidsForEvenParticleSizeRecursiveHelper(int start_pe, int end_pe, int start_cent, int end_cent, int dim);
  void orbCentroids();
  float calcPartialLoadSum(int, int);
  int calcPartialParticleSum(int, int);
  void initVariables();
  void summary();
  void cleanUp();

};

#endif /* _ORB3DLB_H_ */

/*@}*/
