#include "elements.h"
#include "ckheap.h"
#include "PrefixLB.h"
#include "LBCommon.h"

extern int quietModeRequested;
CkpvExtern(int, _lb_obj_index);

using LBCommon::LBUserData;

CreateLBFunc_Def(PrefixLB, "Move jobs base on prefix sum.")

PrefixLB::PrefixLB(const CkLBOptions &opt): CBase_PrefixLB(opt)
{
  lbname = (char *)"PrefixLB";
  if (CkMyPe() == 0 && !quietModeRequested)
    CkPrintf("CharmLB> PrefixLB created.\n");
  #if CMK_LB_USER_DATA
  if (CkpvAccess(_lb_obj_index) == -1)
    CkpvAccess(_lb_obj_index) = LBRegisterObjUserData(sizeof(LBUserData));
  #endif
}

void PrefixLB::work(LDStats* stats)
{
  start_time = CmiWallTimer();
  int obj;
  int n_pes = stats->nprocs();

  // calculate total wallTime of all objects
  double total_load = 0.0, total_nonmig_load = 0.0;
  int migratable_obj_ct = 0, nonmig_obj_ct = 0;
  std::vector<LDObjStats> objMap;

  double total_partition_load = 0.0;
  int total_partical = 0;
  std::vector<int> size_map (stats->objData.size(), 0);
  auto type_to_balance = LBCommon::pt;
  if (!balance_partitions) type_to_balance = LBCommon::st;

  for (int obj_idx = 0; obj_idx < stats->objData.size(); obj_idx++){
    LDObjData &oData = stats->objData[obj_idx];
    int pe = stats->from_proc[obj_idx];

    #if CMK_LB_USER_DATA
    int idx = CkpvAccess(_lb_obj_index);
    LBUserData usr_data = *(LBUserData *)oData.getUserData(CkpvAccess(_lb_obj_index));
    if (usr_data.lb_type == type_to_balance){
      // Assign chare_idx at the position of to_proc
      objMap.push_back(LDObjStats{obj_idx, oData, pe, usr_data.chare_idx});
      size_map[obj_idx] = usr_data.particle_size;
      total_partition_load += oData.wallTime * stats->procs[pe].pe_speed;
      total_partical += usr_data.particle_size;
      //CkPrintf("Type %d: size = %d\n", usr_data.lb_type, usr_data.particle_size);
    }else{
      // For non Parition objects, keep the PE
      stats->assign(obj_idx, pe);
    }
    #endif
  }

  #if CMK_LB_USER_DATA
  std::sort(objMap.begin(), objMap.end(), PrefixLBCompareLDObjStats);

  double avg_load = total_partition_load / n_pes;
  int avg_particals = total_partical / n_pes;
  double prefixed_load = 0.0;
  int prefix_particle_size = 0;
  int migrate_ct = 0;
  vector<double> before_lb_pe_load(n_pes, .0);
  vector<double> after_lb_pe_load(n_pes, .0);
  for (LDObjStats & oStat : objMap){
    int pe = oStat.from_proc;
    double myload = oStat.data.wallTime * stats -> procs[pe].pe_speed;
    prefixed_load += myload;
    prefix_particle_size += size_map[oStat.index];
    int to_pe_load = std::floor(prefixed_load / avg_load);
    int to_pe_size = std::floor(prefix_particle_size / avg_particals);
    int to_pe = std::min(to_pe_load, n_pes-1);
    oStat.to_proc = to_pe;
    stats->assign(oStat.index, to_pe);
    before_lb_pe_load[pe] += myload;
    after_lb_pe_load[to_pe] += myload;
    if (pe != to_pe){
      migrate_ct ++;
      //CkPrintf("Move obj[%d] from PE[%d] to PE[%d]\n", oStat.index, pe, to_pe);
    }
  }

  double before_lb_ratio = *std::max_element(before_lb_pe_load.begin(), before_lb_pe_load.end()) / avg_load;
  double after_lb_ratio = *std::max_element(after_lb_pe_load.begin(), after_lb_pe_load.end()) / avg_load;


  double end_time = CmiWallTimer();
  //CkPrintf("[%d] PrefixLB strategy moved %d objs\n n_pes = %d; n_objs = %d; n_migrateobjs = %d\n total migratable ct= %d load =  %f; total nonmig ct = %d load = %f\n",CkMyPe(),  migrate_ct, n_pes, stats->objData.size(), stats->n_migrateobjs, migratable_obj_ct, total_load, nonmig_obj_ct, total_nonmig_load);
  CkPrintf("CharmLB> PrefixLB on %s : moved %d /%d objects. Elapse time = %.4f ms\n\t before and after lb max/avg ratio: %.4f -> %.4f \n", (balance_partitions? "partitions" : "subtrees"),migrate_ct, objMap.size(), (end_time - start_time)*1000, before_lb_ratio, after_lb_ratio);
  //CkPrintf("Partition load =  %f, else = %f\n", total_partition_load, total_load - total_partition_load);
  balance_partitions = !balance_partitions;
  #endif
}

#include "PrefixLB.def.h"
