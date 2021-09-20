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
  for (LDObjStats & oStat : objMap){
    int pe = oStat.from_proc;
    prefixed_load += oStat.data.wallTime * stats -> procs[pe].pe_speed;
    prefix_particle_size += size_map[oStat.index];
    int to_pe_load = std::floor(prefixed_load / avg_load);
    int to_pe_size = std::floor(prefix_particle_size / avg_particals);
    int to_pe = std::min(to_pe_load, n_pes-1);
    stats->assign(oStat.index, to_pe);
    if (pe != to_pe){
      migrate_ct ++;
      //CkPrintf("Move obj[%d] from PE[%d] to PE[%d]\n", oStat.index, pe, to_pe);
    }
  }

  balance_partitions = !balance_partitions;
  double end_time = CmiWallTimer();
  //CkPrintf("[%d] PrefixLB strategy moved %d objs\n n_pes = %d; n_objs = %d; n_migrateobjs = %d\n total migratable ct= %d load =  %f; total nonmig ct = %d load = %f\n",CkMyPe(),  migrate_ct, n_pes, stats->objData.size(), stats->n_migrateobjs, migratable_obj_ct, total_load, nonmig_obj_ct, total_nonmig_load);
  CkPrintf("CharmLB> PrefixLB: PE [%d] moved %d objects. Elapse time = %.4f ms\n", CkMyPe(), migrate_ct, (end_time - start_time)*1000);
  //CkPrintf("Partition load =  %f, else = %f\n", total_partition_load, total_load - total_partition_load);
  #endif
}

#include "PrefixLB.def.h"
