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
  start_time = CmiWallTimer();/*{{{*/
  if (CkMyPe() == 0) {
    CkPrintf("DistributedOrbLB>> Seq %d In DistributedOrbLB strategy at %lf; bin_size = $d;\n",lb_iter, start_time, bin_size);
  }
  lb_iter += 1;

  // Reset member variables for this LB iteration
  my_stats = stats;
  my_pe = CkMyPe();
  background_load = my_stats->bg_walltime * my_stats->pe_speed;
  //if (lb_iter == 1) background_load = .0f;

  initVariables();
  parseLBData();
  reportPerLBStates();/*}}}*/
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

  recv_spliters = 0;

  global_bin_loads = vector<float>(bin_size, .0f);
  global_bin_sizes = vector<int>(bin_size, 0);

  send_nobjs_to_pes = vector<int>(CkNumPes(), 0);
  send_nload_to_pes = vector<float>(CkNumPes(), .0f);
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
  post_lb_load = total_load;
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
  granularity = global_min_obj_load * 2;
  float avg_load = sum_load / (float)CkNumPes();
  float max_avg_ratio = max_load / avg_load;
  CkPrintf("DistributedOrbLB >> Pre LB load sum = %.4f; max / avg load ratio = %.4f\n", sum_load, max_avg_ratio);/*}}}*/
  CkPrintf("\t\t(%.8f, %.8f)\n", global_min_obj_load , global_max_obj_load);/*}}}*/

  if (max_avg_ratio < 1.15 && lb_iter > 1){
    CkPrintf("DistributedOrbLB >> Summary:: already balanced, skip\n");
    thisProxy.endLB();
  } else {
    thisProxy.reportUniverseDimensions();
  }
}

void DistributedOrbLB::endLB(){
  reset();
  migrates_expected = 0;
  final_migration_msg = new(0,CkNumPes(),CkNumPes(),0) LBMigrateMsg;
  final_migration_msg->n_moves = 0;
  ProcessMigrationDecision(final_migration_msg);
}

void DistributedOrbLB::reportPostLBStates(){
  int tupleSize = 4;/*{{{*/
  CkReduction::tupleElement tupleRedn[] = {
    CkReduction::tupleElement(sizeof(float), &post_lb_load, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &post_lb_load, CkReduction::sum_float),
    CkReduction::tupleElement(sizeof(int), &total_migrates, CkReduction::sum_int),
    CkReduction::tupleElement(sizeof(int), &n_partitions, CkReduction::sum_int),
};
  CkReductionMsg* msg = CkReductionMsg::buildFromTuple(tupleRedn, tupleSize);
  CkCallback cb(CkIndex_DistributedOrbLB::postLBStates(NULL), thisProxy[0]);
  msg->setCallback(cb);
  contribute(msg);/*}}}*/
}

void DistributedOrbLB::postLBStates(CkReductionMsg * msg){
  int numReductions;/*{{{*/
  CkReduction::tupleElement* results;

  msg->toTuple(&results, &numReductions);
  float max_load = *(float*)results[0].data;
  float sum_load = *(float*)results[1].data;
  int n_moves = *(int*)results[2].data;
  int n_total = *(int*)results[3].data;
  delete [] results;
  float avg_load = sum_load / (float)CkNumPes();
  CkPrintf("DistributedOrbLB >> Summary::\n\tPost LB load sum = %.4f; moved %d/%d objs; max / avg load ratio = %.4f\n", sum_load, n_moves, n_total,  max_load / avg_load);
  /*}}}*/
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

  int dim = getDim(-1, curr_lower_coords, curr_upper_coords);
  DorbPartitionRec rec{
    dim, global_load,
    0, CkNumPes(),
    curr_lower_coords[dim], curr_upper_coords[dim],
    curr_lower_coords, curr_upper_coords
  };
  createPartitions(rec);
}

void DistributedOrbLB::createPartitions(DorbPartitionRec rec){
  if(_lb_args.debug() >= debug_l0) CkPrintf("\n\n==== createPartitions dim %d load %.4f left %d right %d ====\n", rec.dim, rec.load, rec.left, rec.right);
  if(_lb_args.debug() >= debug_l0) ckout << rec.lower_coords << " - " << rec.upper_coords << endl;

  // Base case, split into 1 PE
  if (rec.right - rec.left <= 1){
    thisProxy[0].setPeSpliters(rec);
  }
  else // Recursive case
  {/*{{{*/
    bin_data_map[pair<int, int>(rec.left, rec.right)] = tuple<int, vector<float>, vector<int>>(0, vector<float>(bin_size, .0f), vector<int>(bin_size, 0));
    thisProxy.binaryLoadPartitionWithBins(rec);

    //if(_lb_args.debug() >= debug_l0) CkPrintf("Split pt = %.8f\n", curr_split_pt);
    //int mid_idx  = (left + right) / 2;
    //Vector3D<Real> left_lower_coord = lower_coords;
    //Vector3D<Real> left_upper_coord = upper_coords;
    //left_upper_coord[dim] = curr_split_pt;
    //Vector3D<Real> right_lower_coord = lower_coords;
    //right_lower_coord[dim] = curr_split_pt;
    //Vector3D<Real> right_upper_coord = upper_coords;
    //float curr_right_load = load - curr_left_load;
    //thisProxy[left].createPartitions(getDim(dim, left_lower_coord,  left_upper_coord),  curr_left_load, left, mid_idx, left_lower_coord, left_upper_coord);
    //thisProxy[mid_idx].createPartitions(getDim(dim, right_lower_coord, right_upper_coord), curr_right_load, mid_idx, right, right_lower_coord, right_upper_coord);
  }/*}}}*/
}

void DistributedOrbLB::setPeSpliters(DorbPartitionRec rec){
  recv_spliters ++;
  pe_split_coords[rec.left] = vector<Vector3D<Real>>{rec.lower_coords, rec.upper_coords};
  pe_split_loads[rec.left] = rec.load;

  if (_lb_args.debug() == debug_l0){
    CkPrintf("Splitter %d/%d\n", recv_spliters, CkNumPes());
  }
  if (recv_spliters == CkNumPes()){
    thisProxy.migrateObjects(pe_split_coords);
  }
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

void DistributedOrbLB::binaryLoadPartitionWithBins(DorbPartitionRec rec){
  if (my_pe == 0){/*{{{*/
    if(_lb_args.debug() >= debug_l1) ckout << rec.lower_coords << "--" << rec.upper_coords << "; " << rec.low << " - " << rec.high << endl;
  }
  octal_loads = vector<float>(bin_size, .0f);
  octal_sizes = vector<int> (bin_size, 0);

  double delta_coord = (rec.high - rec.low) / bin_size_double;
  for (auto & obj : obj_collection){
    if (inCurrentBox(obj.centroid, rec.lower_coords, rec.upper_coords)){
      int idx = ((double)obj.centroid[dim] - rec.low)/delta_coord;
      if (idx > (bin_size - 1)) {
        //CkPrintf("*** idx %d lower %.8f upper %.8f my %.8f\n", idx, low, high, obj.centroid[dim]);
        idx = bin_size - 1;
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
  //gatherBinLoads();
  thisProxy[rec.left].reportBinLoads(rec, octal_loads, octal_sizes);
}

void DistributedOrbLB::reportBinLoads(DorbPartitionRec rec, vector<float> bin_loads, vector<int> bin_sizes){
  auto & data = bin_data_map[pair<int,int>(rec.left, rec.right)];
  int old = get<0>(data);
  get<0>(data) += 1;
  for (int i  = 0; i < bin_size; i++){
    get<1>(data)[i] += bin_loads[i];
    get<2>(data)[i] += bin_sizes[i];
  }

  CkPrintf("Old %d -> %d\n", old, get<0>(data));

  if (get<0>(data) == CkNumPes()){
    CkPrintf("Collected all Data!!\n");
  }
}

void DistributedOrbLB::gatherBinLoads(){
  int tupleSize = bin_size * 2;/*{{{*/
  CkReduction::tupleElement tupleRedn[tupleSize];
  for (int i = 0; i < bin_size; i++){
    tupleRedn[i] = CkReduction::tupleElement(sizeof(float), &octal_loads[i], CkReduction::sum_float);
    tupleRedn[bin_size + i] = CkReduction::tupleElement(sizeof(int), &octal_sizes[i], CkReduction::sum_int);
  }

  CkReductionMsg* msg = CkReductionMsg::buildFromTuple(tupleRedn, tupleSize);
  CkCallback cb(CkIndex_DistributedOrbLB::getSumBinLoads(0), thisProxy[0]);
  msg->setCallback(cb);
  contribute(msg);/*}}}*/
}

void DistributedOrbLB::getSumBinLoads(CkReductionMsg * msg){
  //curr_depth ++;
  //int numReductions;/*{{{*/
  //CkReduction::tupleElement* results;
  //float acc_load = .0f;
  //int acc_size = 0;
  //int zero_size_count = 0;
  //msg->toTuple(&results, &numReductions);
  //for (int i = 0; i < bin_size; i++){
  //  global_bin_loads[i] = *(float*)results[i].data;
  //  acc_load += global_bin_loads[i];
  //  global_bin_sizes[i] = *(int*)results[bin_size + i].data;
  //  acc_size += global_bin_sizes[i];
  //  if (global_bin_sizes[i] == 0) zero_size_count++;
  //}
  //delete [] results;
  //if(_lb_args.debug() >= debug_l0) CkPrintf("\tacc_load = %.8f; curr_load = %.8f; acc_size = %d;\n", acc_load, curr_load, acc_size);

  //int curr_split_idx = 1;
  //int mid_idx = (left_idx + right_idx)/2;
  //float left_ratio = mid_idx - left_idx;
  //left_ratio /= (right_idx - left_idx);

  //float half_load = acc_load * left_ratio;

  //// Find the split idx that cuts octal bin into two parts of even loads
  //vector<float> prefix_bin_loads(bin_size, .0f);
  //prefix_bin_loads[0] = global_bin_loads[0] - half_load;
  //for (int i = 1; i < bin_size; i++){
  //  prefix_bin_loads[i] = global_bin_loads[i] + prefix_bin_loads[i-1];
  //}

  //if (_lb_args.debug() >= debug_l1) {
  //  // Print global_bin_sizes
  //  ckout << "\tglobal_bin_size::";
  //  for (int i = 0; i < bin_size; i++){
  //    if (i % 8 == 0){
  //      ckout << "\n\t";
  //    }
  //    ckout << global_bin_sizes[i] << " ";
  //  }

  //  ckout << "\n\tprefix_bin_loads";
  //  for (int i = 0; i < bin_size; i++){
  //    if (i % 8 == 0){
  //      ckout << "\n\t";
  //    }
  //    ckout << prefix_bin_loads[i] << " ";
  //  }
  //  ckout << endl;
  //}

  //float curr_delta = prefix_bin_loads[0];
  //for (int i = 0; i < bin_size; i++){
  //  if (prefix_bin_loads[i] > .0f) {
  //    curr_split_idx = i;
  //    break;
  //  }
  //}

  //int split_size = global_bin_sizes[curr_split_idx];

  //if (curr_split_idx > 0 && std::fabs(prefix_bin_loads[curr_split_idx - 1]) < std::fabs(prefix_bin_loads[curr_split_idx])){
  //  curr_delta = prefix_bin_loads[curr_split_idx - 1];
  //  split_size = global_bin_sizes[curr_split_idx - 1];
  //}else{
  //  curr_delta = prefix_bin_loads[curr_split_idx];
  //}

  //if(_lb_args.debug() >= debug_l0) CkPrintf("\tleft_ratio = %.8f curr_split_idx = %d curr_delta = %.8f split_size = %d; max prefix_bin_loads = %.8f\n", left_ratio, curr_split_idx, curr_delta, split_size, prefix_bin_loads[bin_size - 1]);

  //double split_pt = lower_split + ((upper_split - lower_split)/bin_size_double) * (double)curr_split_idx;

  //if(_lb_args.debug() >= debug_l1) CkPrintf("\tcurr_delta = %.8f, granularity = %.4f, curr_split_idx = %d, split_pt = %.8f\n", curr_delta, granularity, curr_split_idx, split_pt);

  //// The split point is good enough
  //if (std::fabs(curr_delta) <= granularity || (upper_split - lower_split) < 0.0000008 ){
  //  curr_split_pt = split_pt;
  //  if (curr_delta > .0f) curr_split_pt += (upper_split - lower_split)/bin_size_double;
  //  curr_left_load = prefix_bin_loads[curr_split_idx - 1] + half_load;
  //  prefix_bin_loads.clear();
  //  if (_lb_args.debug() >= debug_l0) CkPrintf("$$$ End partition curr_delta = %.8f, depth = %d, left_load_prefix = %.8f\n",curr_delta, curr_depth, prefix_bin_loads[curr_split_idx - 1]);
  //  thisProxy.finishedPartitionOneDim(*curr_cb);
  //} else {
  //  prefix_bin_loads.clear();
  //  // Keep partition the larger section to find better split point
  //  if (curr_split_idx == 0){
  //    // Futher partition the right part
  //    upper_split = lower_split + (upper_split - lower_split)/bin_size_double;
  //  } else if (curr_split_idx == bin_size - 1){
  //    lower_split = split_pt;
  //  } else {
  //    // Futher patition the left part
  //    upper_split = split_pt + (upper_split - lower_split)/bin_size_double;
  //    lower_split = split_pt;
  //  }

  //  if(_lb_args.debug() >= debug_l0) CkPrintf("\t****NEW dim = %d lower = %.12f upper = %.12f\n", curr_dim, lower_split, upper_split);
  //  thisProxy.binaryLoadPartitionWithBins(curr_dim, curr_load, left_idx, right_idx, lower_split, upper_split, curr_lower_coords, curr_upper_coords, *curr_cb);
  //}
  ///*}}}*/
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
      send_nload_to_pes[obj.to_pe] += obj.load;
      post_lb_load  -= obj.load;
    }
  }

  for (int i = 0; i < CkNumPes(); i++){
    thisProxy[i].acknowledgeIncomingMigrations(send_nobjs_to_pes[i], send_nload_to_pes[i]);
  }/*}}}*/
}

void DistributedOrbLB::acknowledgeIncomingMigrations(int count, float in_load){
  recv_ack ++;
  incoming_migrates += count;
  post_lb_load += in_load;
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

    reportPostLBStates();

    for (int i = 0; i < CkNumPes(); i++){
      thisProxy[i].sendFinalMigrations(send_nobjs_to_pes[i]);
      if(_lb_args.debug() >= 3) CkPrintf("[%d] send %d objs to [%d]\n", CkMyPe(), send_nobjs_to_pes[i], i);
    }
  }
}

void DistributedOrbLB::sendFinalMigrations(int count){
  recv_final ++;
  incoming_final += count;
  if (recv_final == CkNumPes()){
    migrates_expected = incoming_final;
    if (_lb_args.debug() >= 3) CkPrintf("LB[%d] total move %d / %d  objects; incoming %d objs, final %d\n", my_pe, total_migrates, obj_collection.size(), incoming_final, obj_collection.size() - total_migrates + incoming_final);
    reset();
    ProcessMigrationDecision(final_migration_msg);
  }
}

void DistributedOrbLB::reset(){
  universe_coords.clear();
  for (int i = 0; i < 3; i++){
    obj_coords[i].clear();
  }
  obj_coords.clear();
  global_bin_loads.clear();
  send_nobjs_to_pes.clear();
  migrate_records.clear();
  obj_collection.clear();
  octal_loads.clear();
  pe_split_coords.clear();
  pe_split_loads.clear();
  if (CkMyPe() == 0) {
    CkPrintf("DistributedOrbLB>> DistributedOrbLB strategy end at %lf\n", CmiWallTimer()-start_time);
  }
}

#include "DistributedOrbLB.def.h"
