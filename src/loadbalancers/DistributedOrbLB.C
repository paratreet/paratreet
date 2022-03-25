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
}

void DistributedOrbLB::InitLB(const CkLBOptions &opt) {
  thisProxy = CProxy_DistributedOrbLB(thisgroup);/*{{{*/
  if (opt.getSeqNo() > 0 || (_lb_args.metaLbOn() && _lb_args.metaLbModelDir() != nullptr))
    turnOff();
}

void DistributedOrbLB::Strategy(const DistBaseLB::LDStats* const stats) {
  start_time = CmiWallTimer();/*{{{*/
  if (CkMyPe() == 0) {
    // CkPrintf("*** LB strategy start at %.3lf ms\n", CmiWallTimer() * 1000);
    CkPrintf("*** CkNumPes = %d\n", CkNumPes());
    CkPrintf("DistributedOrbLB>> Seq %d In DistributedOrbLB strategy at %lf; bin_size = %d;\n",lb_iter, start_time, bin_size);
  }
  lb_iter += 1;

  // Reset member variables for this LB iteration
  my_stats = stats;
  my_pe = CkMyPe();
  background_load = my_stats->bg_walltime * my_stats->pe_speed;

  initVariables();
  parseLBData();
  reportPerLBStates();/*}}}*/
}


void DistributedOrbLB::initVariables(){
  if (my_pe == 0){
    CkPrintf("initVariables begin CkNumPes = %d\n", CkNumPes());
  }
  total_load = .0f;
  total_migrates = 0;
  recv_ack = 0;
  recv_final = 0;
  incoming_final = 0;
  n_partitions = 0;
  moved_partitions = 0;
  max_obj_load = .0f;
  min_obj_load = 10.0;
  use_longest_dim = true;
  obj_coords = vector<vector<float>>(3, vector<float>(1, .0f));
  bin_loads_collection = vector<vector<float>>(CkNumPes(), vector<float>(bin_size, .0f));
  bin_sizes_collection = vector<vector<int>>(CkNumPes(), vector<int>(bin_size, 0));
  fdata_collections.resize(CkNumPes());
  
  child_received = vector<int>(CkNumPes(), 0);
  universe_coords = vector<float>(6, .0f);

  recv_summary = 0;
  total_move = 0;
  total_objs = 0;
  moved_in_pe_count = 0;
  section_member_count = 0;
  // ready_to_recurse = 0;

  recv_spliters = 0;

  partition_flags = vector<bool> (CkNumPes(), false);
  global_bin_loads = vector<float>(bin_size, .0f);
  global_bin_sizes = vector<int>(bin_size, 0);

  send_nobjs_to_pes = vector<int>(CkNumPes(), 0);
  send_nload_to_pes = vector<float>(CkNumPes(), .0f);
  fdata_collections = vector<vector<LBShortCmp>>(CkNumPes(), vector<LBShortCmp>());

  if (my_pe == 0){
    CkPrintf("initVariables end CkNumPes = %d\n", CkNumPes());
  }
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
      n_partitions ++;
      // CkPrintf("LBDATA %d %.8f %.8f %.8f %.8f\n", my_pe, 
      //   usr_data.centroid[0],usr_data.centroid[1],usr_data.centroid[2], o_load);
      obj_collection.push_back(
          LBCentroidAndIndexRecord{
          usr_data.centroid, i,
          usr_data.particle_size, my_pe,
          o_load, .0f, 0,
          my_stats->objData[i]
          });

      obj_coords[0].push_back(usr_data.centroid[0]);
      obj_coords[1].push_back(usr_data.centroid[1]);
      obj_coords[2].push_back(usr_data.centroid[2]);

      if(_lb_args.debug() > 3) ckout << usr_data.centroid <<endl;

      if(_lb_args.debug() > 2) ckout << "PT[" << usr_data.chare_idx << "] @ PE[" << my_pe << "] size = "<< usr_data.particle_size <<"; load " << o_load <<"; centroid: " << usr_data.centroid << endl;
    } else {
      background_load += o_load;
    }
    #endif
  }
  if(_lb_args.debug() == 3) CkPrintf("PE[%d] partition size = %d \n", my_pe, n_partitions);

  if (lb_iter == 1) background_load = .0f;
  bgload_per_obj = background_load / n_partitions;

  for (auto & obj : obj_collection){
    obj.load += bgload_per_obj;
    total_load += obj.load;
    if (obj.load > max_obj_load) max_obj_load = obj.load;
    if (obj.load < min_obj_load) min_obj_load = obj.load;
  }
  post_lb_load = total_load;
}

int DistributedOrbLB::getDim(int dim, Vector3D<Real>& lower_coords, Vector3D<Real>& upper_coords){
  if (!use_longest_dim){
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
  return -1;
}

void DistributedOrbLB::reportPerLBStates(){
  int tupleSize = 11;/*{{{*/

  float min_xdim = *std::min_element(obj_coords[0].begin(), obj_coords[0].end());
  float max_xdim = *std::max_element(obj_coords[0].begin(),  obj_coords[0].end());
  float min_ydim = *std::min_element(obj_coords[1].begin(),  obj_coords[1].end());
  float max_ydim = *std::max_element(obj_coords[1].begin(),  obj_coords[1].end());
  float min_zdim = *std::min_element(obj_coords[2].begin(),  obj_coords[2].end());
  float max_zdim = *std::max_element(obj_coords[2].begin(),  obj_coords[2].end());

  CkReduction::tupleElement tupleRedn[] = {
    CkReduction::tupleElement(sizeof(float), &total_load, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &total_load, CkReduction::sum_float),
    CkReduction::tupleElement(sizeof(float), &max_obj_load, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &min_obj_load, CkReduction::min_float),
    CkReduction::tupleElement(sizeof(int), &n_partitions, CkReduction::sum_int),
    CkReduction::tupleElement(sizeof(float), &min_xdim, CkReduction::min_float),
    CkReduction::tupleElement(sizeof(float), &max_xdim, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &min_ydim, CkReduction::min_float),
    CkReduction::tupleElement(sizeof(float), &max_ydim, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &min_zdim, CkReduction::min_float),
    CkReduction::tupleElement(sizeof(float), &max_zdim, CkReduction::max_float),
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
  total_objs = *(int*)results[4].data;
  global_load = sum_load;
  granularity = global_min_obj_load;
  float avg_load = sum_load / (float)CkNumPes();
  float max_avg_ratio = max_load / avg_load;
  CkPrintf("DistributedOrbLB >> Pre LB load sum = %.4f; max / avg load ratio = %.4f\n", sum_load, max_avg_ratio);/*}}}*/
  CkPrintf("DistributedOrbLB >> CkNumPes %d\n", CkNumPes());/*}}}*/
  CkPrintf("\t\t(%.8f, %.8f)\n", global_min_obj_load , global_max_obj_load);/*}}}*/

  if (max_avg_ratio < 1){
    CkPrintf("DistributedOrbLB >> Summary:: already balanced, skip\n");
    thisProxy.endLB();
  } else {
    if(_lb_args.debug() >= debug_l1) ckout << "Universe coord :: ";
    float pad = .1e-5;
    for (int i = 0; i < 6; i++){
      universe_coords[i] = *(float*)results[i+5].data;
      if (i % 2) universe_coords[i] += pad;
      else universe_coords[i] -= pad;
      if(_lb_args.debug() >= debug_l1) ckout << universe_coords[i] << "; ";
    }
    if(_lb_args.debug() >= debug_l1) ckout << endl;
    findPeSplitPoints();
  }

  delete [] results;
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
    CkReduction::tupleElement(sizeof(int), &total_migrates, CkReduction::sum_int),
    CkReduction::tupleElement(sizeof(int), &n_partitions, CkReduction::sum_int),
    CkReduction::tupleElement(sizeof(float), &new_load, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &new_load, CkReduction::sum_float),
  };
  CkReductionMsg* msg = CkReductionMsg::buildFromTuple(tupleRedn, tupleSize);
  CkCallback cb(CkIndex_DistributedOrbLB::postLBStates(NULL), thisProxy[0]);
  msg->setCallback(cb);
  contribute(msg);/*}}}*/
  if (CkMyPe() == 0) CkPrintf("*** Distributed_OrbLB strategy end at %.3lf ms\n", CmiWallTimer() * 1000);

  CkWaitQD();
  reset();
  MigrationDone(1);
}

void DistributedOrbLB::postLBStates(CkReductionMsg * msg){
  int numReductions;/*{{{*/
  CkReduction::tupleElement* results;

  msg->toTuple(&results, &numReductions);
  int n_moves = *(int*)results[0].data;
  int n_total = *(int*)results[1].data;
  float max_new_load = *(float*)results[2].data;
  float new_global_load = *(float*)results[3].data;
  delete [] results;
  CkPrintf("DistributedOrbLB >> Summary::  Post LB moved %d/%d objs; max/avg ratio = %.4f; new load = %.4f \n", n_moves, n_total, max_new_load / (global_load / CkNumPes()), new_global_load/global_load);
  /*}}}*/
}

void DistributedOrbLB::bCastSectionGatherObjects(DorbPartitionRec rec){
  if (rec.right - rec.left == CkNumPes()){
    thisProxy.mergeObjects(rec);
    return;
  }

  if (rec.right - rec.left <= 1){
    section_obj_collection.insert(section_obj_collection.end(), obj_collection.begin(), obj_collection.end());
    thisProxy[rec.left].bruteForceSolver(rec);
  }

  int my_idx = my_pe - rec.left;
  int left_child = my_idx * 2 + rec.left;
  int right_child = left_child + 1;
  if (left_child == rec.left){
    thisProxy[left_child].mergeObjects(rec);
  } else if (left_child < rec.right){
    thisProxy[left_child].bCastSectionGatherObjects(rec);
    thisProxy[left_child].mergeObjects(rec);
  }

  if (right_child < rec.right){
    thisProxy[right_child].bCastSectionGatherObjects(rec);
    thisProxy[right_child].mergeObjects(rec);
  }
}

void DistributedOrbLB::bruteForceSolverRecursiveHelper(DorbPartitionRec rec, int start_idx, int end_idx){
  // bcast group to contribute their objects
  float acc_load = .0f;
  for (int i = start_idx; i < end_idx; i++){
    acc_load += section_dorb_collection[i].load;
  }
  // CkPrintf("rec (%d , %d) bruteForceSolverRecursiveHelper start %d end %d size %d n_obj %d load %.6f acc_load %.6f\n", rec.left, rec.right, start_idx, end_idx, end_idx - start_idx, rec.n_objs, rec.load, acc_load);
  rec.load = acc_load;

  if (rec.right - rec.left <= 1){
    // CkPrintf("rec (%d , %d) bruteForceSolverRecursiveHelper final my_pe = %d start %d end %d size %d load %.6f\n", rec.left, rec.right, my_pe, start_idx, end_idx, end_idx - start_idx, rec.load);
    if (end_idx - start_idx < 1){
      CkPrintf("ERROR! %d tokens assigned to pe %d\n", end_idx - start_idx, rec.left);
    }
    for (int i = start_idx; i < end_idx; i++){
      section_obj_collection[section_dorb_collection[i].original_idx].to_pe = rec.left;
      section_updated_objects[section_dorb_collection[i].from_pe].push_back(section_obj_collection[section_dorb_collection[i].original_idx]);
      if (section_dorb_collection[i].from_pe != rec.left){
        section_updated_migrates[rec.left - my_pe] += 1;
      }
    }

    section_updated_load[rec.left - my_pe] = acc_load;
    return;
  }

  std::sort(section_dorb_collection.begin()+start_idx, section_dorb_collection.begin()+end_idx, NdComparator(rec.dim));
  
  int mid_pe = (rec.left + rec.right) / 2;
  acc_load  = .0f;
  float acc_load_more;
  float target_left_load = mid_pe - rec.left;
  target_left_load /= (rec.right - rec.left);
  target_left_load *= rec.load;

  int left_size = 0;
  for (; (start_idx + left_size) < end_idx; left_size++){
    if (acc_load + section_dorb_collection[start_idx + left_size].load > target_left_load){
      // CkPrintf("acc_load %.6f left_size %d next_idx %d\n", acc_load, left_size, start_idx + left_size);
      acc_load_more = acc_load + section_dorb_collection[start_idx + left_size].load;
      break;
    }
    acc_load += section_dorb_collection[start_idx + left_size].load;
  }

  if (std::abs(target_left_load - acc_load_more) < std::abs(target_left_load - acc_load)){
    acc_load = acc_load_more;
    left_size ++;
    // CkPrintf("acc_load %.6f left_size %d next_idx %d\n", acc_load, left_size, start_idx + left_size);
  }
  if (start_idx + left_size + 1 >= section_dorb_collection.size()){
    CkPrintf("rec (%d , %d) my_pe = %d, section_dorb_collection size = %d, split_point idx = %d left_size %d start_idx %d end_idx %d\n",rec.left, rec.right, my_pe, section_dorb_collection.size(), start_idx + left_size + 1, left_size, start_idx, end_idx);
    CkPrintf("acc_load %.8f, acc_load_more %.8f, target_left_load %.8f, rec.load %.8f\n", acc_load, acc_load_more, target_left_load, rec.load);
 
    // CkPrintf("rec (%d , %d) my_pe = %d, section_obj_collection size = %d, split_point idx = %d left_size %d start_idx %d end_idx %d\n",rec.left, rec.right, my_pe, section_obj_collection.size(), left_size, start_idx + left_size + 1, start_idx, end_idx);
  }

  float split_point = section_dorb_collection[start_idx + left_size + 1].centroid[rec.dim];

  Vector3D<Real> left_lower_coord = rec.lower_coords;
  Vector3D<Real> left_upper_coord = rec.upper_coords;
  left_upper_coord[rec.dim] = split_point;
  Vector3D<Real> right_lower_coord = rec.lower_coords;
  right_lower_coord[rec.dim] = split_point;
  Vector3D<Real> right_upper_coord = rec.upper_coords;
  float curr_right_load = rec.load - acc_load;

  int left_dim = getDim(rec.dim, left_lower_coord, left_upper_coord);
  DorbPartitionRec left_rec{
    left_dim, acc_load, left_size,
    rec.left, mid_pe,
    left_lower_coord[left_dim], left_upper_coord[left_dim],
    rec.granularity,
    left_lower_coord, left_upper_coord};

  int right_dim = getDim(rec.dim, right_lower_coord, right_upper_coord);
  DorbPartitionRec right_rec{
    right_dim, rec.load - acc_load, rec.n_objs - left_size,
    mid_pe, rec.right,
    right_lower_coord[right_dim], right_upper_coord[right_dim],
    rec.granularity,
    right_lower_coord, right_upper_coord};

  bruteForceSolverRecursiveHelper(left_rec, start_idx, start_idx + left_size);
  bruteForceSolverRecursiveHelper(right_rec, start_idx + left_size, end_idx);
}

void DistributedOrbLB::bruteForceSolver(DorbPartitionRec rec){
  CkPrintf("rec (%d, %d) bruteForceSolver\n", rec.left, rec.right);
  // bcast group to contribute their objects
  section_updated_objects.resize(CkNumPes());
  section_updated_load.resize(rec.right - rec.left);
  section_updated_migrates = vector<int>(rec.right - rec.left, 0);
  float acc_load = .0f;
  for (int i = 0; i < section_obj_collection.size(); i++){
    acc_load += section_obj_collection[i].load;
    section_dorb_collection.push_back(
      LBDorbShortToken{
      section_obj_collection[i].centroid, 
      section_obj_collection[i].from_pe, 
      section_obj_collection[i].to_pe,i,
      section_obj_collection[i].load
      }
    );
  }
  rec.load = acc_load;
  rec.n_objs = section_obj_collection.size();

  bruteForceSolverRecursiveHelper(rec, 0, section_obj_collection.size());

  // CkPrintf("***rec(%d, %d) bruteForceSolver completed\n", rec.left, rec.right);

  // merge decisions
  for (int i = rec.left; i < rec.right; i++){
    // CkPrintf("Send final loads:: %d -> %d\n", my_pe, i);
    thisProxy[i].ackBruteForceResults(section_updated_load[i - my_pe], section_updated_migrates[i - my_pe]);
  }
  for (int i = 0; i < CkNumPes(); i++){
    // CkPrintf("Send final tokens:: %d -> %d size %d\n", my_pe, i, section_updated_objects[i].size());
    if (section_updated_objects[i].size() > 0){
      thisProxy[i].sendUpdatedTokens(section_updated_objects[i]);
    }
  }
}

void DistributedOrbLB::sendUpdatedTokens(vector<LBCentroidAndIndexRecord> tokens){
  for (auto & obj : tokens){
    if (obj.from_pe != my_pe){
      CkPrintf("ERROR: [%d] but %d\n", my_pe, obj.from_pe);
    }
    else if (obj.to_pe != my_pe){
      // total_migrates ++;
      // CkPrintf("Move %d -> %d\n", obj.from_pe, obj.to_pe);
      lbmgr->Migrate(obj.obj_data.handle,obj.to_pe);
    }
  }
}

void DistributedOrbLB::ackBruteForceResults(float load, int n_migrates){
  new_load = load;
  moved_partitions = n_partitions;
  total_migrates = n_migrates;

  reportPostLBStates();
}

void DistributedOrbLB::mergeObjects(DorbPartitionRec rec){
  addObjects(rec, obj_collection);
}

void DistributedOrbLB::addObjects(DorbPartitionRec rec, vector<LBCentroidAndIndexRecord> objs){
  child_received[rec.left] += 1;
  section_obj_collection.insert(section_obj_collection.end(), objs.begin(), objs.end());

  if (my_pe == rec.left && child_received[rec.left] == 2){
    child_received[rec.left] = 0;
    thisProxy[rec.left].bruteForceSolver(rec);
    return;
  }
  
  int my_spinning_tree_idx = calSpinningTreeIdx(rec.left);
  int my_spinning_tree_parent = my_spinning_tree_idx / 2 + rec.left;

  if (child_received[rec.left] == calNChildren(my_pe, rec)){
    child_received[rec.left] = 0;
    thisProxy[my_spinning_tree_parent].addObjects(rec, section_obj_collection);
  }
}

void DistributedOrbLB::findPeSplitPoints(){
  CkPrintf("findPeSplitPoints cknumpe %d\n", CkNumPes());
  curr_lower_coords = Vector3D<Real>(universe_coords[0], universe_coords[2], universe_coords[4]);
  curr_upper_coords = Vector3D<Real>(universe_coords[1], universe_coords[3], universe_coords[5]);

  int dim = getDim(-1, curr_lower_coords, curr_upper_coords);
  DorbPartitionRec rec = DorbPartitionRec{
    dim, global_load, total_objs,
    0, CkNumPes(),
    curr_lower_coords[dim], curr_upper_coords[dim], granularity,
    curr_lower_coords, curr_upper_coords
  };
  thisProxy[0].createPartitions(rec);
}

void DistributedOrbLB::createPartitions(DorbPartitionRec rec){
  // CkPrintf("rec (%d, %d) partition_flags %s CkNumPes %d\n", rec.left, rec.right, partition_flags[rec.right]? "true" : "false", CkNumPes());
  if (partition_flags[rec.right]) return;
  partition_flags[rec.right] = true;
  if (rec.left == 0) CkPrintf("*** rec(%d,  %d) createPartitions at %.3lf ms\n", rec.left, rec.right, CmiWallTimer() * 1000);
  if(_lb_args.debug() >= debug_l0) CkPrintf("\n\n==== createPartitions dim %d load %.4f left %d right %d size %d ====\n", rec.dim, rec.load, rec.left, rec.right, rec.n_objs);/*{{{*/
  if(_lb_args.debug() >= debug_l1) ckout << rec.lower_coords << " - " << rec.upper_coords << endl;

  // Base case, split into 1 PE
  if (rec.right - rec.left <= 1){
    // CkPrintf("\n**** rec(%d, %d) Done partition dim %d load %.4f size %d  ****\n", rec.left, rec.right, rec.dim, rec.load, rec.n_objs);
    thisProxy[rec.left].bCastSectionGatherObjects(rec);
  } else if (rec.n_objs <= brute_force_size){
    thisProxy[rec.left].bCastSectionGatherObjects(rec);
  }
  else // Recursive case
  {
    thisProxy[rec.left].bCastSectionLoadBinning(rec);
  }
}

void DistributedOrbLB::bCastSectionLoadBinning(DorbPartitionRec rec){
  if (rec.right - rec.left == CkNumPes()){
    thisProxy.loadBinning(rec);
    return;
  }

  int my_idx = my_pe - rec.left;
  int left_child = my_idx * 2 + rec.left;
  int right_child = left_child + 1;
  if (left_child == rec.left){
    thisProxy[left_child].loadBinning(rec);
  } else if (left_child < rec.right){
    thisProxy[left_child].bCastSectionLoadBinning(rec);
    thisProxy[left_child].loadBinning(rec);
  }

  if (right_child < rec.right){
    thisProxy[right_child].bCastSectionLoadBinning(rec);
    thisProxy[right_child].loadBinning(rec);
  }
}

bool DistributedOrbLB::inCurrentBox(Vector3D<Real> & centroid, Vector3D<Real> & lower_coords, Vector3D<Real> & upper_coords){
  bool ret = true;
  for (int i = 0; i < 3; i++){
    ret = ret
      && centroid[i] > lower_coords[i]
      && centroid[i] <= upper_coords[i];
  }

  return ret;
}

int DistributedOrbLB::calNChildren(int pe, DorbPartitionRec & rec){
  if (pe == rec.left) return 2; // The root
  int my_idx = pe - rec.left;
  int max_idx = rec.right - rec.left -1;

  if (my_idx > max_idx / 2) return 1; // The leaf
  if (my_idx < max_idx / 2) return 3; // The subroot expect the largest
  if (max_idx % 2) return 3;
  return 2;
}

int DistributedOrbLB::calSpinningTreeIdx(int root){
  return my_pe - root;
}

void DistributedOrbLB::loadBinning(DorbPartitionRec rec){
  // CkPrintf("[%d] rec (%d , %d) loadBinning size %d\n", my_pe, rec.left, rec.right, obj_collection.size());

  if (my_pe == rec.left){/*{{{*/
    if(_lb_args.debug() >= debug_l1) ckout << rec.lower_coords << "--" << rec.upper_coords << "; " << rec.low << " - " << rec.high << endl;
  }

  vector<float> load_bins = vector<float>(bin_size, .0f);
  vector<int>   size_bins = vector<int> (bin_size, 0);

  double delta_coord = (rec.high - rec.low) / bin_size_double;
  for (auto & obj : obj_collection){
    if (inCurrentBox(obj.centroid, rec.lower_coords, rec.upper_coords)){
      int idx = ((double)obj.centroid[rec.dim] - rec.low)/delta_coord;
      if (idx > (bin_size - 1)) {
        // CkPrintf("*** case 1 idx %d lower %.8f upper %.8f my %.8f\n", idx, low, high, obj.centroid[rec.dim]);
        idx = bin_size - 1;
      }
      if (idx < 0) {
        // CkPrintf("*** case 2 idx %d lower %.8f upper %.8f my %.8f\n", idx, rec.low, rec.high, obj.centroid[rec.dim]);
        idx = 0;
      }
      load_bins[idx] += obj.load;
      size_bins[idx] += 1;

      if(_lb_args.debug() >= debug_l2) ckout << "\t\t++ Add to bin " << idx << "; " << obj.centroid << endl;
    } else {
      CkPrintf("*** case 3 rec(%d, %d) lower %.8f upper %.8f my %.8f\n", rec.left, rec.right, rec.low, rec.high, obj.centroid[rec.dim]);
    }
  }

  // CkPrintf("[%d] rec left %d tree_idx %d nchild %d \n", my_pe, rec.left, calSpinningTreeIdx(rec.left), calNChildren(my_pe, rec));
  thisProxy[my_pe].mergeBinLoads(rec, load_bins, size_bins);
}

void DistributedOrbLB::mergeBinLoads(DorbPartitionRec rec, vector<float> bin_loads, vector<int> bin_sizes){
  child_received[rec.left] += 1;
  int acc_size = 0;
  for (int i = 0; i < bin_size; i++){
    bin_loads_collection[rec.left][i] += bin_loads[i];
    bin_sizes_collection[rec.left][i] += bin_sizes[i];
    acc_size += bin_sizes[i];
  }
  
  // CkPrintf("%d rec (%d, %d)  mergeBinLoads size  = %d nchild = %d\n",my_pe,  rec.left, rec.right, acc_size, calNChildren(my_pe, rec));


  // When I am the root of the spinning tree
  if (my_pe == rec.left && child_received[rec.left] == 2){
    // CkPrintf("[Target %d] mergeBinLoads to root\n", rec.left);
    vector<float> final_bin_loads = bin_loads_collection[rec.left];
    vector<int>   final_bin_sizes = bin_sizes_collection[rec.left];
    bin_loads_collection[rec.left] = vector<float>(bin_size, .0f);
    bin_sizes_collection[rec.left] = vector<int>(bin_size, 0);
    child_received[rec.left] = 0;
    processBinLoads(rec, final_bin_loads, final_bin_sizes);
    return;
  }

  // When I am a subroot and collected all my child
  if (child_received[rec.left] == calNChildren(my_pe, rec)){
    int my_spinning_tree_idx = calSpinningTreeIdx(rec.left);
    int my_spinning_tree_parent = my_spinning_tree_idx / 2 + rec.left;
    child_received[rec.left] = 0;

    vector<float> bin_loads = bin_loads_collection[rec.left];
    vector<int>   bin_sizes = bin_sizes_collection[rec.left];
    bin_loads_collection[rec.left] = vector<float>(bin_size, .0f);
    bin_sizes_collection[rec.left] = vector<int>(bin_size, 0);

    thisProxy[my_spinning_tree_parent].mergeBinLoads(rec, bin_loads, bin_sizes);
    // if (_lb_args.debug() == debug_l0) 
    // CkPrintf("[Target %d] mergeBinLoads %d to %d (%d)\n", rec.left, my_pe, my_spinning_tree_parent, my_spinning_tree_idx);
  }
}

void DistributedOrbLB::processBinLoads(DorbPartitionRec rec, vector<float> bin_loads, vector<int> bin_sizes){
  vector<float> col_loads = bin_loads;
  vector<int> col_sizes = bin_sizes;
  int total_size = 0;

  // Print global_bin_loads and sizes
  if (_lb_args.debug() >= debug_l1) {
    ckout << "\trec (" << rec.left << ", " << rec.right << ") bin_size::";
    for (int i = 0; i < bin_size; i++){
      if (i % 8 == 0){
        ckout << "\n\t";
      }
      total_size += col_sizes[i];
      ckout << col_sizes[i] << " ";
    }
    ckout << "\ntotal size = "<<total_size;

    ckout << "\n\tbin_loads";
    for (int i = 0; i < bin_size; i++){
      if (i % 8 == 0){
        ckout << "\n\t";
      }
      ckout << col_loads[i] << " ";
    }
    ckout << endl;
  }

  int acc_size = 0;
  float acc_load = .0f;
  for (int i = 0; i < bin_size; i++){
    acc_load += col_loads[i];
    acc_size += col_sizes[i];
  }

  // CkPrintf("ROOT rec(%d, %d) acc_size %d expected_size %d acc_load ratio = %.4f \n", rec.left, rec.right, acc_size, rec.n_objs, acc_load / rec.load);
  rec.load = acc_load;
  rec.n_objs = acc_size;

  int curr_split_idx = 1;
  curr_left_nobjs = 0;
  curr_right_nobjs = 0;
  int mid_idx = (rec.left + rec.right)/2;
  float left_ratio = mid_idx - rec.left;
  left_ratio /= (rec.right - rec.left);

  float half_load = acc_load * left_ratio;

  // Find the split idx that cuts octal bin into two parts of even loads
  vector<float> prefix_bin_loads(bin_size, .0f);
  prefix_bin_loads[0] = col_loads[0] - half_load;
  for (int i = 1; i < bin_size; i++){
    prefix_bin_loads[i] = col_loads[i] + prefix_bin_loads[i-1];
  }
  
  // Print prefix bin loads
  if (_lb_args.debug() >= debug_l1) {
    // Print global_bin_sizes
    ckout << "\trec (" << rec.left << ", " << rec.right << ") prefix_bin_loads";
    for (int i = 0; i < bin_size; i++){
      if (i % 8 == 0){
        ckout << "\n\t";
      }
      ckout << prefix_bin_loads[i] << " ";
    }
    ckout << endl;
  }

  float curr_delta = prefix_bin_loads[0];
  float left_load = - half_load;
  for (int i = 0; i < bin_size; i++){
    if (prefix_bin_loads[i] > .0f) {
      curr_split_idx = i;
      break;
    }else{
      curr_left_nobjs += col_sizes[i];
    }
  }

  if (curr_split_idx > 0) left_load = prefix_bin_loads[curr_split_idx - 1];

  int split_size = col_sizes[curr_split_idx];

  if (curr_split_idx > 0 && std::fabs(prefix_bin_loads[curr_split_idx - 1]) < std::fabs(prefix_bin_loads[curr_split_idx])){
    curr_delta = prefix_bin_loads[curr_split_idx - 1];
  }else{
    curr_delta = prefix_bin_loads[curr_split_idx];
  }

  if(_lb_args.debug() >= debug_l0) CkPrintf("\trec (%d, %d) left_ratio = %.8f curr_split_idx = %d curr_delta = %.8f split_size = %d left_nobjs = %d ; split_load = %.4f largest_prefix_bin_loads = %.8f\n", rec.left, rec.right, left_ratio, curr_split_idx, curr_delta, split_size, curr_left_nobjs, col_loads[curr_split_idx], prefix_bin_loads[bin_size - 1]);

  double split_pt = rec.low + ((rec.high - rec.low)/bin_size_double) * (double)curr_split_idx;




  // The split point is good enough
  if (std::fabs(curr_delta) <= rec.granularity )
  {
    // if (curr_delta > .0f) split_pt += (rec.high - rec.low)/bin_size_double;
    if (_lb_args.debug() >= debug_l0) CkPrintf("$$$ End partition curr_delta = %.8f, left_load_prefix = %.8f\n",curr_delta, curr_delta + half_load);
    finishedPartitionOneDim(rec, split_pt, curr_delta + half_load, curr_left_nobjs);
  }
  else if (split_size < 100 || (rec.high - rec.low) < 0.0000008)
  {
    curr_left_load = left_load;
    curr_half_load = half_load;
    curr_split_pt = split_pt;
    if(_lb_args.debug() >= debug_l0) CkPrintf("Need Final step for rec (%d, %d) curr_split_idx = %d, curr_bin_load %.8f left_load = %.8f acc_load = %.8f half_load = %.8f\n", rec.left, rec.right, curr_split_idx, col_loads[curr_split_idx], left_load, acc_load, half_load);
    thisProxy[rec.left].bCastSectionFinalPartitionStep(rec, curr_split_idx);
  }
  else
  {
    prefix_bin_loads.clear();
    DorbPartitionRec new_rec = rec;
    if (curr_split_idx == 0){
      new_rec.high = rec.low + (rec.high - rec.low)/bin_size_double;
    } else if (curr_split_idx == bin_size - 1){
      new_rec.low = split_pt;
    } else {
      new_rec.high = split_pt + (new_rec.high - new_rec.low)/bin_size_double;
      new_rec.low = split_pt;
    }

    // if(_lb_args.debug() >= debug_l0) 
    //CkPrintf("\t****NEW dim = %d lower = %.12f upper = %.12f\n", new_rec.dim, new_rec.low, new_rec.high);
    thisProxy[new_rec.left].bCastSectionLoadBinning(new_rec);
  }
}

void DistributedOrbLB::bCastSectionFinalPartitionStep(DorbPartitionRec rec, int curr_split_idx){
  if (rec.right - rec.left == CkNumPes()){
    thisProxy.finalPartitionStep(rec, curr_split_idx);
    return;
  }

  int my_idx = my_pe - rec.left;
  int left_child = my_idx * 2 + rec.left;
  int right_child = left_child + 1;
  if (left_child == rec.left){
    thisProxy[left_child].finalPartitionStep(rec, curr_split_idx);
  } else if (left_child < rec.right){
    thisProxy[left_child].bCastSectionFinalPartitionStep(rec, curr_split_idx);
    thisProxy[left_child].finalPartitionStep(rec, curr_split_idx);
  }

  if (right_child < rec.right){
    thisProxy[right_child].bCastSectionFinalPartitionStep(rec, curr_split_idx);
    thisProxy[right_child].finalPartitionStep(rec, curr_split_idx);
  }
}

void DistributedOrbLB::finalPartitionStep(DorbPartitionRec rec, int bin_idx){
  // CkPrintf("rec (%d, %d) finalPartitionStep %d\n",rec.left, rec.right, my_pe);
  if (rec.left > my_pe || rec.right <= my_pe) return;
  if (my_pe == 0){/*{{{*/
    if(_lb_args.debug() >= debug_l1) ckout <<"Final Step:: rec("<< rec.left << "," << rec.right << ") "<< rec.lower_coords << "--" << rec.upper_coords << "; " << rec.low << " - " << rec.high << endl;
  }
  vector<LBShortCmp> fdata;

  double delta_coord = (rec.high - rec.low) / bin_size_double;
  for (auto & obj : obj_collection){
    if (inCurrentBox(obj.centroid, rec.lower_coords, rec.upper_coords)){
      int idx = ((double)obj.centroid[rec.dim] - rec.low) / delta_coord;
      if (idx == bin_idx){
        fdata.push_back(LBShortCmp{obj.centroid[rec.dim], obj.load});
        // if(_lb_args.debug() >= debug_l2) ckout << "\t\t++ Add to final " << "; " << obj.centroid << endl;
      }
    }
  }
  fdata_collections[rec.left].clear();
  thisProxy[my_pe].mergeFinalStepData(rec, fdata);
}

void DistributedOrbLB::mergeFinalStepData(DorbPartitionRec rec, vector<LBShortCmp> data){
  // if (_lb_args.debug() >= debug_l0) 
  int my_spinning_tree_idx = calSpinningTreeIdx(rec.left);
  int my_spinning_tree_parent = my_spinning_tree_idx / 2 + rec.left;
  // CkPrintf("rec (%d, %d) mergeFinalStepData %d to %d (%d)\n",rec.left, rec.right, my_pe, my_spinning_tree_parent, my_spinning_tree_idx);
  child_received[rec.left] += 1;
  fdata_collections[rec.left].insert(fdata_collections[rec.left].end(), data.begin(), data.end());

  if (my_pe == rec.left && child_received[rec.left] == 2){
    // if (_lb_args.debug() >= debug_l0) 
    // CkPrintf("[Target %d] mergeFinalStepData to root\n", rec.left);
    child_received[rec.left] = 0;
    processFinalStepData(rec);
    return;
  }

  if (child_received[rec.left] == calNChildren(my_pe, rec)){
    child_received[rec.left] = 0;
    thisProxy[my_spinning_tree_parent].mergeFinalStepData(rec, fdata_collections[rec.left]);
    // if (_lb_args.debug() >= debug_l0) 
    // CkPrintf("[Target %d] mergeFinalStepData %d to %d (%d)\n",rec.left, my_pe, my_spinning_tree_parent, my_spinning_tree_idx);
  }
}

void DistributedOrbLB::processFinalStepData(DorbPartitionRec rec){
  vector<LBShortCmp> final_data = fdata_collections[rec.left];
  fdata_collections[rec.left].clear();
  float left_load = curr_left_load;
  float split_point = curr_split_pt;
  float other_load = .0f;
  int left_count = 0;
  std::sort(final_data.begin(), final_data.end(), CompareLBShortCmp);
  // CkPrintf("rec (%d, %d) left_count = %d split_point = %.8f left_load = %.8f\n", rec.left, rec.right, left_count, split_point, left_load);
  for (auto d : final_data){
    if (left_load + d.load > .0f){
      if (-left_load > (left_load + d.load)){
        // split_point = d.coord;
        other_load = left_load;
        left_load += d.load;
        left_count ++;
        // CkPrintf("rec (%d, %d) left_count = %d split_point = %.8f left_load = %.8f\n", rec.left, rec.right, left_count, final_data[left_count].coord, left_load);
      } else {
        other_load = left_load + d.load;
      }
      break;
    }
    // split_point = d.coord;
    left_load += d.load;
    left_count ++;
    // CkPrintf("rec (%d, %d) left_count = %d split_point = %.8f left_load = %.8f\n", rec.left, rec.right, left_count, final_data[left_count].coord, left_load);
  }

  float total_load = .0f;
  for (auto d : final_data){
    total_load += d.load;
  }
  float left_total = curr_half_load + left_load;
  float right_total = rec.load - left_total;
  // split_point = final_data[left_count].coord;
  if (left_count > 0) split_point = final_data[left_count-1].coord;

  if (_lb_args.debug() >= debug_l0){
    CkPrintf("Final step for rec (%d, %d)\n", rec.left, rec.right);
    CkPrintf("\trec (%d, %d) size = %d load %.8f split point = %.8f split_count %d left_load = %.8f -> %.8f (other  = %.8f)\n", rec.left, rec.right, final_data.size(), total_load, split_point, left_count, curr_left_load, left_load, other_load);
    CkPrintf("\trec (%d, %d) left_total = %.8f right_total = %.8f\n",rec.left, rec.right, left_total, right_total);
  }

  thisProxy[rec.left].finishedPartitionOneDim(rec, split_point, left_total, curr_left_nobjs + left_count);
}

void DistributedOrbLB::finishedPartitionOneDim(DorbPartitionRec rec, float split_point, float split_load, int left_nobjs){
  // CkPrintf("\n######## rec (%d, %d) split_point = %.8f split_load = %.8f left_objs = %d\n\n", rec.left, rec.right, split_point, split_load, left_nobjs);
  int mid_idx  = (rec.left + rec.right) / 2;
  Vector3D<Real> left_lower_coord = rec.lower_coords;
  Vector3D<Real> left_upper_coord = rec.upper_coords;
  left_upper_coord[rec.dim] = split_point;
  Vector3D<Real> right_lower_coord = rec.lower_coords;
  right_lower_coord[rec.dim] = split_point;
  Vector3D<Real> right_upper_coord = rec.upper_coords;
  float curr_right_load = rec.load - split_load;

  int left_dim = getDim(rec.dim, left_lower_coord, left_upper_coord);
  DorbPartitionRec left_rec{
    left_dim, split_load, left_nobjs,
    rec.left, mid_idx,
    left_lower_coord[left_dim], left_upper_coord[left_dim],
    rec.granularity,
    left_lower_coord, left_upper_coord};

  int right_dim = getDim(rec.dim, right_lower_coord, right_upper_coord);
  DorbPartitionRec right_rec{
    right_dim, curr_right_load, rec.n_objs - left_nobjs,
    mid_idx, rec.right,
    right_lower_coord[right_dim], right_upper_coord[right_dim],
    rec.granularity,
    right_lower_coord, right_upper_coord};

  thisProxy[rec.left].bCastSectionMoveTokenWithSplitPoint(left_rec, right_rec);
  // thisProxy.moveTokenWithSplitPoint(right_rec);
}

void DistributedOrbLB::bCastSectionMoveTokenWithSplitPoint(DorbPartitionRec rec1, DorbPartitionRec rec2){
  updated_local_half_obj_collection = -1;
  if (rec2.right - rec1.left == CkNumPes()){
    thisProxy.moveTokenWithSplitPoint(rec1, rec2);
    return;
  }

  int my_idx = my_pe - rec1.left;
  int left_child = my_idx * 2 + rec1.left;
  int right_child = left_child + 1;
  if (left_child == rec1.left){
    thisProxy[left_child].moveTokenWithSplitPoint(rec1, rec2);
  } else if (left_child < rec2.right){
    thisProxy[left_child].bCastSectionMoveTokenWithSplitPoint(rec1, rec2);
    thisProxy[left_child].moveTokenWithSplitPoint(rec1, rec2);
  }

  if (right_child < rec2.right){
    thisProxy[right_child].bCastSectionMoveTokenWithSplitPoint(rec1, rec2);
    thisProxy[right_child].moveTokenWithSplitPoint(rec1, rec2);
  }
}

void DistributedOrbLB::moveTokenWithSplitPoint(DorbPartitionRec rec1, DorbPartitionRec rec2){
  if (my_pe == rec1.left || my_pe == rec2.left){
    left_half_rec = rec1;
    right_half_rec = rec2;
  }

  vector<LBCentroidAndIndexRecord> left_obj_collection;
  vector<LBCentroidAndIndexRecord> right_obj_collection;
  
  for (auto obj : obj_collection){
    if (inCurrentBox(obj.centroid, rec1.lower_coords, rec1.upper_coords)){
      left_obj_collection.push_back(obj);
    } else if (inCurrentBox(obj.centroid, rec2.lower_coords, rec2.upper_coords)){
      right_obj_collection.push_back(obj);
    }else{
      CkPrintf("ERROR!!");
    }
  }
  bool take_left = (my_pe < rec2.left);
  if (take_left){
    obj_collection = left_obj_collection;
    int send_to = (my_pe - rec1.left) % (rec2.right - rec2.left) + rec2.left;
    // CkPrintf("rec(%d, %d) Send to right %d -> %d\n", rec1.left, rec2.right, my_pe, send_to);
    thisProxy[send_to].recv_tokens(rec1, rec2, right_obj_collection);
  }else{
    obj_collection = right_obj_collection;
    int send_to = (my_pe - rec2.left) % (rec1.right - rec1.left) + rec1.left;
    // CkPrintf("rec(%d, %d) Send to left %d -> %d\n", rec1.left, rec2.right, my_pe, send_to);
    thisProxy[send_to].recv_tokens(rec1, rec2, left_obj_collection);
  }
  updated_local_half_obj_collection = 1;

  updateLocalObjCollection();
}

void DistributedOrbLB::recv_tokens(DorbPartitionRec rec1, DorbPartitionRec rec2, vector<LBCentroidAndIndexRecord> objs){
  for (auto & obj : objs){
    obj.to_pe = my_pe;
  }

  moved_in_tokens.insert(moved_in_tokens.end(), objs.begin(), objs.end());
  // moved_in_pe_count += 1;
  
  if (updated_local_half_obj_collection == 1){
    updateLocalObjCollection();
  }

  if (my_pe < rec2.left){
    thisProxy[rec1.left].ackTokenReceived();
  } else {
    thisProxy[rec2.left].ackTokenReceived();
  }
}

void DistributedOrbLB::updateLocalObjCollection(){
  obj_collection.insert(obj_collection.end(), moved_in_tokens.begin(), moved_in_tokens.end());
  moved_in_tokens.clear();
}

void DistributedOrbLB::ackTokenReceived(){
  moved_in_pe_count += 1;

  int to_recv = left_half_rec.right - left_half_rec.left;
  if (my_pe == left_half_rec.left){
    to_recv = right_half_rec.right - right_half_rec.left;
  }

  if (moved_in_pe_count == to_recv){
    // moved_in_tokens.clear();
    moved_in_pe_count = 0;
    // CkPrintf("[%d] rec(%d , %d) done moving\n", my_pe, rec1.left, rec2.right);

    if (my_pe == left_half_rec.left){
      prepareToRecurse(left_half_rec);
    } else {
      prepareToRecurse(right_half_rec);
    }
  }
}

void DistributedOrbLB::prepareToRecurse(DorbPartitionRec rec){
  // obj_collection.insert(obj_collection.end(), moved_in_tokens.begin(), moved_in_tokens.end());
  moved_in_tokens.clear();
  // ready_to_recurse = 0;
  thisProxy[my_pe].createPartitions(rec);
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
  partition_flags.clear();
  bin_loads_collection.clear();
  bin_sizes_collection.clear();
  fdata_collections.clear();
  section_updated_load.clear();
  section_updated_migrates.clear();
  section_updated_objects.clear();

  if (CkMyPe() == 0) {
    CkPrintf("DistributedOrbLB>> DistributedOrbLB strategy end at %lf\n", CmiWallTimer()-start_time);
  }
}

#include "DistributedOrbLB.def.h"
