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
  curr_dim = -1;
  bool use_longest_dim = false;
  obj_coords = vector<vector<float>>(3, vector<float>());

  global_octal_loads = vector<float>(4, .0f);
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

int DistributedOrbLB::getDim(){
  if (!use_longest_dim){/*{{{*/
    return (curr_dim + 1) % 3;
  }

  return -1;/*}}}*/
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
  float margin = .0001;

  curr_lower_coords = Vector3D<Real>(universe_coords[0] - margin, universe_coords[2] - margin, universe_coords[4] - margin);
  curr_upper_coords = Vector3D<Real>(universe_coords[1] + margin, universe_coords[3] + margin, universe_coords[5] + margin);
  curr_dim = getDim();
  curr_depth = 0;
  //left_load_col.push_back(.0f);
  curr_load = global_load;

  createPartitions(curr_dim, curr_load, 0, CkNumPes(), curr_lower_coords, curr_upper_coords);
  /*}}}*/
}

void DistributedOrbLB::createPartitions(int dim, float load, int left, int right, Vector3D<Real> lower_coords, Vector3D<Real> upper_coords){
  CkPrintf("createPartitions dim %d load %.4f left %d right %d\n", dim, load, left, right);
  left_idx = left;
  right_idx = right;
  lower_split = lower_coords[dim];
  upper_split = upper_coords[dim];
  curr_cb = new CkCallbackResumeThread();
  thisProxy.binaryLoadPartitionWithOctalBins(dim, load, left, right, lower_split, upper_split, lower_coords, upper_coords, *curr_cb);
  delete curr_cb;
}

bool inCurrentBox(Vector3D<Real> & centroid, Vector3D<Real> & lower_coords, Vector3D<Real> & upper_coords){
  bool ret = true;
  for (int i = 0; i < 3; i++){
    ret = ret
      && centroid[i] >= lower_coords[i]
      && centroid[i] <= upper_coords[i];
  }

  return ret;
}

void DistributedOrbLB::binaryLoadPartitionWithOctalBins(int dim, float load, int left, int right, float low, float high, Vector3D<Real> lower_coords, Vector3D<Real> upper_coords, const CkCallback & curr_cb){
  if (my_pe == 0){
    ckout << lower_coords << "--" << upper_coords << "; " << low << " - " << high << endl;
  }
  octal_loads = vector<float>(8, .0f);

  float delta_coord = (high - low) / 8;
  for (auto & obj : obj_collection){
    if (inCurrentBox(obj.centroid, lower_coords, upper_coords)){
      int idx = (obj.centroid[dim] - low)/delta_coord;
      octal_loads[idx] += obj.load;

      ckout << "\t\t++ Add to bin " << idx << "; " << obj.centroid << endl;
    }
    else{
      CkPrintf("\t\tNot in the current box\n");
    }
  }

  CkPrintf("\t\tPE[%d] octal_loads = %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f\n", my_pe,
      octal_loads[0], octal_loads[1], octal_loads[2], octal_loads[3],
      octal_loads[4], octal_loads[5], octal_loads[6], octal_loads[7]);

  gatherOctalLoads();
}

void DistributedOrbLB::gatherOctalLoads(){
  int tupleSize = 8;/*{{{*/
  CkReduction::tupleElement tupleRedn[tupleSize];
  for (int i = 0; i < 8; i++){
    tupleRedn[i] = CkReduction::tupleElement(sizeof(float), &octal_loads[i], CkReduction::sum_float);
  }

  CkReductionMsg* msg = CkReductionMsg::buildFromTuple(tupleRedn, tupleSize);
  CkCallback cb(CkIndex_DistributedOrbLB::getSumOctalLoads(0), thisProxy[0]);
  msg->setCallback(cb);
  contribute(msg);/*}}}*/
}

void DistributedOrbLB::getSumOctalLoads(CkReductionMsg * msg){
  int numReductions;/*{{{*/
  CkReduction::tupleElement* results;
  float total_load = .0f;
  msg->toTuple(&results, &numReductions);
  for (int i = 0; i < 8; i++){
    global_octal_loads[i] = *(float*)results[i].data;
    total_load += global_octal_loads[i];
  }
  delete [] results;
  CkPrintf("\tGlobal_octal_loads = total = %.8f; curr_load = %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f\n", total_load, curr_load,
      global_octal_loads[0], global_octal_loads[1], global_octal_loads[2], global_octal_loads[3],
      global_octal_loads[4], global_octal_loads[5], global_octal_loads[6], global_octal_loads[7]);
  //left_load_col[curr_depth] = global_octal_loads[0];
  curr_left_load = .0f;
  int curr_split_idx = 1;
  float half_load = curr_load / 2;

  // Find the split idx that cuts octal bin into two parts of even loads
  vector<float> prefix_octal_load(8, .0f);
  prefix_octal_load[0] = global_octal_loads[0] - half_load;
  for (int i = 1; i < 8; i++){
    prefix_octal_load[i] = global_octal_loads[i] + prefix_octal_load[i-1];
  }
  CkPrintf("\tprefix_octal_load = %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f, %.8f\n",
      prefix_octal_load[0], prefix_octal_load[1], prefix_octal_load[2], prefix_octal_load[3],
      prefix_octal_load[4], prefix_octal_load[5], prefix_octal_load[6], prefix_octal_load[7]);

  float curr_delta = prefix_octal_load[0];
  for (int i = 1; i < 8; i++){
    float abs_delta = std::fabs(prefix_octal_load[i]);
    if (abs_delta < std::fabs(curr_delta)){
      curr_delta = prefix_octal_load[i];
      curr_split_idx = i+1;
    }
  }

  float split_pt = (upper_split - lower_split)/8 * curr_split_idx;
  split_pt += lower_split;

  CkPrintf("\tcurr_delta = %.8f, granularity = %.4f, curr_split_idx = %d, split_pt = %.8f\n", curr_delta, granularity, curr_split_idx, split_pt);

  // The split point is good enough
  if (std::fabs(curr_delta) <= granularity){
    split_coords.push_back(split_pt);
    curr_split_pt = split_pt;
    lower_coords_col.push_back(curr_lower_coords);
    upper_coords_col.push_back(curr_upper_coords);
    thisProxy.finishedPartitionOneDim(*curr_cb);
  } else {
    // Keep partition the larger section to find better split point
    int posi_idx = 0;
    if (curr_delta < 0){
      // Futher partition the right part
      for (int i = 0; i < 8; i++){
        if (prefix_octal_load[i] > 0.0){
          posi_idx = i;
          upper_split = lower_split + (upper_split - lower_split) * (i+1);
          break;
        }
      }
      lower_split = split_pt;
    } else {
      // Futher patition the left part
      for (int i = 0; i < 8; i++){
        if (prefix_octal_load[i] > 0.0){
          posi_idx = i;
          lower_split = lower_split + (upper_split - lower_split) * i;
          break;
        }
      }
      upper_split = split_pt;
    }

    CkPrintf("\tNEW idx (%d, %d) lower = %.8f upper = %.8f\n", curr_split_idx-1,posi_idx );
    thisProxy.binaryLoadPartitionWithOctalBins(curr_dim, curr_load, left_idx, right_idx, lower_split, upper_split, curr_lower_coords, curr_upper_coords, *curr_cb);
  }
  /*}}}*/
}


void DistributedOrbLB::finishedPartitionOneDim(const CkCallback & curr_cb){
  contribute(curr_cb);
}

void DistributedOrbLB::reset(){
  universe_coords.clear();
  obj_coords.clear();
  //lower_coords_col.clear();
  //upper_coords_col.clear();
  //load_col.clear();
  //left_load_col.clear();
}

#include "DistributedOrbLB.def.h"
