/**
 * Author: simengl2@illinois.edu (Simeng Liu)
 * A distributed prefix load balancer.
*/

#include "DistributedOrbLB.h"
#include "elements.h"

extern int quietModeRequested;
CkpvExtern(int, _lb_obj_index);

static void lbinit()
{/*{{{*/
  LBRegisterBalancer<DistributedOrbLB>("DistributedOrbLB", "The distributed orb3d load balancer");/*}}}*/
}

DistributedOrbLB::DistributedOrbLB(CkMigrateMessage *m) : CBase_DistributedOrbLB(m) {
}

DistributedOrbLB::DistributedOrbLB(const CkLBOptions &opt) : CBase_DistributedOrbLB(opt) {
  lbname = "DistributedOrbLB";/*{{{*/
  if (CkMyPe() == 0 && !quietModeRequested) {
    CkPrintf("CharmLB> DistributedOrbLB created.\n");
  }
  #if CMK_LB_USER_DATA
  if (CkpvAccess(_lb_obj_index) == -1)
    CkpvAccess(_lb_obj_index) = LBRegisterObjUserData(sizeof(LBUserData));
  #endif
  InitLB(opt);/*}}}*/
}

void DistributedOrbLB::initnodeFn()
{
  //_registerCommandLineOpt("+DistLBBackgroundRatio");
  //_registerCommandLineOpt("+DistLBMaxPhases");
}

void DistributedOrbLB::InitLB(const CkLBOptions &opt) {
  thisProxy = CProxy_DistributedOrbLB(thisgroup);/*{{{*/
  if (opt.getSeqNo() > 0 || (_lb_args.metaLbOn() && _lb_args.metaLbModelDir() != nullptr))
    turnOff();
  //nearestK = _lb_args.maxDistPhases();
  /*}}}*/
}

void DistributedOrbLB::Strategy(const DistBaseLB::LDStats* const stats) {
  start_time = CmiWallTimer();
  if (CkMyPe() == 0) {
    CkPrintf("DistributedOrbLB>> In DistributedOrbLB strategy at %lf\n", start_time);
  }

  // Reset member variables for this LB iteration
  my_stats = stats;
  my_pe = CkMyPe();
  background_load = my_stats->bg_walltime * my_stats->pe_speed;

  initVariables();
  parseLBData();
  reportPerLBStates();
  reportUniverseDimensions();
}


void DistributedOrbLB::initVariables(){
  total_load = .0f;
  total_migrates = 0;
  incoming_migrates = 0;
  recv_ack = 0;
  recv_final = 0;
  incoming_final = 0;
  n_partitions = 0;
  n_particles = 0;
  max_obj_load = .0f;
  min_obj_load = 10.0;
  dim = 0;
  curr_dim = -1;
  use_longest_dim = true;
  obj_coords = vector<vector<float>>(3, vector<float>());
  pe_split_coords.resize(CkNumPes());
  pe_split_loads = vector<float>(CkNumPes(), .0f);
  universe_coords = vector<float>(6, .0f);

  recv_summary = 0;
  total_move = 0;
  total_objs = 0;

  global_octal_loads = vector<float>(8, .0f);
  global_octal_sizes = vector<int>(8, 0);

  send_nobjs_to_pes = vector<int>(CkNumPes(), 0);
}

void DistributedOrbLB::parseLBData(){
  for (int i = 0; i < my_stats->objData.size(); i++) {/*{{{*/
    LDObjData oData = my_stats->objData[i];
    #if CMK_LB_USER_DATA
    int idx = CkpvAccess(_lb_obj_index);
    float o_load = oData.wallTime * my_stats->pe_speed;
    LBUserData usr_data = *(LBUserData *)oData.getUserData(CkpvAccess(_lb_obj_index));

    // Collect Partition chare element data
    if (usr_data.lb_type == LBCommon::pt){
      obj_load += o_load;
      n_partitions ++;
      obj_collection.push_back(
          LBCentroidAndIndexRecord{
          usr_data.centroid, i,
          usr_data.particle_size, my_pe,
          o_load, .0f, 0,
          &my_stats->objData[i]
          });
      n_particles += usr_data.particle_size;

      obj_coords[0].push_back(usr_data.centroid[0]);
      obj_coords[1].push_back(usr_data.centroid[1]);
      obj_coords[2].push_back(usr_data.centroid[2]);

      if(_lb_args.debug() >= 3) ckout << usr_data.centroid <<endl;

      if(_lb_args.debug() == 4) ckout << "PT[" << usr_data.chare_idx << "] @ PE[" << my_pe << "] size = "<< usr_data.particle_size <<"; centroid: " << usr_data.centroid << endl;
    } else {
      background_load += o_load;
    }
    #endif
  }

  bgload_per_obj = background_load / n_partitions;

  //if (_lb_args.debug() == 2 && my_pe == 0){
  //  CkPrintf("obj[0] load w/o bg: %f\n", obj_collection[0].load);
  //}
  for (auto & obj : obj_collection){
    obj.load += bgload_per_obj;
    total_load += obj.load;
    if (obj.load > max_obj_load) max_obj_load = obj.load;
    if (obj.load < min_obj_load) min_obj_load = obj.load;
  }
  //if (_lb_args.debug() == 2 && my_pe == 0){
  //  CkPrintf("obj[0] load with bg: %f\n", obj_collection[0].load);
  //}}}}
}

int DistributedOrbLB::getDim(int dim, Vector3D<Real>& lower_coords, Vector3D<Real>& upper_coords){
  if (!use_longest_dim){/*{{{*/
    return (dim + 1) % 3;
  } else {
    int d = 0;
    float delta = upper_coords[0] - lower_coords[0];
    for (int i = 1; i < 3; i++){
      float tmp_d = upper_coords[i] - lower_coords[i];
      if (tmp_d > delta) {
        delta = tmp_d;
        d = i;
      }
    }
    return d;
  }

  return -1;/*}}}*/
}

void DistributedOrbLB::reportPerLBStates(){
  int tupleSize = 4;/*{{{*/
  CkReduction::tupleElement tupleRedn[] = {
    CkReduction::tupleElement(sizeof(float), &total_load, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &total_load, CkReduction::sum_float),
    CkReduction::tupleElement(sizeof(float), &max_obj_load, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &min_obj_load, CkReduction::min_float),
};
  CkReductionMsg* msg = CkReductionMsg::buildFromTuple(tupleRedn, tupleSize);
  CkCallback cb(CkIndex_DistributedOrbLB::perLBStates(NULL), thisProxy[0]);
  msg->setCallback(cb);
  contribute(msg);/*}}}*/
}

void DistributedOrbLB::perLBStates(CkReductionMsg * msg){
  int numReductions;/*{{{*/
  CkReduction::tupleElement* results;

  msg->toTuple(&results, &numReductions);
  float max_load = *(float*)results[0].data;
  float sum_load = *(float*)results[1].data;
  global_max_obj_load = *(float*)results[2].data;
  global_min_obj_load = *(float*)results[3].data;
  delete [] results;
  global_load = sum_load;
  granularity = global_max_obj_load / global_load;

  float avg_load = sum_load / (float)CkNumPes();
  CkPrintf("DistributedOrbLB >> Pre LB load sum = %.4f; max / avg load ratio = %.4f\n", sum_load, max_load / avg_load);/*}}}*/
  CkPrintf("\t\t(%.8f, %.8f)\n", global_min_obj_load / global_load, granularity);/*}}}*/
}

void DistributedOrbLB::reportUniverseDimensions(){
  int tupleSize = 6;/*{{{*/
  float min_xdim = *std::min_element(obj_coords[0].begin(), obj_coords[0].end());
  float max_xdim = *std::max_element(obj_coords[0].begin(),  obj_coords[0].end());
  float min_ydim = *std::min_element(obj_coords[1].begin(),  obj_coords[1].end());
  float max_ydim = *std::max_element(obj_coords[1].begin(),  obj_coords[1].end());
  float min_zdim = *std::min_element(obj_coords[2].begin(),  obj_coords[2].end());
  float max_zdim = *std::max_element(obj_coords[2].begin(),  obj_coords[2].end());

  CkReduction::tupleElement tupleRedn[] = {
    CkReduction::tupleElement(sizeof(float), &min_xdim, CkReduction::min_float),
    CkReduction::tupleElement(sizeof(float), &max_xdim, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &min_ydim, CkReduction::min_float),
    CkReduction::tupleElement(sizeof(float), &max_ydim, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &min_zdim, CkReduction::min_float),
    CkReduction::tupleElement(sizeof(float), &max_zdim, CkReduction::max_float),
};

  CkReductionMsg* msg = CkReductionMsg::buildFromTuple(tupleRedn, tupleSize);
  CkCallback cb(CkIndex_DistributedOrbLB::getUniverseDimensions(NULL), thisProxy[0]);
  msg->setCallback(cb);
  contribute(msg);
  /*}}}*/
}

void DistributedOrbLB::getUniverseDimensions(CkReductionMsg * msg){
  int numReductions;/*{{{*/
  CkReduction::tupleElement* results;

  if(_lb_args.debug() >= debug_l0) ckout << "Universe coord :: ";
  msg->toTuple(&results, &numReductions);
  float pad = .1e-5;
  for (int i = 0; i < 6; i++){
    universe_coords[i] = *(float*)results[i].data;
    if (i % 2) universe_coords[i] += pad;
    if(_lb_args.debug() >= debug_l0) ckout << universe_coords[i] << "; ";
  }
  if(_lb_args.debug() >= debug_l0) ckout << endl;
  delete [] results;

  findPeSplitPoints();

  /*}}}*/
}

void DistributedOrbLB::findPeSplitPoints(){
  curr_lower_coords = Vector3D<Real>(universe_coords[0], universe_coords[2], universe_coords[4]);
  curr_upper_coords = Vector3D<Real>(universe_coords[1], universe_coords[3], universe_coords[5]);
  curr_depth = 0;

  createPartitions(getDim(-1, curr_lower_coords, curr_upper_coords), global_load, 0, CkNumPes(), curr_lower_coords, curr_upper_coords);

  for (int i = 0; i < CkNumPes(); i++){
    if(_lb_args.debug() >= debug_l0) ckout << "@@@@ Final [" <<i << "] " << pe_split_loads[i]
      << " (" << pe_split_coords[i][0] << " , "<< pe_split_coords[i][1] << ")" <<endl;
  }

  thisProxy.migrateObjects(pe_split_coords);
}

void DistributedOrbLB::createPartitions(int dim, float load, int left, int right, Vector3D<Real> lower_coords, Vector3D<Real> upper_coords){
  if(_lb_args.debug() >= debug_l0) CkPrintf("\n\n==== createPartitions dim %d load %.4f left %d right %d ====\n", dim, load, left, right);
  if(_lb_args.debug() >= debug_l0) ckout << lower_coords << " - " << upper_coords << endl;
  left_idx = left;
  right_idx = right;
  curr_load = load;
  curr_dim = dim;
  lower_split = lower_coords[dim];
  upper_split = upper_coords[dim];
  curr_lower_coords = lower_coords;
  curr_upper_coords = upper_coords;

  // Base case, split into 1 PE
  if (right_idx - left_idx <= 1){
    pe_split_coords[left] = vector<Vector3D<Real>>{lower_coords, upper_coords};
    pe_split_loads[left] = load;
}
  else // Recursive case
  {/*{{{*/
    curr_cb = new CkCallbackResumeThread();
    thisProxy.binaryLoadPartitionWithOctalBins(dim, load, left_idx, right_idx, lower_split, upper_split, lower_coords, upper_coords, *curr_cb);
    delete curr_cb;

    if(_lb_args.debug() >= debug_l0) CkPrintf("Split pt = %.8f\n", curr_split_pt);
    int mid_idx  = (left + right) / 2;
    Vector3D<Real> left_lower_coord = lower_coords;
    Vector3D<Real> left_upper_coord = upper_coords;
    left_upper_coord[dim] = curr_split_pt;
    Vector3D<Real> right_lower_coord = lower_coords;
    right_lower_coord[dim] = curr_split_pt;
    Vector3D<Real> right_upper_coord = upper_coords;
    float curr_right_load = load - curr_left_load;
    createPartitions(getDim(dim, left_lower_coord,  left_upper_coord),  curr_left_load, left, mid_idx, left_lower_coord, left_upper_coord);
    createPartitions(getDim(dim, right_lower_coord, right_upper_coord), curr_right_load, mid_idx, right, right_lower_coord, right_upper_coord);
  }/*}}}*/
}

bool inCurrentBox(Vector3D<Real> & centroid, Vector3D<Real> & lower_coords, Vector3D<Real> & upper_coords){
  bool ret = true;
  for (int i = 0; i < 3; i++){
    ret = ret
      && centroid[i] >= lower_coords[i]
      && centroid[i] < upper_coords[i];
  }

  return ret;
}

void DistributedOrbLB::binaryLoadPartitionWithOctalBins(int dim, float load, int left, int right, float low, float high, Vector3D<Real> lower_coords, Vector3D<Real> upper_coords, const CkCallback & curr_cb){
  if (my_pe == 0){/*{{{*/
    if(_lb_args.debug() >= debug_l1) ckout << lower_coords << "--" << upper_coords << "; " << low << " - " << high << endl;
  }
  octal_loads = vector<float>(8, .0f);
  octal_sizes = vector<int> (8, 0);

  float delta_coord = (high - low) / 8.0;
  for (auto & obj : obj_collection){
    if (inCurrentBox(obj.centroid, lower_coords, upper_coords)){
      int idx = (obj.centroid[dim] - low)/delta_coord;
      if (idx > 7) {
        //CkPrintf("*** idx %d lower %.8f upper %.8f my %.8f\n", idx, low, high, obj.centroid[dim]);
        idx = 7;
      }
      if (idx < 0) {
        //CkPrintf("*** idx %d lower %.8f upper %.8f my %.8f\n", idx, low, high, obj.centroid[dim]);
        idx = 0;
      }
      octal_loads[idx] += obj.load;
      octal_sizes[idx] += 1;

      if(_lb_args.debug() >= debug_l2) ckout << "\t\t++ Add to bin " << idx << "; " << obj.centroid << endl;
    }
    else{
      if(_lb_args.debug() >= debug_l2) ckout << "\t\tNot in the current box: " << obj.centroid << endl;
    }
  }

  if(_lb_args.debug() >= debug_l2) CkPrintf("\t\tPE[%d] octal_loads = %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f\n", my_pe,
      octal_loads[0], octal_loads[1], octal_loads[2], octal_loads[3],
      octal_loads[4], octal_loads[5], octal_loads[6], octal_loads[7]);
  if(_lb_args.debug() >= debug_l2) CkPrintf("\t\tPE[%d] octal_sizes = %d, %d, %d, %d, %d, %d, %d, %d\n", my_pe,
      octal_sizes[0], octal_sizes[1], octal_sizes[2], octal_sizes[3],
      octal_sizes[4], octal_sizes[5], octal_sizes[6], octal_sizes[7]);
/*}}}*/
  gatherOctalLoads();
}

void DistributedOrbLB::gatherOctalLoads(){
  int tupleSize = 16;/*{{{*/
  CkReduction::tupleElement tupleRedn[tupleSize];
  for (int i = 0; i < 8; i++){
    tupleRedn[i] = CkReduction::tupleElement(sizeof(float), &octal_loads[i], CkReduction::sum_float);
    tupleRedn[8+i] = CkReduction::tupleElement(sizeof(int), &octal_sizes[i], CkReduction::sum_int);
  }

  CkReductionMsg* msg = CkReductionMsg::buildFromTuple(tupleRedn, tupleSize);
  CkCallback cb(CkIndex_DistributedOrbLB::getSumOctalLoads(0), thisProxy[0]);
  msg->setCallback(cb);
  contribute(msg);/*}}}*/
}

void DistributedOrbLB::getSumOctalLoads(CkReductionMsg * msg){
  int numReductions;/*{{{*/
  CkReduction::tupleElement* results;
  float acc_load = .0f;
  int acc_size = 0;
  msg->toTuple(&results, &numReductions);
  for (int i = 0; i < 8; i++){
    global_octal_loads[i] = *(float*)results[i].data;
    acc_load += global_octal_loads[i];
    global_octal_sizes[i] = *(int*)results[8+i].data;
    acc_size += global_octal_sizes[i];
  }
  delete [] results;
  if(_lb_args.debug() >= debug_l0) CkPrintf("\tacc_load = %.8f; curr_load = %.8f; acc_size = %d;\n\tGlobal_octal_loads = %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f\n\tGlobal_octal_sizes = %d, %d, %d, %d, %d, %d, %d, %d\n", acc_load, curr_load, acc_size,
      global_octal_loads[0], global_octal_loads[1], global_octal_loads[2], global_octal_loads[3],
      global_octal_loads[4], global_octal_loads[5], global_octal_loads[6], global_octal_loads[7],
      global_octal_sizes[0], global_octal_sizes[1], global_octal_sizes[2], global_octal_sizes[3],
      global_octal_sizes[4], global_octal_sizes[5], global_octal_sizes[6], global_octal_sizes[7]);

  int curr_split_idx = 1;
  float half_load = acc_load / 2;

  // Find the split idx that cuts octal bin into two parts of even loads
  vector<float> prefix_octal_load(8, .0f);
  prefix_octal_load[0] = global_octal_loads[0] - half_load;
  for (int i = 1; i < 8; i++){
    prefix_octal_load[i] = global_octal_loads[i] + prefix_octal_load[i-1];
  }

  float curr_delta = prefix_octal_load[0];
  for (int i = 1; i < 7; i++){
    float abs_delta = std::fabs(prefix_octal_load[i]);
    if (abs_delta < std::fabs(curr_delta)){
      curr_delta = prefix_octal_load[i];
      curr_split_idx = i+1;
    }
  }

  if(_lb_args.debug() >= debug_l0) CkPrintf("\tcurr_split_idx = %d curr_delta = %.8f;\n\tprefix_octal_load = %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f\n", curr_split_idx, curr_delta,
      prefix_octal_load[0], prefix_octal_load[1], prefix_octal_load[2], prefix_octal_load[3],
      prefix_octal_load[4], prefix_octal_load[5], prefix_octal_load[6], prefix_octal_load[7]);

  float split_pt = ((upper_split - lower_split)/8) * (float)curr_split_idx;
  split_pt += lower_split;

  if(_lb_args.debug() >= debug_l1) CkPrintf("\tcurr_delta = %.8f, granularity = %.4f, curr_split_idx = %d, split_pt = %.8f\n", curr_delta, granularity, curr_split_idx, split_pt);

  // The split point is good enough
  if (std::fabs(curr_delta) <= granularity || (upper_split - lower_split) < 0.0000001){
    split_coords.push_back(split_pt);
    curr_split_pt = split_pt;
    curr_left_load = prefix_octal_load[curr_split_idx - 1] + half_load;
    lower_coords_col.push_back(curr_lower_coords);
    upper_coords_col.push_back(curr_upper_coords);
    thisProxy.finishedPartitionOneDim(*curr_cb);
  } else {
    // Keep partition the larger section to find better split point
    if (curr_delta < 0){
      // Futher partition the right part
      upper_split = split_pt + (upper_split - lower_split)/8;
      lower_split = split_pt;
    } else {
      // Futher patition the left part
      lower_split = split_pt - (upper_split - lower_split)/8;
      upper_split = split_pt;
    }

    if(_lb_args.debug() >= debug_l0) CkPrintf("\t****NEW dim = %d lower = %.8f upper = %.8f\n", curr_dim, lower_split, upper_split);
    thisProxy.binaryLoadPartitionWithOctalBins(curr_dim, curr_load, left_idx, right_idx, lower_split, upper_split, curr_lower_coords, curr_upper_coords, *curr_cb);
  }
  /*}}}*/
}


void DistributedOrbLB::finishedPartitionOneDim(const CkCallback & curr_cb){
  contribute(curr_cb);
}

void DistributedOrbLB::migrateObjects(std::vector<std::vector<Vector3D<Real>>> pe_splits){
  // Find the expected PE for each object{{{
  bool succ = false;
  for (auto & obj : obj_collection){
    for (int i = 0; i < pe_splits.size(); i++){
      if (inCurrentBox(obj.centroid, pe_splits[i][0], pe_splits[i][1])){
        obj.to_pe = i;
        succ = true;
        break;
      }
    }

    if (!succ) ckout << "!!!Failed obj "<< obj.centroid << " @ PE "<< my_pe << endl;
  }

  // Create migration message for each migration
  for (auto & obj : obj_collection){
    if (obj.to_pe != my_pe){
      total_migrates ++;
      MigrateInfo * move = new MigrateInfo;
      move->obj = obj.data_ptr->handle;
      move->from_pe = my_pe;
      move->to_pe = obj.to_pe;
      migrate_records.push_back(move);
      send_nobjs_to_pes[obj.to_pe] += 1;
    }
  }

  for (int i = 0; i < CkNumPes(); i++){
    thisProxy[i].acknowledgeIncomingMigrations(send_nobjs_to_pes[i]);
  }/*}}}*/
}

void DistributedOrbLB::acknowledgeIncomingMigrations(int count){
  recv_ack ++;
  incoming_migrates += count;
  if(recv_ack == CkNumPes()){
    // Make sure that final PE object count is not 0
    if (obj_collection.size() == total_migrates){
      total_migrates --;
      send_nobjs_to_pes[migrate_records[total_migrates]->to_pe] -= 1;
      delete migrate_records[total_migrates];
      migrate_records[total_migrates] = NULL;
    }
    final_migration_msg = new(total_migrates,CkNumPes(),CkNumPes(),0) LBMigrateMsg;
    final_migration_msg->n_moves = total_migrates;
    for (int i = 0; i < total_migrates; i++){
      final_migration_msg->moves[i] = *migrate_records[i];
      delete migrate_records[i];
      migrate_records[i] = NULL;
    }

    thisProxy[0].summary(total_migrates, n_partitions);

    for (int i = 0; i < CkNumPes(); i++){
      thisProxy[i].sendFinalMigrations(send_nobjs_to_pes[i]);
      if(_lb_args.debug() >= 2) CkPrintf("[%d] send %d objs to [%d]\n", CkMyPe(), send_nobjs_to_pes[i], i);
    }

  }
}

void DistributedOrbLB::sendFinalMigrations(int count){
  recv_final ++;
  incoming_final += count;
  if (recv_final == CkNumPes()){
    migrates_expected = incoming_final;
    if (_lb_args.debug() >= 1) CkPrintf("LB[%d] total move %d / %d  objects; incoming %d objs, final %d\n", my_pe, total_migrates, obj_collection.size(), incoming_final, obj_collection.size() - total_migrates + incoming_final);
    reset();
    ProcessMigrationDecision(final_migration_msg);
  }
}

void DistributedOrbLB::summary(int nmove, int total){
  recv_summary ++;
  total_move += nmove;
  total_objs += total;
  if (recv_summary == CkNumPes()){
    CkPrintf("DistributedOrbLB >> Summary:: moved %d / %d objects\n", total_move, total_objs);
  }
}

void DistributedOrbLB::reset(){
  universe_coords.clear();
  for (int i = 0; i < 3; i++){
    obj_coords[i].clear();
  }
  obj_coords.clear();
  global_octal_loads.clear();
  send_nobjs_to_pes.clear();
  migrate_records.clear();
  obj_collection.clear();
  octal_loads.clear();
  lower_coords_col.clear();
  upper_coords_col.clear();
  pe_split_coords.clear();
  pe_split_loads.clear();
}

#include "DistributedOrbLB.def.h"
