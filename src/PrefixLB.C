/**
 * \addtogroup CkLdb
*/
/*@{*/

#include "elements.h"
#include "ckheap.h"
#include "PrefixLB.h"

extern int quietModeRequested;
CkpvExtern(int, _lb_obj_index);

CreateLBFunc_Def(PrefixLB, "Move jobs base on prefix sum.")

PrefixLB::PrefixLB(const CkLBOptions &opt): CBase_PrefixLB(opt)
{
  lbname = (char *)"PrefixLB";
  if (CkMyPe() == 0 && !quietModeRequested)
    CkPrintf("CharmLB> PrefixLB created.\n");
  #if CMK_LB_USER_DATA
  CkpvAccess(_lb_obj_index) = LBRegisterObjUserData(sizeof(int));
  #endif
}

//typedef struct {
//  int index;
//  LDObjData data;
//  int from_proc;
//  int to_proc;
//  inline void pup(PUP::er &p);
//} LDObjStats;
bool compareLDObjStats(const LDObjStats & a, const LDObjStats & b)
{
  // first compare PE
  int a_pe = a.from_proc, b_pe = b.from_proc;
  if (a_pe != b_pe) return a_pe < b_pe;

  // then compare chare array index
  CmiUInt8 a_id = ck::ObjID(a.data.id()).getElementID();
  CmiUInt8 b_id = ck::ObjID(b.data.id()).getElementID();
  return a_id < b_id;
}

void PrefixLB::work(LDStats* stats)
{
  int obj;
  int n_pes = stats->nprocs();

  //CkPrintf("[%d] PrefixLB strategy\n n_pes = %d; n_objs = %d; n_migrateobjs = %d\n",CkMyPe(), n_pes, stats->n_objs, stats->n_migrateobjs);

  // calculate total wallTime of all objects
  double total_load = 0.0, total_nonmig_load = 0.0;
  int migratable_obj_ct = 0, nonmig_obj_ct = 0;
  std::vector<LDObjStats> objMap;

  for (int obj_idx = 0; obj_idx < stats->objData.size(); obj_idx++){
    LDObjData &oData = stats->objData[obj_idx];
    #if CMK_LB_USER_DATA
    int particle_size = *(int *)oData.getUserData(CkpvAccess(_lb_obj_index));
    CkPrintf("Obj %d, size = %d\n", obj_idx, particle_size);
    #endif
    int pe = stats->from_proc[obj_idx];
    objMap.push_back(LDObjStats{obj_idx, oData, pe, pe});

    if(!oData.migratable){
      nonmig_obj_ct ++;
      total_nonmig_load += oData.wallTime * stats->procs[pe].pe_speed;
    }else{
      migratable_obj_ct ++;
      total_load += oData.wallTime * stats->procs[pe].pe_speed;
    }
  }

  std::sort(objMap.begin(), objMap.end(), compareLDObjStats);

  double avg_load = total_load / n_pes;
  double prefixed_load = 0.0;
  int migrate_ct = 0;
  for (LDObjStats & oStat : objMap){
    int pe = oStat.from_proc;
    prefixed_load += oStat.data.wallTime * stats -> procs[pe].pe_speed;
    int to_pe = std::floor(prefixed_load / avg_load);
    to_pe = std::min(to_pe, n_pes-1);
    stats->assign(oStat.index, to_pe);
    if (pe != to_pe){
      migrate_ct ++;
      //CkPrintf("Move obj[%d] from PE[%d] to PE[%d]\n", oStat.index, pe, to_pe);
    }
  }


  CkPrintf("[%d] PrefixLB strategy moved %d objs\n n_pes = %d; n_objs = %d; n_migrateobjs = %d\n total migratable ct= %d load =  %f; total nonmig ct = %d load = %f\n",CkMyPe(),  migrate_ct, n_pes, stats->objData.size(), stats->n_migrateobjs, migratable_obj_ct, total_load, nonmig_obj_ct, total_nonmig_load);

}

#include "PrefixLB.def.h"

/*@}*/
