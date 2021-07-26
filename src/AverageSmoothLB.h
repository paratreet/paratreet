#ifndef _AVERAGESMOOTHINGLB_H_
#define _AVERAGESMOOTHINGLB_H_

#include "CentralLB.h"
#include "AverageSmoothLB.decl.h"
#include "LBCommon.h"

void CreateAverageSmoothLB();
BaseLB * AllocateAverageSmoothLB();

using LBCommon::LBUserData;
using LBCommon::LBCompareStats;

class AverageSmoothLB : public CBase_AverageSmoothLB {
  public:
    AverageSmoothLB(const CkLBOptions &);
    AverageSmoothLB(CkMigrateMessage *m):CBase_AverageSmoothLB(m){
      lbname = (char*)"AverageSmoothLB";
    };
    void work(LDStats* stats);
    void initVariables();
    void calcBackgroundLoads();
    void createObjMaps();
    void calcSmoothedLoads();
    void averageSmoothedLoads();
    void maxRatioAverageSmoothedLoads();
    void neighborSmoothedLoads();
    void makeDecisions();
    void cleanUp();


  private:
    bool QueryBalanceNow(int step){
      return true;
    };

    double start_time;
    double total_load;// = obj load + bg_load
    double max_load, smoothed_max_load, min_load, average_load;
    double max_avg_ratio, smoothed_max_avg_ratio, max_min_ratio;
    int n_pes;
    int pt_ct, st_ct, nonpt_ct, migration_ct;

    double avg_smooth_alpha = 0.2;
    double avg_smooth_ratio_alpha = 0.15;
    double neighbor_smooth_alpha = 0.7;

    LDStats * my_stats;

    std::vector<LBCompareStats> obj_collection; //processed LDObjData for sorting
    std::vector<double> background_loads; //pe[i]'s background_load
    std::vector<double> pe_background_share; //pe[i]'s background_load / number of parition objects
    std::vector<double> smoothed_loads; // smoothed load for each PE
    std::vector<double> smoothed_prefixs; // prefix of smoothed loads for each PE
    std::vector<double> pe_pt_loads;
    // non partition load for each PE
    std::vector<double> pe_nonpt_loads;
    std::vector<double> pe_sum_loads;

    std::vector<int> pe_pt_ct;
};

#endif /* _AVERAGESMOOTHINGLB_H_ */
