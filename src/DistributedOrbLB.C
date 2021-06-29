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
  n_partitions = 0;
  n_particles = 0;
  max_obj_load = .0f;
  dim = 0;
  obj_coords = vector<vector<float>>(3, vector<float>());

  binary_loads = vector<float>(4, .0f);
  global_binary_loads = vector<float>(4, .0f);
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
  }
  //if (_lb_args.debug() == 2 && my_pe == 0){
  //  CkPrintf("obj[0] load with bg: %f\n", obj_collection[0].load);
  //}}}}
}

void DistributedOrbLB::reportPerLBStates(){
  int tupleSize = 3;/*{{{*/
  CkReduction::tupleElement tupleRedn[] = {
    CkReduction::tupleElement(sizeof(float), &total_load, CkReduction::max_float),
    CkReduction::tupleElement(sizeof(float), &total_load, CkReduction::sum_float),
    CkReduction::tupleElement(sizeof(float), &max_obj_load, CkReduction::max_float),
};
  CkReductionMsg* msg = CkReductionMsg::buildFromTuple(tupleRedn, tupleSize);
  CkCallback cb(CkIndex_DistributedOrbLB::perLBStates(0), thisProxy[0]);
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
  delete [] results;
  global_load = sum_load;
  granularity = global_max_obj_load / global_load;

  float avg_load = sum_load / (float)CkNumPes();
  CkPrintf("DistributedOrbLB >> Pre LB load sum = %.4f; max / avg load ratio = %.4f\n", sum_load, max_load / avg_load);/*}}}*/
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
  CkCallback cb(CkIndex_DistributedOrbLB::getUniverseDimensions(0), thisProxy[0]);
  msg->setCallback(cb);
  contribute(msg);
  /*}}}*/
}

void DistributedOrbLB::getUniverseDimensions(CkReductionMsg * msg){
  int numReductions;/*{{{*/
  CkReduction::tupleElement* results;

  ckout << "Universe coord :: ";
  msg->toTuple(&results, &numReductions);
  for (int i = 0; i < 6; i++){
    universe_coords.push_back(*(float*)results[i].data);
    ckout << universe_coords[i] << "; ";
  }
  ckout << endl;
  delete [] results;

  curr_dim = 0;
  curr_depth = 0;
  left_load_col.push_back(.0f);
  curr_load = global_load;
  curr_lower_coords = Vector3D<Real>(universe_coords[0], universe_coords[2], universe_coords[4]);
  curr_upper_coords = Vector3D<Real>(universe_coords[1], universe_coords[3], universe_coords[5]);

  createPartitions(curr_dim, curr_load, curr_lower_coords, curr_upper_coords);
  /*}}}*/
}

void DistributedOrbLB::createPartitions(int dim, float load, Vector3D<Real> lower_coords, Vector3D<Real> upper_coords){
  thisProxy.binaryLoadPartition(dim, load, lower_coords, upper_coords);
}

void DistributedOrbLB::binaryLoadPartition(int dim, float load, Vector3D<Real> lower_coords, Vector3D<Real> upper_coords){
  if (my_pe == 0){
    ckout << lower_coords << "--" << upper_coords <<endl;
  }
  binary_loads[0] = .0f;
  binary_loads[1] = .0f;

  float mid_coord = (lower_coords[dim] + upper_coords[dim]) / 2;
  for (auto & obj : obj_collection){
    if (obj.centroid[dim] < mid_coord){
      binary_loads[0] += obj.load;
    } else {
      binary_loads[1] += obj.load;
    }
  }

  CkPrintf("PE[%d] binary_loads = %.8f, %.8f\n", my_pe, binary_loads[0], binary_loads[1]);

  gatherBinaryLoads();
}

void DistributedOrbLB::gatherBinaryLoads(){
  int tupleSize = 2;/*{{{*/
  CkReduction::tupleElement tupleRedn[] = {
    CkReduction::tupleElement(sizeof(float), &binary_loads[0], CkReduction::sum_float),
    CkReduction::tupleElement(sizeof(float), &binary_loads[1], CkReduction::sum_float),
};
  CkReductionMsg* msg = CkReductionMsg::buildFromTuple(tupleRedn, tupleSize);
  CkCallback cb(CkIndex_DistributedOrbLB::getSumBinaryLoads(0), thisProxy[0]);
  msg->setCallback(cb);
  contribute(msg);/*}}}*/
}

void DistributedOrbLB::getSumBinaryLoads(CkReductionMsg * msg){
  int numReductions;/*{{{*/
  CkReduction::tupleElement* results;
  msg->toTuple(&results, &numReductions);
  for (int i = 0; i < 2; i++){
    global_binary_loads[i] = *(float*)results[i].data;
  }
  delete [] results;
  left_load_col[curr_depth] = global_binary_loads[0];
  float delta = left_load_col[curr_depth]/curr_load - 0.5;
  CkPrintf("global_binary_loads = %.8f, %.8f, acc_left_load = %.4f, granularity = %.4f, curr_left_load_offset = %.4f\n", global_binary_loads[0], global_binary_loads[1], left_load_col[curr_depth], granularity, delta);

  float split_pt = (curr_lower_coords[curr_dim] + curr_upper_coords[curr_dim])/2;
  // The split point is good enough
  if (delta > -granularity && delta < granularity){
    curr_depth += 1;
    left_load_col.push_back(.0f);
    split_coords.push_back(split_pt);
    lower_coords_col.push_back(curr_lower_coords);
    upper_coords_col.push_back(curr_upper_coords);
    curr_dim = (curr_dim + 1) % 3;
  } else {
    // Keep partition the larger section to find better split point
    if (delta < 0){
      // Futher partition the right part
      curr_lower_coords[curr_dim] = split_pt;
    } else {
      // Futher patition the left part
      curr_upper_coords[curr_dim] = split_pt;
    }

    ckout << "dim " << curr_dim <<" split_pt = "<< split_pt << " acc_left = "<< left_load_col[curr_depth] << " lower "<< curr_lower_coords << " upper " << curr_upper_coords << endl;
    CkPrintf("\n\n");
    thisProxy.binaryLoadPartition(curr_dim, curr_load, curr_lower_coords, curr_upper_coords);
  }

  /*}}}*/
}

void DistributedOrbLB::reset(){
  universe_coords.clear();
  obj_coords.clear();
  lower_coords_col.clear();
  upper_coords_col.clear();
  load_col.clear();
  left_load_col.clear();
}

#include "DistributedOrbLB.def.h"
