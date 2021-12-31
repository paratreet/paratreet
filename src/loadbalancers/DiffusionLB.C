#include "elements.h"
#include "ckheap.h"
#include "DiffusionLB.h"

extern int quietModeRequested;
CkpvExtern(int, _lb_obj_index);

CreateLBFunc_Def(DiffusionLB, "Move jobs base on Diffusion.")

DiffusionLB::DiffusionLB(const CkLBOptions &opt): CBase_DiffusionLB(opt)
{
  lbname = (char *)"DiffusionLB";
  if (CkMyPe() == 0 && !quietModeRequested)
    CkPrintf("CharmLB> DiffusionLB created.\n");
  #if CMK_LB_USER_DATA
  if (CkpvAccess(_lb_obj_index) == -1)
    CkpvAccess(_lb_obj_index) = LBRegisterObjUserData(sizeof(LBUserData));
  #endif
}

void DiffusionLB::work(LDStats* stats)
{
  start_time = CmiWallTimer();
  int obj;
  n_pes = stats->nprocs();
  my_stats = stats;

  initVariables();
  collectPELoads();
  collectUserData();
  doDiffusion();
  printSummary();
  cleanUp();
}

void DiffusionLB::initVariables(){
  total_load = 0.0;
  max_load = 0.0;
  min_load = 0.0;
  average_load = 0.0;

  post_lb_max_load = 0.0;
  post_lb_min_load = 0.0;
  migration_ct = 0;

  pe_loads = vector<float>(n_pes, 0.0);
  pe_bgload = vector<float>(n_pes, 0.0);
  pe_bgload_share = vector<float>(n_pes, 0.0);
  pe_obj_loads = vector<float>(n_pes, 0.0);
  pe_nobjs = vector<int>(n_pes, 0);
  pe_nparticles = vector<int>(n_pes, 0);
  pe_obj_records = vector<vector<LBCentroidAndIndexRecord>>(n_pes, vector<LBCentroidAndIndexRecord>());
  Real zero = 0.0;
  pe_avg_centroids = vector<Vector3D<Real>>(n_pes, Vector3D<Real>(zero, zero, zero));

  pe_diffused_loads = vector<float>(n_pes, 0.0);
  pe_delta_loads = vector<float>(n_pes, 0.0);

  //post_lb_pe_loads = vector<float>(n_pes, 0.0);
  //post_lb_pe_nparticles = vector<int>(n_pes, 0);
  //map_sorted_idx_to_pe = vector<int>(n_pes, 0);
}

void DiffusionLB::collectPELoads(){
  float sum = 0.0;
  for(int i = 0; i < n_pes; i++){
    auto p = my_stats->procs[i];
    pe_loads[p.pe] = p.pe_speed * (p.total_walltime - p.idletime);
    sum += pe_loads[p.pe];
    pe_diffused_loads[p.pe] = pe_loads[p.pe];
  }
  max_load = *std::max_element(pe_loads.begin(), pe_loads.end());
  min_load = *std::min_element(pe_loads.begin(), pe_loads.end());
  average_load = sum / (float)n_pes;
  max_avg_ratio = max_load / average_load;
  max_min_ratio = max_load / min_load;

  if(_lb_args.debug()) CkPrintf("DiffusionLB >> Pre LB load:: range (%.4f, %.4f); sum = %.4f, average = %.4f; max_avg_ratio = %.4f, max_min_ratio = %.4f\n", min_load, max_load, sum, average_load, max_avg_ratio, max_min_ratio);
}

void DiffusionLB::collectUserData(){
  int sum = 0;
  for(int obj_idx = 0; obj_idx < my_stats->objData.size(); obj_idx++){
    LDObjData &oData = my_stats->objData[obj_idx];
    int pe = my_stats->from_proc[obj_idx];
    float obj_load = my_stats->procs[pe].pe_speed * oData.wallTime;

    #if CMK_LB_USER_DATA
    int idx = CkpvAccess(_lb_obj_index);
    LBUserData usr_data = *(LBUserData *)oData.getUserData(CkpvAccess(_lb_obj_index));
    if (usr_data.lb_type == LBCommon::pt){
      //CkPrintf("Obj load = %.8f\n", obj_load);
      pe_nobjs[pe] += 1;
      pe_nparticles[pe] += usr_data.particle_size;
      sum += usr_data.particle_size;
      pe_obj_loads[pe] += obj_load;

      pe_obj_records[pe].push_back(
          LBCentroidAndIndexRecord{
            usr_data.centroid, obj_idx,
            usr_data.particle_size,
            pe, obj_load
          });
    }
    else{
      // For non Parition objects, keep the PE
      my_stats->assign(obj_idx, pe);
    }
    #endif
  }

  average_particle_size = (float)sum  / (float)n_pes;
  printParticlesSummary();

  for (int i  = 0; i < n_pes; i++){
    pe_bgload[i] = pe_loads[i] - pe_obj_loads[i];
    pe_bgload_share[i] = pe_bgload[i] / (float)pe_nobjs[i];
    //CkPrintf("bg:: load = %.8f; share = %.8f\n",pe_bgload[i], pe_bgload_share[i]);
    // calculate the average centroid for each pe
    int ct = 0;
    for (auto rec : pe_obj_records[i]){
      pe_avg_centroids[i] += rec.centroid * (Real) rec.particle_size;
      ct += rec.particle_size;
    }
    pe_avg_centroids[i] /= (Real)ct;
    //ckout << "pe[" << i << "] " << pe_avg_centroids[i] <<endl;
  }

  // split bgload evenly to each object Parititon
  for (int pe = 0; pe < n_pes; pe++){
    for (auto & rec : pe_obj_records[pe]){
      rec.load += pe_bgload_share[pe];
    //CkPrintf("Rec:: load = %.8f\n", rec.load);
    }
  }
}

void DiffusionLB::printParticlesSummary(){
  max_particle_size = *std::max_element(pe_nparticles.begin(), pe_nparticles.end());
  min_particle_size = *std::min_element(pe_nparticles.begin(), pe_nparticles.end());
  max_avg_particle_ratio = max_particle_size / average_particle_size;


  if(_lb_args.debug()) CkPrintf("DiffusionLB >> Pre LB particle size:: range (%d, %d); average = %.2f; max_avg_ratio = %.4f\n", min_particle_size, max_particle_size, average_particle_size, max_avg_particle_ratio);
}

void DiffusionLB::doDiffusion(){
  // Calculate the diffused pe load
  int max_iter = 3;
  double alpha = 0.5;
  vector<float> load_cpy(pe_diffused_loads);

  for (int i = 0; i < max_iter; i++){
    for (int i = 0; i < n_pes - 1; i++){
      pe_diffused_loads[i] = alpha * load_cpy[i] + alpha * load_cpy[i+1];
    }
    pe_diffused_loads[n_pes - 1] = alpha * load_cpy[n_pes - 1] + alpha * load_cpy[0];

    load_cpy = pe_diffused_loads;
  }

  diffused_max_load = *std::max_element(pe_diffused_loads.begin(), pe_diffused_loads.end());

  if(_lb_args.debug()) CkPrintf("DiffusionLB>> Diffused max load = %.4f, max_avg_ratio = %.4f\n", diffused_max_load, diffused_max_load / average_load);

  // Find overload and underload pes
  for (int i = 0; i < n_pes; i++){
    float delta_load = pe_loads[i] - pe_diffused_loads[i];
    pe_delta_loads[i] = delta_load;
    if (delta_load > 0) overload_pes.push_back(i);
    else underload_pes.push_back(i);
  }

  pairNearestPE();
}

void DiffusionLB::calculateNearestPE(LBCentroidAndIndexRecord & rec){
  float min_dis = 100000.0;
  float min_pe = 0;

  for (int pe : underload_pes){
    float dis = (rec.centroid  - pe_avg_centroids[pe]).lengthSquared();
    if (dis < min_dis){
      min_dis = dis;
      min_pe = pe;
    }
  }
  rec.distance = min_dis;
  rec.to_pe = min_pe;
}

void DiffusionLB::pairNearestPE(){
  // move objects
  bool exit  = false;
  for (int current_overload_idx = 0; current_overload_idx < overload_pes.size(); current_overload_idx ++){
    int current_overload_pe = overload_pes[current_overload_idx];
    bool underload_pe_filled = false;
    bool overload_pe_filled = false;

    vector<LBCentroidAndIndexRecord> centroids(pe_obj_records[current_overload_pe]);
    if(_lb_args.debug() > 1) CkPrintf("%d ->, size = %d\n", current_overload_pe, centroids.size());
    for (auto & c : centroids){
      calculateNearestPE(c);
      if(_lb_args.debug() > 2) CkPrintf("\t distance = %.4f; to_pe = %d\n", c.distance, c.to_pe);
    }
    // sort in descending distance order
    std::sort(centroids.begin(), centroids.end(), DiffusionCompareCentroidDistance);

    while (pe_delta_loads[current_overload_pe] >= .0){
      LBCentroidAndIndexRecord rec = centroids.back();
      centroids.pop_back();
      if(_lb_args.debug() > 1) CkPrintf("DiffusionLB >> move %d to %d \n", rec.from_pe, rec.to_pe);
      my_stats->assign(rec.idx, rec.to_pe);
      pe_delta_loads[current_overload_pe] -= rec.load;
      if (pe_delta_loads[current_overload_pe] < .0){
        overload_pe_filled = true;
        if(_lb_args.debug() > 1) CkPrintf("Overload filled\n");
      }
      pe_delta_loads[rec.to_pe] += rec.load;

      if (pe_delta_loads[rec.to_pe] >= .0){
        underload_pes.erase(find(underload_pes.begin(), underload_pes.end(), rec.to_pe));
        if (underload_pes.empty()) {
          exit = true;
          if(_lb_args.debug() > 1) CkPrintf("Exit\n");
        }
        underload_pe_filled = true;
        if(_lb_args.debug() > 1) CkPrintf("Remove %d\n", rec.to_pe);
        if (!overload_pe_filled) current_overload_idx --;
        break;
      }
    }
    pe_obj_records[current_overload_pe] = centroids;
    if (exit) break;
  }
}

void DiffusionLB::pairOverAndUnderloadedPEs(){
  // move objects
  int current_underload_idx = 0;
  int current_underload_pe = underload_pes[0];
  bool exit = false;
  bool current_underload_pe_filled = false;
  for (int current_overload_idx = 0; current_overload_idx < overload_pes.size(); current_overload_idx ++){
    int current_overload_pe = overload_pes[current_overload_idx];
    bool update_overload_idx = false;
    vector<LBCentroidAndIndexRecord> centroids(pe_obj_records[current_overload_pe]);
    if(_lb_args.debug() > 1) CkPrintf("Prepare centroids for %d -> %d, size = %d\n", current_overload_pe, current_underload_pe, centroids.size());
    for (auto & c : centroids){
      c.distance = (c.centroid  - pe_avg_centroids[current_underload_pe]).lengthSquared();
      if(_lb_args.debug() > 2) ckout << c.centroid << "; " << pe_avg_centroids[current_underload_pe] <<endl;
      if(_lb_args.debug() > 2) CkPrintf("\t distance = %.4f\n", c.distance);
    }
    // sort in descending distance order
    //CkPrintf("Started sorting\n");
    std::sort(centroids.begin(), centroids.end(), DiffusionCompareCentroidDistance);
    //CkPrintf("Centroids sorted.\n");

    while(!centroids.empty()){
      if(_lb_args.debug() > 1) CkPrintf("Remaining size = %d\n", centroids.size());
      if(_lb_args.debug() > 1) CkPrintf("DiffusionLB >> move %d to %d \n", centroids.back().from_pe, current_underload_pe);
      my_stats->assign(centroids.back().idx, current_underload_pe);
      pe_delta_loads[current_underload_pe] += centroids.back().load;
      if (pe_delta_loads[current_underload_pe] >= .0){
        current_underload_pe_filled = true;
        if(_lb_args.debug() > 1) CkPrintf("Underload pe filled\n");
        current_underload_idx ++;
        if (current_underload_idx >= underload_pes.size()){
          exit = true;
          if(_lb_args.debug() > 1) CkPrintf("Underload end\n");
        }else{
          current_underload_pe = underload_pes[current_underload_idx];
        }
      }

      if(_lb_args.debug() > 2) CkPrintf("__line 206 __\n");
      pe_delta_loads[current_overload_pe] -= centroids.back().load;
      centroids.pop_back();
      if (pe_delta_loads[current_overload_pe] <= .0){
        pe_obj_records[current_overload_pe] = centroids;
        update_overload_idx = true;
        if(_lb_args.debug() > 1) CkPrintf("Overload pe filled\n");
        break;
      }
      if (current_underload_pe_filled || exit) {
        pe_obj_records[current_overload_pe] = centroids;
        break;
      };
    }
    if (!update_overload_idx){
      current_overload_idx --;
    }
    if (exit){
      break;
    }
  }
}

void DiffusionLB::printSummary(){
  vector<float> new_pe_loads = vector<float>(n_pes, 0.0);
  for (int i = 0; i < n_pes; i++){
    new_pe_loads[i] = pe_loads[i] + pe_delta_loads[i];
  }

  double new_max = *std::max_element(new_pe_loads.begin(), new_pe_loads.end());
  double new_ratio = new_max / average_load;


  if(_lb_args.debug()) CkPrintf("DiffusionLB >> Post LB particles: new_max = %.4f, max_avg_ratio = %.4f\n", new_max, new_ratio);
}

void DiffusionLB::cleanUp(){
  pe_obj_records.clear();

  pe_loads.clear();
  pe_bgload.clear();
  pe_bgload_share.clear();
  pe_obj_loads.clear();
  pe_nobjs.clear();
  pe_nparticles.clear();

  pe_diffused_loads.clear();
  overload_pes.clear();
  underload_pes.clear();
  pe_delta_loads.clear();
  //post_lb_pe_loads.clear();
  //post_lb_pe_nparticles.clear();
  //map_sorted_idx_to_pe.clear();
}

#include "DiffusionLB.def.h"
