/**
 * Author: gplkrsh2@illinois.edu (Harshitha Menon)
 * A distributed load balancer.
*/

#include "DistributedPrefixLB.h"
#include "LBCommon.h"

#include "elements.h"

extern int quietModeRequested;
CkpvExtern(int, _lb_obj_index);

static void lbinit()
{
  LBRegisterBalancer<DistributedPrefixLB>("DistributedPrefixLB", "The distributed load balancer");
}

using std::vector;

DistributedPrefixLB::DistributedPrefixLB(CkMigrateMessage *m) : CBase_DistributedPrefixLB(m) {
}

DistributedPrefixLB::DistributedPrefixLB(const CkLBOptions &opt) : CBase_DistributedPrefixLB(opt) {
  lbname = "DistributedPrefixLB";
  if (CkMyPe() == 0 && !quietModeRequested) {
    CkPrintf("CharmLB> DistributedPrefixLB created: threshold %lf, max phases %i\n",
        kTargetRatio, kMaxPhases);
  }
  #if CMK_LB_USER_DATA
  if (CkpvAccess(_lb_obj_index) == -1)
    CkpvAccess(_lb_obj_index) = LBRegisterObjUserData(sizeof(LBUserData));
  #endif
  InitLB(opt);
}

bool DistPrefixLBCompareLDObjStats(const LDObjStats & a, const LDObjStats & b)
{
  // first compare PE
  int a_pe = a.from_proc, b_pe = b.from_proc;
  if (a_pe != b_pe) return a_pe < b_pe;

  // then compare chare array index
  // to_proc is assigned with chare_idx from user data
  return a.to_proc < b.to_proc;
};

void DistributedPrefixLB::initnodeFn()
{
  _registerCommandLineOpt("+DistLBTargetRatio");
  _registerCommandLineOpt("+DistLBMaxPhases");
}

void DistributedPrefixLB::InitLB(const CkLBOptions &opt) {
  thisProxy = CProxy_DistributedPrefixLB(thisgroup);
  if (opt.getSeqNo() > 0 || (_lb_args.metaLbOn() && _lb_args.metaLbModelDir() != nullptr))
    turnOff();

  // Set constants
  kUseAck = true;
  kPartialInfoCount = -1;
  kMaxPhases = _lb_args.maxDistPhases();
  kTargetRatio = _lb_args.targetRatio();
}

void DistributedPrefixLB::Strategy(const DistBaseLB::LDStats* const stats) {
  if (CkMyPe() == 0 && _lb_args.debug() >= 1) {
    start_time = CmiWallTimer();
    CkPrintf("In DistributedPrefixLB strategy at %lf\n", start_time);
  }

  // Set constants for this iteration (these depend on CkNumPes() or number of
  // objects, so may not be constant for the entire program)
  kMaxObjPickTrials = stats->objData.size();
  // Maximum number of times we will try to find a PE to transfer an object
  // successfully
  kMaxTrials = CkNumPes();
  // Max gossip messages sent from each PE
  kMaxGossipMsgCount = 2 * CmiLog2(CkNumPes());

  // Reset member variables for this LB iteration
  my_stats = stats;

  total_partition_load = 0;
  my_particle_size_sum = 0;
  total_particle_size = 0;
  int st_ct = 0;
  size_map = std::vector<int>(my_stats->objData.size(), 0);
  for (int i = 0; i < my_stats->objData.size(); i++) {
    LDObjData oData = stats->objData[i];
    #if CMK_LB_USER_DATA
    int idx = CkpvAccess(_lb_obj_index);
    LBUserData usr_data = *(LBUserData *)oData.getUserData(CkpvAccess(_lb_obj_index));
    if (usr_data.lb_type == pt){
      // Assign chare_idx at the position of to_proc
      st_ct ++;
      obj_map.push_back(LDObjStats{i, oData, my_stats->from_pe, usr_data.chare_idx});
      size_map[i] = usr_data.particle_size;
      total_partition_load += oData.wallTime * my_stats->pe_speed;
      my_particle_size_sum += usr_data.particle_size;
      if (total_particle_size == 0) total_particle_size = usr_data.particle_sum;
    }
    #endif
  }

  srand((unsigned)(CmiWallTimer()*1.0e06) + CkMyPe());
  //CkPrintf("My pe = %d; st_ct = %d\n", CkMyPe(), st_ct);
  prefix_start_time = CmiWallTimer();
  doPrefix();
  // Not sure if necessary
  std::sort(obj_map.begin(), obj_map.end(), DistPrefixLBCompareLDObjStats);
}

void DistributedPrefixLB::doPrefix(){
  prefixReset();
  thisProxy[thisIndex].prefixStep();
}

void DistributedPrefixLB::prefixReset(){
  total_iter = ceil(log2(CkNumPes()));
  prefix_sum = my_particle_size_sum;
  prefix_stage = 0;
  flag_buf = vector<char>(total_iter, 0);
  value_buf = vector<int>(total_iter, 0);
}

/*
 * Prefix steps
 */
void DistributedPrefixLB::prefixStep(){
  if (prefix_stage >= total_iter){
    CkPrintf("LB[%d] init size = %d; prefix sum = %d\n", thisIndex, my_particle_size_sum, prefix_sum);
    thisProxy[thisIndex].donePrefix();
  }else{
    int send_to_idx = thisIndex + (1 << prefix_stage);
    if(send_to_idx < CkNumPes()){
      thisProxy[send_to_idx].prefixPassValue(prefix_stage, prefix_sum);
    }
    if(flag_buf[prefix_stage]){
      prefix_sum += value_buf[prefix_stage];
      prefix_stage ++;
      thisProxy[thisIndex].prefixStep();
    }else if(thisIndex - (1 << prefix_stage) < 0){
      prefix_stage ++;
      thisProxy[thisIndex].prefixStep();
    }
  }
}

void DistributedPrefixLB::prefixPassValue(int in_stage, int in_val){
  flag_buf[in_stage] = 1;
  value_buf[in_stage] = in_val;
  if (flag_buf[prefix_stage] == 1){
    prefix_sum += value_buf[prefix_stage];
    prefix_stage ++;
    thisProxy[thisIndex].prefixStep();
  }
}

void DistributedPrefixLB::PackAndSendMigrateMsgs() {
  total_migrates = 0;
  int average_size = total_particle_size / CkNumPes();
  int curr_prefix = prefix_sum - my_particle_size_sum;
  vector<MigrateInfo*> migrate_records;
  int from_pe = CkMyPe();
  for (LDObjStats & oStat : obj_map){
    int to_pe = std::min(curr_prefix / average_size, CkNumPes()-1);
    curr_prefix += size_map[oStat.index];
    if (from_pe != to_pe){
      total_migrates ++;
      CkPrintf("LB[%d] move obj[%d] to %d\n", CkMyPe(), oStat.index, to_pe);
      MigrateInfo * move = new MigrateInfo;
      move->obj = oStat.data.handle;
      move->from_pe = from_pe;
      move->to_pe = to_pe;
      migrate_records.push_back(move);
    }
  }
  LBMigrateMsg* msg = new(total_migrates,CkNumPes(),CkNumPes(),0) LBMigrateMsg;
  msg->n_moves = total_migrates;
  for (int i = 0; i < total_migrates; i++){
    msg->moves[i] = *migrate_records[i];
    delete migrate_records[i];
    migrate_records[i] = NULL;
  }
  CkPrintf("LB[%d] move %d / %d  objects\n", CkMyPe(), total_migrates, obj_map.size());
  ProcessMigrationDecision(msg);
  prefixCleanup();
}

void DistributedPrefixLB::donePrefix(){
  if (CkMyPe() == 0 && _lb_args.debug() >= 1) {
    prefix_end_time = CmiWallTimer();
    CkPrintf("DistributedPrefixLB>>Prefix finished at %lf (%lf elapsed)\n",
        prefix_end_time, prefix_end_time - prefix_start_time);
  }
  PackAndSendMigrateMsgs();
}

void DistributedPrefixLB::prefixCleanup(){
  obj_map.clear();
  size_map.clear();
}

#include "DistributedPrefixLB.def.h"
