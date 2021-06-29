#include "elements.h"
#include "AverageSmoothLB.h"

extern int quietModeRequested;
CkpvExtern(int, _lb_obj_index);

CreateLBFunc_Def(AverageSmoothLB, "Move jobs base on average smoothing.")

AverageSmoothLB::AverageSmoothLB(const CkLBOptions& opt):CBase_AverageSmoothLB(opt){
  lbname = (char*)"AverageSmoothLB";/*{{{*/
  if(CkMyPe() == 0) CkPrintf("CharmLB> AverageSmoothLB created.\n");
  #if CMK_LB_USER_DATA
  if (CkpvAccess(_lb_obj_index) == -1)
    CkpvAccess(_lb_obj_index) = LBRegisterObjUserData(sizeof(LBUserData));
  #endif
};/*}}}*/

void AverageSmoothLB::work(LDStats * stats){
  start_time = CmiWallTimer();
  n_pes = stats->nprocs();
  my_stats = stats;

  initVariables();
  calcBackgroundLoads();
  createObjMaps();
  calcSmoothedLoads();
  makeDecisions();
  cleanUp();

  double end_time = CmiWallTimer();
  CkPrintf("AverageSmoothLB >> Strategy elapse time = %.4f ms\n", (end_time - start_time) * 1000);
};

void AverageSmoothLB::initVariables(){
  total_load = 0.0;/*{{{*/
  max_load = 0.0;
  smoothed_max_load = 0.0;
  min_load = 0.0;
  average_load = 0.0;
  max_avg_ratio = 0.0;
  smoothed_max_avg_ratio;
  max_min_ratio = 0.0;
  pt_ct = 0;
  st_ct = 0;
  nonpt_ct = 0;
  migration_ct = 0;

  background_loads = std::vector<double>(n_pes, 0.0);
  pe_background_share = std::vector<double>(n_pes, 0.0);
  smoothed_loads = std::vector<double>(n_pes, 0.0);
  smoothed_prefixs = std::vector<double>(n_pes, 0.0);
  pe_pt_loads = std::vector<double>(n_pes, 0.0);
  pe_nonpt_loads = std::vector<double>(n_pes, 0.0);
  //pe_sum_loads = std::vector<double>(n_pes, 0.0);
  pe_pt_ct = std::vector<int>(n_pes, 0);
}/*}}}*/

void AverageSmoothLB::calcBackgroundLoads(){
  for(auto & p : my_stats->procs){/*{{{*/
    background_loads[p.pe] = p.pe_speed * p.bg_walltime;
    total_load += background_loads[p.pe];
    double tmp_total_load = p.pe_speed * p.total_walltime;
    double idle_load = p.pe_speed * p.idletime;
    if (_lb_args.debug() >= 2)
      CkPrintf("PE[%d] %d objs total load = %.4f idle load = %.4f background load = %.4f\n", p.pe, p.n_objs, tmp_total_load, idle_load, background_loads[p.pe]);
  }
}/*}}}*/

void AverageSmoothLB::createObjMaps(){
  for(int obj_idx = 0; obj_idx < my_stats->objData.size(); obj_idx++){/*{{{*/
    LDObjData &oData = my_stats->objData[obj_idx];
    int pe = my_stats->from_proc[obj_idx];
    double obj_load = my_stats->procs[pe].pe_speed * oData.wallTime;
    total_load += obj_load;

    #if CMK_LB_USER_DATA
    int idx = CkpvAccess(_lb_obj_index);
    LBUserData usr_data = *(LBUserData *)oData.getUserData(CkpvAccess(_lb_obj_index));
    if (usr_data.lb_type == LBCommon::pt){
      // Assign chare_idx at the position of to_proc
      obj_collection.push_back(
          LBCompareStats{
            obj_idx, usr_data.chare_idx,
            usr_data.particle_size, obj_load,
            &oData, pe, usr_data.centroid
          });
      pe_pt_ct[pe] ++;
      pe_pt_loads[pe] += obj_load;
      pt_ct ++;
    }else if(usr_data.lb_type == LBCommon::st){
      st_ct ++;
    }
    else{
      // For non Parition objects, keep the PE
      my_stats->assign(obj_idx, pe);
      nonpt_ct ++;
      background_loads[pe] += obj_load;
    }
    #endif
  }

  std::sort(obj_collection.begin(), obj_collection.end(), ComparePEAndChareidx);
  // debug prints
  //int obj_sum = 0;{{{
  //for (int i = 0; i < n_pes; i++){
  //  obj_sum += pe_pt_ct[i];
  //  CkPrintf("PE[%d] obj ct = %d ; pt_load = %.4f nonpt_load = %.4f\n", i, pe_pt_ct[i], pe_pt_loads[i], pe_nonpt_loads[i]);
  //}

  //CkPrintf("Sum = %d ; n_migt = %d; pt_ct = %d; st_ct = %d; other = %d\n", obj_sum, my_stats->n_migrateobjs, pt_ct, st_ct, nonpt_ct);}}}

}/*}}}*/

void AverageSmoothLB::calcSmoothedLoads(){
  //averageSmoothedLoads();
  //neighborSmoothedLoads();
  maxRatioAverageSmoothedLoads();
}

void AverageSmoothLB::averageSmoothedLoads(){
  average_load = total_load / (double) n_pes;/*{{{*/
  for (int i = 0; i< n_pes; i++){
    double curr_pe_load = pe_pt_loads[i] + background_loads[i];
    double smoothed_pe_load = avg_smooth_alpha * curr_pe_load + (1.0 - avg_smooth_alpha) * average_load;
    smoothed_loads[i] = smoothed_pe_load;
    if (max_load < curr_pe_load) max_load = curr_pe_load;
    if (smoothed_max_load < smoothed_pe_load) smoothed_max_load = smoothed_pe_load;
    if (i == 0)
      smoothed_prefixs[i] = smoothed_pe_load;
    else
      smoothed_prefixs[i] = smoothed_prefixs[i-1] + smoothed_loads[i];
  }
  max_avg_ratio = max_load / average_load;
  smoothed_max_avg_ratio = smoothed_max_load / average_load;
}/*}}}*/

void AverageSmoothLB::maxRatioAverageSmoothedLoads(){
  average_load = total_load / (double) n_pes;/*{{{*/
  std::vector<double> deltas(n_pes, 0.0);
  std::vector<double> pe_loads(n_pes, 0.0);
  double max_delta = 0.0;
  for (int i = 0; i< n_pes; i++){
    double curr_pe_load = pe_pt_loads[i] + background_loads[i];
    pe_loads[i] = curr_pe_load;
    deltas[i] = std::abs(curr_pe_load - average_load);
    if (deltas[i] > max_delta) max_delta = deltas[i];
  }

  // calculate the alpha so that the resulting max_avg_ratio
  // won't be bigger than avg_smooth_ratio_alpha
  avg_smooth_alpha = avg_smooth_ratio_alpha * average_load / max_delta;

  for (int i = 0; i< n_pes; i++){
    double smoothed_pe_load = avg_smooth_alpha * pe_loads[i] + (1.0 - avg_smooth_alpha) * average_load;
    smoothed_loads[i] = smoothed_pe_load;
    if (max_load < pe_loads[i]) max_load = pe_loads[i];
    if (smoothed_max_load < smoothed_pe_load) smoothed_max_load = smoothed_pe_load;
    if (i == 0)
      smoothed_prefixs[i] = smoothed_pe_load;
    else
      smoothed_prefixs[i] = smoothed_prefixs[i-1] + smoothed_loads[i];
  }
  max_avg_ratio = max_load / average_load;
  smoothed_max_avg_ratio = smoothed_max_load / average_load;
}/*}}}*/

void AverageSmoothLB::neighborSmoothedLoads(){
  average_load = total_load / (double) n_pes;/*{{{*/
  double curr_pe_load, next_pe_load, smoothed_pe_load;
  for (int i = 0; i< n_pes; i++){
    curr_pe_load = pe_pt_loads[i] + background_loads[i];
    if (i == n_pes - 1){
      smoothed_pe_load = total_load - smoothed_prefixs[i-1];
    }else{
      next_pe_load = pe_pt_loads[i+1] + background_loads[i+1];
      smoothed_pe_load = neighbor_smooth_alpha * curr_pe_load + (1.0 - neighbor_smooth_alpha) * next_pe_load;
    }
    smoothed_loads[i] = smoothed_pe_load;
    if (max_load < curr_pe_load) max_load = curr_pe_load;
    if (smoothed_max_load < smoothed_pe_load) smoothed_max_load = smoothed_pe_load;
    if (i == 0)
      smoothed_prefixs[i] = smoothed_pe_load;
    else
      smoothed_prefixs[i] = smoothed_prefixs[i-1] + smoothed_loads[i];
  }

  max_avg_ratio = max_load / average_load;
  smoothed_max_avg_ratio = smoothed_max_load / average_load;
}/*}}}*/

void AverageSmoothLB::makeDecisions(){
  int to_pe = 0;
  double curr_prefix = 0.0;

  for (int i = 0; i < n_pes; i++){
    pe_background_share[i] = background_loads[i] / (double) pe_pt_ct[i];
  }

  for (LBCompareStats & obj : obj_collection){
    int pe = obj.from_proc;
    while (curr_prefix > smoothed_prefixs[to_pe]){
      to_pe ++;
      if(_lb_args.debug() >= 2) CkPrintf("curr_prefix = %.4f to_pe = %d last prefix = %.4f next_prefix = %.4f\n", curr_prefix, to_pe, smoothed_prefixs[to_pe - 1], smoothed_prefixs[to_pe]);
    }

    if(_lb_args.debug() >= 1) CkPrintf("LB[%d] obj[%d] send to %d; curr_prefix = %.4f\n", pe, obj.chare_idx, to_pe, curr_prefix);


    curr_prefix += (obj.load + pe_background_share[pe]);
    if (to_pe != pe){
      my_stats->assign(obj.index, to_pe);
      migration_ct ++;
    }
  }

  CkPrintf("AverageSmoothLB >> avg_smooth_alpha = %.2f\nSummary:: moved %d / %d objects; max_avg_ratio %.4f -> %.4f \n", avg_smooth_alpha, migration_ct, pt_ct, max_avg_ratio, smoothed_max_avg_ratio);
}

void AverageSmoothLB::cleanUp(){
  obj_collection.clear();/*{{{*/
  background_loads.clear();
  pe_background_share.clear();
  smoothed_loads.clear();
  smoothed_prefixs.clear();
  pe_pt_loads.clear();
  pe_pt_ct.clear();
  pe_nonpt_loads.clear();
}/*}}}*/

#include "AverageSmoothLB.def.h"
