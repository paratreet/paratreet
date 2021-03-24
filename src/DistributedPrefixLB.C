/**
 * Author: simengl2@illinois.edu (Simeng Liu)
 * A distributed prefix load balancer.
*/

#include "DistributedPrefixLB.h"
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
    CkPrintf("CharmLB> DistributedPrefixLB created.\n");
  }
  #if CMK_LB_USER_DATA
  if (CkpvAccess(_lb_obj_index) == -1)
    CkpvAccess(_lb_obj_index) = LBRegisterObjUserData(sizeof(LBUserData));
  #endif
  InitLB(opt);
}

bool DistPrefixLBCompare(const LBCompareStats & a, const LBCompareStats & b)
{
  // compare chare array index
  return a.chare_idx < b.chare_idx;
};

void DistributedPrefixLB::initnodeFn()
{
  //_registerCommandLineOpt("+DistLBTargetRatio");
  //_registerCommandLineOpt("+DistLBMaxPhases");
}

void DistributedPrefixLB::InitLB(const CkLBOptions &opt) {
  thisProxy = CProxy_DistributedPrefixLB(thisgroup);
  if (opt.getSeqNo() > 0 || (_lb_args.metaLbOn() && _lb_args.metaLbModelDir() != nullptr))
    turnOff();
}

void DistributedPrefixLB::Strategy(const DistBaseLB::LDStats* const stats) {
  if (CkMyPe() == 0 && _lb_args.debug() >= 1) {
    start_time = CmiWallTimer();
    CkPrintf("In DistributedPrefixLB strategy at %lf\n", start_time);
  }

  // Reset member variables for this LB iteration
  my_stats = stats;
  my_pe = CkMyPe();

  initVariables();

  createObjMaps();
  prefixInit();

  if (st_ct > 0) std::sort(st_obj_map.begin(), st_obj_map.end(), DistPrefixLBCompare);
  if (pt_ct > 0) std::sort(pt_obj_map.begin(), pt_obj_map.end(), DistPrefixLBCompare);

  if (_lb_args.debug() >= 2) CkPrintf("My pe = %d; st_ct = %d; total_st_particle_size = %d; pt_ct = %d total_pt_particle_size = %d \n", my_pe, st_ct, st_particle_size_sum, pt_ct, pt_particle_size_sum);
  prefix_start_time = CmiWallTimer();

  thisProxy[0].reportInitDone();
}

void DistributedPrefixLB::reportInitDone(){
  total_init_complete ++;
  if(total_init_complete == CkNumPes()){
    CkPrintf("DistributedPrefixLB>>> Initialization done. Start prefix\n");
    total_init_complete = 0;
    for (int i = 0; i < CkNumPes(); i++)
      thisProxy[i].prefixStep();
  }
}

void DistributedPrefixLB::initVariables(){
  // Used in PE[0] only
  total_prefix_moves = 0;
  total_prefix_objects = 0;
  recv_prefix_summary_ct = 0;

  total_incoming_prefix_migrations = 0;

  total_partition_load = 0.0;
  total_particle_size = 0;
}

void DistributedPrefixLB::createObjMaps(){
  st_ct = 0;
  pt_ct = 0;
  st_particle_size_sum = 0;
  pt_particle_size_sum = 0;
  for (int i = 0; i < my_stats->objData.size(); i++) {
    LDObjData oData = my_stats->objData[i];
    #if CMK_LB_USER_DATA
    int idx = CkpvAccess(_lb_obj_index);
    LBUserData usr_data = *(LBUserData *)oData.getUserData(CkpvAccess(_lb_obj_index));
    // Collect Subtree chare element data
    if (usr_data.lb_type == LBCommon::st){
      st_ct ++;
      st_obj_map.push_back(
          LBCompareStats{
            i, usr_data.chare_idx,
            usr_data.particle_size,
            &my_stats->objData[i], my_pe, usr_data.centroid
          });
  if(_lb_args.debug() > 3) ckout << "ST[" << usr_data.chare_idx << "] @ PE[" << my_pe << "] size = "<< usr_data.particle_size <<"; centroid: " << usr_data.centroid << endl;
      total_partition_load += oData.wallTime * my_stats->pe_speed;
      st_particle_size_sum += usr_data.particle_size;
      if (total_particle_size == 0) total_particle_size = usr_data.particle_sum;
    }
    // Collect Partition chare element data
    else if (usr_data.lb_type == LBCommon::pt){
      pt_ct ++;
      pt_obj_map.push_back(
          LBCompareStats{
            i, usr_data.chare_idx,
            usr_data.particle_size,
            &my_stats->objData[i], my_pe, usr_data.centroid
          });
    if(_lb_args.debug() == 4) ckout << "PT[" << usr_data.chare_idx << "] @ PE[" << my_pe << "] size = "<< usr_data.particle_size <<"; centroid: " << usr_data.centroid << endl;
      total_partition_load += oData.wallTime * my_stats->pe_speed;
      pt_particle_size_sum += usr_data.particle_size;
    }
    #endif
  }
}

void DistributedPrefixLB::prefixInit(){
  total_iter = ceil(log2(CkNumPes()));
  if(my_pe == 0) CkPrintf("total_iter = %d\n", total_iter);
  // When Subtree and Partition are bounded
  my_particle_sum = st_particle_size_sum;
  obj_map_to_balance = st_obj_map;
  prefix_sum = my_particle_sum;
  prefix_stage = 0;
  prefix_done = false;
  flag_buf = vector<char>(total_iter, 0);
  value_buf = vector<int>(total_iter, 0);
  update_tracker = vector<char>(total_iter, 0);
  send_tracker = vector<char>(total_iter, 0);
  prefix_migrate_out_ct = vector<int>(CkNumPes(), 0);
  recv_prefix_move_pes = 0;
}


/*
 * Prefix steps
 */
void DistributedPrefixLB::prefixStep(){
  //CkPrintf("LB<%d> @step %d; prefix = %d\n", my_pe, prefix_stage, prefix_sum);
  if (prefix_stage >= total_iter && (!prefix_done)){
    prefix_done = true;
    if (_lb_args.debug() >= 2) CkPrintf("LB[%d] init size = %d; prefix sum = %d\n", my_pe, my_particle_sum, prefix_sum);
    donePrefix();
  }else{
    int send_to_idx = my_pe + (1 << prefix_stage);
    if(send_to_idx < CkNumPes() && !send_tracker[prefix_stage]){
      //CkPrintf("LB<%d> @step %d; send to [%d]  prefix = %d\n", my_pe, prefix_stage, send_to_idx, prefix_sum);
      send_tracker[prefix_stage] = 1;
      thisProxy[send_to_idx].prefixPassValue(prefix_stage, prefix_sum);
    }
    if(flag_buf[prefix_stage] && !update_tracker[prefix_stage]){
      //CkPrintf("LB<%d> @step %d update\n", my_pe, prefix_stage);
      prefix_sum += value_buf[prefix_stage];
      update_tracker[prefix_stage] = 1;
      prefix_stage ++;
      thisProxy[my_pe].prefixStep();
    }else if(my_pe - (1 << prefix_stage) < 0 && !update_tracker[prefix_stage]){
      //CkPrintf("LB<%d> @step %d update\n", my_pe, prefix_stage);
      update_tracker[prefix_stage] = 1;
      prefix_stage ++;
      thisProxy[my_pe].prefixStep();
    }
  }
}

void DistributedPrefixLB::prefixPassValue(int in_stage, int in_val){
  //CkPrintf("LB[%d] in_stage = %d; in_val = %d\n", my_pe, in_stage, in_val);
  if(!flag_buf[in_stage]){
    flag_buf[in_stage] = 1;
    value_buf[in_stage] = in_val;
    thisProxy[my_pe].prefixStep();
  }
}

void DistributedPrefixLB::acknowledgeIncomingPrefixMigrations(int nMoves){
  total_incoming_prefix_migrations += nMoves;
  recv_prefix_move_pes += 1;
  if(recv_prefix_move_pes == CkNumPes()){
    PackAndMakeMigrateMsgs(total_migrates, obj_map_to_balance.size());
  }
}

void DistributedPrefixLB::sendOutPrefixDecisions(){
  for (int i = 0; i < CkNumPes(); i++){
    thisProxy[i].acknowledgeIncomingPrefixMigrations(prefix_migrate_out_ct[i]);
    if(_lb_args.debug() >= 3) CkPrintf("[%d] send %d to [%d]\n", CkMyPe(), prefix_migrate_out_ct[i], i);
  }
}

void DistributedPrefixLB::makePrefixMoves(){
  total_migrates = 0;
  int average_size = total_particle_size / CkNumPes();
  int curr_prefix = prefix_sum - my_particle_sum;
  for (LBCompareStats & oStat : obj_map_to_balance){
    int to_pe = std::min(curr_prefix / average_size, CkNumPes()-1);
    curr_prefix += oStat.particle_size;
    if (my_pe != to_pe){
      total_migrates ++;
      MigrateInfo * move = new MigrateInfo;
      move->obj = oStat.data_ptr->handle;
      move->from_pe = my_pe;
      move->to_pe = to_pe;
      //if(_lb_args.debug()) CkPrintf("LB[%d] send to %d\n", my_pe, to_pe);
      migrate_records.push_back(move);
      prefix_migrate_out_ct[to_pe] += 1;
    }
  }

  sendOutPrefixDecisions();
}

void DistributedPrefixLB::PackAndMakeMigrateMsgs(int num_moves, int total_ct) {
  // Make sure that one chare at least has one obj
  // Otherwise, will cause segfault

  if(_lb_args.debug() >= 2) CkPrintf("LB[%d] PackAndMakeMigrateMsgs num_moves = %d, total_ct = %d\n", my_pe, num_moves, total_ct);
  if (num_moves == total_ct) {
    if(_lb_args.debug() >= 2) CkPrintf("LB[%d] move all out\n", my_pe);
    num_moves -= 1;
    delete migrate_records[num_moves];
    migrate_records[num_moves] = NULL;
  }

  final_migration_msg = new(num_moves,CkNumPes(),CkNumPes(),0) LBMigrateMsg;
  final_migration_msg->n_moves = num_moves;
  for (int i = 0; i < num_moves; i++){
    final_migration_msg->moves[i] = *migrate_records[i];
    delete migrate_records[i];
    migrate_records[i] = NULL;
  }

  if (_lb_args.debug() > 2) CkPrintf("LB[%d] move %d / %d  objects\n", my_pe, num_moves, total_ct);
  thisProxy[0].sendPrefixSummary(num_moves, total_ct);
  sendMigrateMsgs();
}

void DistributedPrefixLB::sendMigrateMsgs(){
  if(_lb_args.debug() >= 2) CkPrintf("LB[%d] sendMigrateMsgs\n", my_pe);
  migrates_expected = total_incoming_prefix_migrations;
  cleanUp();
  ProcessMigrationDecision(final_migration_msg);
}

void DistributedPrefixLB::donePrefix(){
  if (my_pe + 1 == CkNumPes() && _lb_args.debug() >= 1) {
    prefix_end_time = CmiWallTimer();
    CkPrintf("DistributedPrefixLB>> Prefix finished at %lf (%lf ms elapsed)\n",
        prefix_end_time, (prefix_end_time - prefix_start_time)*1000);
  }
  makePrefixMoves();
}

void DistributedPrefixLB::sendPrefixSummary(int num_moves, int base_ct){
  total_prefix_moves += num_moves;
  total_prefix_objects += base_ct;
  recv_prefix_summary_ct += 1;

  if (recv_prefix_summary_ct == CkNumPes()){
    CkPrintf("DistributedPrefixLB>> Prefix Summary:: moved %d/%d objects\n", total_prefix_moves, total_prefix_objects);
  }
}

void DistributedPrefixLB::cleanUp(){
  if(pt_obj_map.size() > 0) pt_obj_map.clear();
  if(st_obj_map.size() > 0) st_obj_map.clear();
  if(obj_map_to_balance.size() > 0) obj_map_to_balance.clear();

  if(flag_buf.size() > 0){
    flag_buf.clear();
    value_buf.clear();
    update_tracker.clear();
    send_tracker.clear();
  }

  migrate_records.clear();
}

#include "DistributedPrefixLB.def.h"
