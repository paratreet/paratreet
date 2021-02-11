/**
 * \addtogroup CkLdb
*/
/*@{*/

#include "elements.h"
#include "ckheap.h"
#include "PrefixLB.h"

extern int quietModeRequested;

CreateLBFunc_Def(PrefixLB, "Move jobs base on prefix sum.")

PrefixLB::PrefixLB(const CkLBOptions &opt): CBase_PrefixLB(opt)
{
  lbname = (char *)"PrefixLB";
  if (CkMyPe() == 0 && !quietModeRequested)
    CkPrintf("CharmLB> PrefixLB created.\n");
}

void PrefixLB::work(LDStats* stats)
{
  int obj;
  int n_pes = stats->nprocs();

  //CkPrintf("[%d] PrefixLB strategy\n n_pes = %d; n_objs = %d; n_migrateobjs = %d\n",CkMyPe(), n_pes, stats->n_objs, stats->n_migrateobjs);

  // calculate total wallTime of all objects
  double total_load = 0.0, total_nonmig_load = 0.0;
  int migratable_obj_ct = 0, nonmig_obj_ct = 0;
  int prefixed_load[stats->n_objs];

  for (int obj_idx = 0; obj_idx < stats->n_objs; obj_idx++){
    LDObjData &oData = stats->objData[obj_idx];
    int pe = stats->from_proc[obj_idx];
    stats->assign(obj_idx, pe);
    if(!oData.migratable){
      nonmig_obj_ct ++;
      total_nonmig_load += oData.wallTime * stats->procs[pe].pe_speed;
    }else{
      migratable_obj_ct ++;
      total_load += oData.wallTime * stats->procs[pe].pe_speed;
      prefixed_load[obj_idx] = total_load;
    }
  }

  double avg_load = total_load / n_pes;
  int migrate_ct = 0;
  for (int obj_idx = 0; obj_idx < stats->n_objs; obj_idx++){
    int pe = stats->from_proc[obj_idx];
    int to_pe = std::floor(prefixed_load[obj_idx] / avg_load);
    to_pe = std::min(to_pe, n_pes-1);
    stats->assign(obj_idx, to_pe);
    if (pe != to_pe){
      migrate_ct ++;
      CkPrintf("Move obj[%d] from PE[%d] to PE[%d]\n", obj_idx, pe, to_pe);
    }
  }


  CkPrintf("[%d] PrefixLB strategy moved %d objs\n n_pes = %d; n_objs = %d; n_migrateobjs = %d\n total migratable ct= %d load =  %f; total nonmig ct = %d load = %f\n",CkMyPe(),  migrate_ct, n_pes, stats->n_objs, stats->n_migrateobjs, migratable_obj_ct, total_load, nonmig_obj_ct, total_nonmig_load);

}

#include "PrefixLB.def.h"

/*@}*/
