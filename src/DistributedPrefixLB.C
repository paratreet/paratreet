/**
 * Author: gplkrsh2@illinois.edu (Harshitha Menon)
 * A distributed load balancer.
*/

#include "DistributedPrefixLB.h"
#include "elements.h"
#include "SFC.h"

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

bool DistPrefixLBCompare(const LBCompareStats & a, const LBCompareStats & b)
{
  // compare chare array index
  // to_proc is assigned with chare_idx from user data
  return a.chare_idx < b.chare_idx;
};

bool DistPrefixLBCompareWithSFCKey(const LBCompareStats & a, const LBCompareStats & b)
{
  // then compare chare array index
  // to_proc is assigned with chare_idx from user data
  return a.sfc_key < b.sfc_key;
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
    CkPrintf("In DistributedPrefixLB strategy at %lf; iteration to balance %s\n", start_time, (balance_partition? "partition" : "subtree"));
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
  my_pe = CkMyPe();

  initVariables();

  createObjMaps();

  if (st_ct > 0) std::sort(st_obj_map.begin(), st_obj_map.end(), DistPrefixLBCompare);
  if (pt_ct > 0) std::sort(pt_obj_map.begin(), pt_obj_map.end(), DistPrefixLBCompare);

  srand((unsigned)(CmiWallTimer()*1.0e06) + my_pe);
  if (_lb_args.debug() >= 1) CkPrintf("My pe = %d; st_ct = %d; total_st_particle_size = %d; pt_ct = %d total_pt_particle_size = %d \n", my_pe, st_ct, st_particle_size_sum, pt_ct, pt_particle_size_sum);
  prefix_start_time = CmiWallTimer();

  if (balance_partition) doPrefix();
  else doSubtreeMove();
}

void DistributedPrefixLB::initVariables(){
  total_prefix_moves = 0;
  total_prefix_base = 0;
  recv_prefix_summary_ct = 0;
  total_partition_load = 0;
  total_particle_size = 0;
  recv_pe_ct = 0;
  recv_pe_migrates_ct = 0;

  recv_closest_pe_migrates_ct = std::vector<int>(CkNumPes(), 0);
  recv_closest_pt_migrates_ct = std::vector<int>(CkNumPes(), 0);

  global_pt_centroids.resize(CkNumPes());
  global_pe_centroids.resize(CkNumPes());

  closest_pe_migrates = 0;
  closest_pe_migrates_ct = std::vector<int>(CkNumPes(), 0);
  closest_pt_migrates = 0;
  closest_pt_migrates_ct = std::vector<int>(CkNumPes(), 0);
}

void DistributedPrefixLB::createObjMaps(){
  st_ct = 0;
  pt_ct = 0;
  st_particle_size_sum = 0;
  pt_particle_size_sum = 0;
  Vector3D<Real> avg_centroid;
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
            usr_data.sfc_key, usr_data.particle_size,
            &my_stats->objData[i], my_pe, usr_data.centroid
          });
      if(_lb_args.debug() == 1) ckout << "ST[" << usr_data.chare_idx << "] @ PE[" << my_pe << "] size = "<< usr_data.particle_size <<"; centroid: " << usr_data.centroid << endl;
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
            usr_data.sfc_key, usr_data.particle_size,
            &my_stats->objData[i], my_pe, usr_data.centroid
          });
      if(_lb_args.debug() == 1) ckout << "PT[" << usr_data.chare_idx << "] @ PE[" << my_pe << "] size = "<< usr_data.particle_size <<"; centroid: " << usr_data.centroid << endl;
      total_partition_load += oData.wallTime * my_stats->pe_speed;
      pt_particle_size_sum += usr_data.particle_size;

      // If balance subtree, create vector of LBCentroidData and broadcast to assign subtree chares to the the pe with the closest Treepiece
      if(!balance_partition){
        avg_centroid += usr_data.centroid;
        local_pt_centroids.push_back(usr_data.centroid);
      }
    }
    #endif
  }

  pe_pt_centroid = avg_centroid / (Real)pt_ct;
}

void DistributedPrefixLB::doSubtreeMove(){
  broadcastPECentroid();
}

void DistributedPrefixLB::makeSubtreeMoves(){
  moveSubtreeToClosestPE();
  moveSubtreeToClosestPartition();
  broadcastSubtreeMigrateCts();
}

void DistributedPrefixLB::getOtherPECentroids(vector<Vector3D<Real>> centroids,Vector3D<Real> pe_pt_centroid_, int in_pe){
  global_pt_centroids[in_pe] = centroids;
  global_pe_centroids[in_pe] = pe_pt_centroid_;
  global_pt_ct += centroids.size();
  recv_pe_ct ++;

  if (recv_pe_ct == CkNumPes()){
    makeSubtreeMoves();
  }
}

void DistributedPrefixLB::broadcastPECentroid(){
  for (int i = 0; i < CkNumPes(); i++){
    thisProxy[i].getOtherPECentroids(local_pt_centroids, pe_pt_centroid, my_pe);
  }
}

int DistributedPrefixLB::findPEWithClosestTP(Vector3D<Real> c){
  int to_pe = 0;
  Real min_len_squared = std::numeric_limits<Real>::max();

  for (int i = 0; i < CkNumPes(); i++){
    for (auto centd : global_pt_centroids[i]){
      Real curr_delta = (centd - c).lengthSquared();
      if (curr_delta < min_len_squared){
        to_pe = i;
        min_len_squared = curr_delta;
      }
    }
  }
  return to_pe;
}

int DistributedPrefixLB::findPEWithClosestAvgCentroid(Vector3D<Real> c){
  int to_pe = 0;
  Real min_len_squared = std::numeric_limits<Real>::max();

  for (int i = 0; i < CkNumPes(); i++){
    Real curr_delta = (global_pe_centroids[i] - c).lengthSquared();
    if (curr_delta < min_len_squared){
      to_pe = i;
      min_len_squared = curr_delta;
    }
  }
  return to_pe;
}

void DistributedPrefixLB::moveSubtreeToClosestPartition(){
  vector<MigrateInfo*> migrate_records;

  for (LBCompareStats & oStat : st_obj_map){
    int to_pe = findPEWithClosestTP(oStat.centroid);
    if (my_pe != to_pe){
      closest_pt_migrates ++;
      MigrateInfo * move = new MigrateInfo;
      move->obj = oStat.data_ptr->handle;
      move->from_pe = my_pe;
      move->to_pe = to_pe;
      migrate_records.push_back(move);
    }
    closest_pt_migrates_ct[to_pe] ++;
  }

  PackAndMakeMigrateMsgs(closest_pt_migrates, st_ct, migrate_records);
}

void DistributedPrefixLB::moveSubtreeToClosestPE(){
  for (LBCompareStats & oStat : st_obj_map){
    int to_pe = findPEWithClosestAvgCentroid(oStat.centroid);
    if (my_pe != to_pe){
      closest_pe_migrates ++;
    }
    closest_pe_migrates_ct[to_pe] ++;
  }
}

void DistributedPrefixLB::send_migrate_cts(int closest_pt, int closest_pe, int in_pe){
    recv_closest_pe_migrates_ct[in_pe] = closest_pe;
    recv_closest_pt_migrates_ct[in_pe] = closest_pt;
    recv_pe_migrates_ct ++;

    if(recv_pe_migrates_ct == CkNumPes()){
      subtreeMigrationSummary();
    }
}

void DistributedPrefixLB::broadcastSubtreeMigrateCts(){
  for (int i = 0; i < CkNumPes(); i++){
    thisProxy[i].send_migrate_cts(closest_pt_migrates_ct[i], closest_pe_migrates_ct[i], my_pe);
  }
}

void DistributedPrefixLB::subtreeMigrationSummary(){
  int total_pt_out = 0, total_pt_in = 0, total_pe_out = 0, total_pe_in = 0, final_pt = 0, final_pe = 0;
  for (int i = 0; i < CkNumPes(); i++){
    if(i != my_pe) {
      total_pt_out += closest_pt_migrates_ct[i];
      total_pt_in  += recv_closest_pt_migrates_ct[i];
      total_pe_out += closest_pe_migrates_ct[i];
      total_pe_in  += recv_closest_pe_migrates_ct[i];
    }
  }
  final_pt = closest_pt_migrates_ct[my_pe] + total_pt_in;
  final_pe = closest_pe_migrates_ct[my_pe] + total_pe_in;
  if(_lb_args.debug() >= 1) CkPrintf("LB[%02d] Origin:%02d, pt_out = %02d; pt_in = %02d; pt_remain = %02d; pe_out = %02d; pe_in=%02d; pe_remain = %02d; pt_final=%02d; pe_final=%02d\n", my_pe, st_ct, total_pt_out, total_pt_in, closest_pt_migrates_ct[my_pe], total_pe_out, total_pe_in, closest_pe_migrates_ct[my_pe], final_pt, final_pe);
}

void DistributedPrefixLB::doPrefix(){
  prefixReset();
  thisProxy[my_pe].prefixStep();
}

void DistributedPrefixLB::prefixReset(){
  total_iter = ceil(log2(CkNumPes()));
  // When Subtree and Partition are bounded
  my_particle_sum = (pt_ct > 0)? pt_particle_size_sum : st_particle_size_sum;
  obj_map_to_balance = (pt_ct > 0)? pt_obj_map : st_obj_map;
  prefix_sum = my_particle_sum;
  prefix_stage = 0;
  prefix_done = false;
  flag_buf = vector<char>(total_iter, 0);
  value_buf = vector<int>(total_iter, 0);
  update_tracker = vector<char>(total_iter, 0);
  send_tracker = vector<char>(total_iter, 0);
}

/*
 * Prefix steps
 */
void DistributedPrefixLB::prefixStep(){
  //CkPrintf("LB<%d> @step %d; prefix = %d\n", my_pe, prefix_stage, prefix_sum);
  if (prefix_stage >= total_iter && (!prefix_done)){
    prefix_done = true;
    if (_lb_args.debug() >= 3) CkPrintf("LB[%d] init size = %d; prefix sum = %d\n", my_pe, my_particle_sum, prefix_sum);
    thisProxy[my_pe].donePrefix();
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
  if(!flag_buf[in_stage]){
    flag_buf[in_stage] = 1;
    value_buf[in_stage] = in_val;
    thisProxy[my_pe].prefixStep();
  }
}

void DistributedPrefixLB::makePrefixMoves(){
  total_migrates = 0;
  int average_size = total_particle_size / CkNumPes();
  int curr_prefix = prefix_sum - my_particle_sum;
  vector<MigrateInfo*> migrate_records;
  for (LBCompareStats & oStat : obj_map_to_balance){
    int to_pe = std::min(curr_prefix / average_size, CkNumPes()-1);
    curr_prefix += oStat.particle_size;
    if (my_pe != to_pe){
      total_migrates ++;
      MigrateInfo * move = new MigrateInfo;
      move->obj = oStat.data_ptr->handle;
      move->from_pe = my_pe;
      move->to_pe = to_pe;
      if(_lb_args.debug() >= 2) CkPrintf("LB[%d] send to %d\n", my_pe, to_pe);
      migrate_records.push_back(move);
    }
  }
  PackAndMakeMigrateMsgs(total_migrates, obj_map_to_balance.size(), migrate_records);
}

void DistributedPrefixLB::PackAndMakeMigrateMsgs(int num_moves, int total_ct, std::vector<MigrateInfo*> moves) {
  if (num_moves == total_ct) {
    num_moves -= 1;
    delete moves[num_moves];
    moves[num_moves] = NULL;
  }

  final_migration_msg = new(num_moves,CkNumPes(),CkNumPes(),0) LBMigrateMsg;
  final_migration_msg->n_moves = num_moves;
  for (int i = 0; i < num_moves; i++){
    final_migration_msg->moves[i] = *moves[i];
    delete moves[i];
    moves[i] = NULL;
  }

  cleanUp();
  if (_lb_args.debug() >= 2) CkPrintf("LB[%d] move %d / %d  objects\n", my_pe, num_moves, total_ct);
  if(!balance_partition){
    sendMigrateMsgs();
  }else if (my_pe != 0){
    thisProxy[0].sendPrefixSummary(num_moves, total_ct);
    sendMigrateMsgs();
  }
}

void DistributedPrefixLB::sendMigrateMsgs(){
  if(pt_ct > 0) balance_partition = !balance_partition;
  migrates_expected = final_migration_msg->n_moves;
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
  total_prefix_base += base_ct;
  recv_prefix_summary_ct ++;

  if (recv_prefix_summary_ct + 1 == CkNumPes()){
    CkPrintf("DistributedPrefixLB>> Prefix Summary:: moved %d/%d objects\n", total_prefix_moves + total_migrates, total_prefix_base + obj_map_to_balance.size());
    sendMigrateMsgs();
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

  local_pt_centroids.clear();
  global_pt_centroids.clear();
  global_pe_centroids.clear();
  closest_pt_migrates_ct.clear();
  closest_pe_migrates_ct.clear();
  recv_closest_pe_migrates_ct.clear();
  recv_closest_pt_migrates_ct.clear();
}

#include "DistributedPrefixLB.def.h"
