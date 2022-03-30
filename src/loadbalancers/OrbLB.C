#include "elements.h"
#include "ckheap.h"
#include "OrbLB.h"

extern int quietModeRequested;
CkpvExtern(int, _lb_obj_index);

CreateLBFunc_Def(OrbLB, "Move jobs base on Orb.")

OrbLB::OrbLB(const CkLBOptions &opt): CBase_OrbLB(opt)
{
  lbname = (char *)"OrbLB";
  if (CkMyPe() == 0 && !quietModeRequested)
    CkPrintf("CharmLB> OrbLB created.\n");
  #if CMK_LB_USER_DATA
  if (CkpvAccess(_lb_obj_index) == -1)
    CkpvAccess(_lb_obj_index) = LBRegisterObjUserData(sizeof(LBUserData));
  #endif
}

void OrbLB::work(LDStats* stats)
{
  start_time = CmiWallTimer();
  int obj;
  n_pes = stats->nprocs();
  CkPrintf("*** LB strategy init at %.3lf ms\n", CmiWallTimer() * 1000);
  //CkPrintf("n_pes = %d\n", n_pes);
  my_stats = stats;

  initVariables();
  collectPELoads();
  collectUserData();
  orbCentroids();
  summary();
  cleanUp();
}

void OrbLB::collectUserData(){
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
      pe_obj_size[pe] += 1;
      pe_particle_size[pe] += usr_data.particle_size;
      sum += usr_data.particle_size;
      pe_objload[pe] += obj_load;

      centroids.push_back(
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

  max_particle_size = *std::max_element(pe_particle_size.begin(), pe_particle_size.end());
  min_particle_size = *std::min_element(pe_particle_size.begin(), pe_particle_size.end());
  average_particle_size = (float)sum  / (float)n_pes;
  max_avg_particle_ratio = max_particle_size / average_particle_size;


  if(_lb_args.debug()) CkPrintf("OrbLB >> Pre LB particle size:: range (%d, %d); average = %.2f; max_avg_ratio = %.4f\n", min_particle_size, max_particle_size, average_particle_size, max_avg_particle_ratio);

  for (int i  = 0; i < n_pes; i++){
    pe_bgload[i] = pe_loads[i] - pe_objload[i];
    pe_bgload_share[i] = pe_bgload[i] / pe_obj_size[i];
    //CkPrintf("bg:: load = %.8f; share = %.8f\n",pe_bgload[i], pe_bgload_share[i]);
  }

  // split bgload evenly to each object Parititon
  for (auto & rec : centroids){
    rec.load += pe_bgload_share[rec.from_pe];
    //CkPrintf("Rec:: load = %.8f\n", rec.load);
  }

  // debug prints
  //int obj_sum = 0;
  //for (int i = 0; i < n_pes; i++){
  //  obj_sum += pe_pt_ct[i];
  //  CkPrintf("PE[%d] obj ct = %d ; pt_load = %.4f nonpt_load = %.4f\n", i, pe_pt_ct[i], pe_pt_loads[i], pe_nonpt_loads[i]);
  //}

  //CkPrintf("Sum = %d ; n_migt = %d; pt_ct = %d; st_ct = %d; other = %d\n", obj_sum, my_stats->n_migrateobjs, pt_ct, st_ct, nonpt_ct);

}

void OrbLB::collectPELoads(){
  float sum = 0.0;
  for(int i = 0; i < n_pes; i++){
    auto p = my_stats->procs[i];
    pe_loads[p.pe] = p.pe_speed * (p.total_walltime - p.idletime);
    sum += pe_loads[p.pe];
  }
  max_load = *std::max_element(pe_loads.begin(), pe_loads.end());
  min_load = *std::min_element(pe_loads.begin(), pe_loads.end());
  average_load = sum / (float)n_pes;
  max_avg_ratio = max_load / average_load;
  max_min_ratio = max_load / min_load;

  if(_lb_args.debug()) CkPrintf("OrbLB >> Pre LB load:: range (%.4f, %.4f); sum = %.4f, average = %.4f; max_avg_ratio = %.4f, max_min_ratio = %.4f\n", min_load, max_load, sum, average_load, max_avg_ratio, max_min_ratio);
}

int OrbLB::findLongestDimInCentroids(int start, int end){
  vector<float> dim_min(3, 3.40282e+38);
  vector<float> dim_max(3, 1.17549e-38);

  for (int idx = start; idx < end; idx ++){
    LBCentroidAndIndexRecord & rec = centroids[idx];
    for (int i = 0; i < 3; i++){
      dim_min[i] = std::min(dim_min[i], rec.centroid[i]);
      dim_max[i] = std::max(dim_max[i], rec.centroid[i]);
    }
  }

  int longest_dim = 0;
  float max_delta = dim_max[0] - dim_min[0];
  for (int i = 1; i < 3; i++){
    float dim_delta = dim_max[i] - dim_min[i];
    if (max_delta < dim_delta){
      longest_dim = i;
      max_delta = dim_delta;
    }
  }

  //return 0;
  return longest_dim;
}

float OrbLB::calcPartialLoadSum(int start, int end){
  float sum = 0.0;
  for (int idx = start; idx < end; idx ++){
    sum += centroids[idx].load;
  }

  return sum;
}

void OrbLB::assign(int idx, int to_pe){
  my_stats->assign(centroids[idx].idx, to_pe);
  post_lb_pe_loads[to_pe] += centroids[idx].load;
  post_lb_pe_particle_size[to_pe] += centroids[idx].particle_size;
  if (_lb_args.debug() >= 3)
    ckout << "PE " << to_pe << " <- " << centroids[idx].from_pe << " Centroid : " << centroids[idx].centroid << endl;

  if (centroids[idx].from_pe != to_pe){
    migration_ct += 1;
  }
}

int OrbLB::calcPartialParticleSum(int start, int end){
  int sum = 0;
  for (int idx = start; idx < end; idx ++){
    sum += centroids[idx].particle_size;
  }

  return sum;
}

void OrbLB::orbCentroidsForEvenParticleSizeRecursiveHelper(int start_pe, int end_pe, int start_cent, int end_cent, int depth){
  // iterval = 0
  if ((end_pe - start_pe) == 1){
    for (int i = start_cent; i < end_cent; i++){
      assign(i, start_pe);
    }
    return;
  }

  // iterval > 0;
  int mid_pe = (start_pe + end_pe) / 2;
  int dim = depth % 3;
  if (use_longest_dim)
    dim = findLongestDimInCentroids(start_cent, end_cent);

  int partial_particle_sum = calcPartialParticleSum(start_cent, end_cent);
  std::sort(centroids.begin()+start_cent, centroids.begin()+end_cent, NdComparator(dim));
  int half_size = partial_particle_sum / 2;

  int half_load_idx = 0;
  for (int idx = start_cent; idx < end_cent; idx ++){
    int half = centroids[idx].particle_size / 2;
    int rest = centroids[idx].particle_size - half;
    half_size -= half;
    if (half_size < 0){
      half_load_idx = idx;
      break;
    }
    half_size -= rest;
  }

  if(_lb_args.debug() >= 2){
    CkPrintf("recurse on PE [%d, %d, %d) with centroids [%d, %d, %d)\n", start_pe, mid_pe, end_pe, start_cent, half_load_idx, end_cent);
    CkPrintf("dim = %d; partial_particle_sum = %d\n", dim, partial_particle_sum);
  }

  // recurse on the left
  orbCentroidsForEvenParticleSizeRecursiveHelper(start_pe, mid_pe, start_cent, half_load_idx, depth+1);
  // recurse on the right
  orbCentroidsForEvenParticleSizeRecursiveHelper(mid_pe, end_pe, half_load_idx, end_cent, depth+1);

}

void OrbLB::orbCentroidsForEvenLoadRecursiveHelper(int start_pe, int end_pe, int start_cent, int end_cent, int depth){
  // iterval = 0;
  if ((end_pe - start_pe) == 1){
    for (int i = start_cent; i < end_cent; i++){
      assign(i, start_pe);
    }
    return;
  }

  // iterval > 0;
  int mid_pe = (start_pe + end_pe) / 2;
  int dim = depth % 3;
  if (use_longest_dim)
    dim = findLongestDimInCentroids(start_cent, end_cent);
  float partial_load = calcPartialLoadSum(start_cent, end_cent);
  std::sort(centroids.begin()+start_cent, centroids.begin()+end_cent, NdComparator(dim));
  float half_partial_load = partial_load / 2;

  int half_load_idx = 0;
  for (int idx = start_cent; idx < end_cent; idx ++){
    half_partial_load -= centroids[idx].load;
    if (half_partial_load < 0){
      half_load_idx = idx;
      break;
    }
  }

  if(_lb_args.debug() >= 2){
    CkPrintf("recurse on PE [%d, %d, %d) with centroids [%d, %d, %d)\n", start_pe, mid_pe, end_pe, start_cent, half_load_idx, end_cent);
    CkPrintf("dim = %d; partial_load = %.4f\n", dim, partial_load);
  }

  // recurse on the left
  orbCentroidsForEvenLoadRecursiveHelper(start_pe, mid_pe, start_cent, half_load_idx, depth+1);
  // recurse on the right
  orbCentroidsForEvenLoadRecursiveHelper(mid_pe, end_pe, half_load_idx, end_cent, depth+1);

}

void OrbLB::orbCentroids(){
  orbCentroidsForEvenLoadRecursiveHelper(0, n_pes, 0, centroids.size(), 0);
}

void OrbLB::initVariables(){
  total_load = 0.0;
  max_load = 0.0;
  min_load = 0.0;
  post_lb_max_load = 0.0;
  post_lb_min_load = 0.0;
  average_load = 0.0;
  migration_ct = 0;

  pe_loads = vector<float>(n_pes, 0.0);
  pe_bgload = vector<float>(n_pes, 0.0);
  pe_objload = vector<float>(n_pes, 0.0);
  pe_bgload_share = vector<float>(n_pes, 0.0);
  pe_obj_size = vector<int>(n_pes, 0);
  pe_particle_size = vector<int>(n_pes, 0);
  post_lb_pe_loads = vector<float>(n_pes, 0.0);
  post_lb_pe_particle_size = vector<int>(n_pes, 0);
  map_sorted_idx_to_pe = vector<int>(n_pes, 0);
}

void OrbLB::summary(){
  float load_sum = 0.0;
  int particle_size_sum = 0;
  for(int i = 0; i < n_pes; i++){
    load_sum += post_lb_pe_loads[i];
    particle_size_sum += post_lb_pe_particle_size[i];

    //CkPrintf("[%d] pe load = %.4f; bg = %.4f, obj = %.4f, rest = %.4f\n", i, pe_loads[i], pe_bgload[i], pe_objload[i], (pe_loads[i] - pe_bgload[i] - pe_objload[i]));
  }

  post_lb_max_load = *std::max_element(post_lb_pe_loads.begin(), post_lb_pe_loads.end());
  post_lb_min_load = *std::min_element(post_lb_pe_loads.begin(), post_lb_pe_loads.end());

  post_lb_max_size = *std::max_element(post_lb_pe_particle_size.begin(), post_lb_pe_particle_size.end());
  post_lb_min_size = *std::min_element(post_lb_pe_particle_size.begin(), post_lb_pe_particle_size.end());
  post_lb_size_max_avg_ratio = (float)post_lb_max_size / average_particle_size;

  post_lb_average_load = load_sum / (float)n_pes;
  post_lb_max_avg_ratio = post_lb_max_load / post_lb_average_load;
  post_lb_max_min_ratio = post_lb_max_load / post_lb_min_load;
  migrate_ratio = migration_ct;
  migrate_ratio /= (float) centroids.size();
  double end_time = CmiWallTimer();
  CkPrintf("*** Centralized_OrbLB strategy end at %.3lf ms\n", CmiWallTimer() * 1000);
  if(_lb_args.debug()) CkPrintf("OrbLB >> Post LB load:: range (%.4f, %.4f); average = %.4f; max_avg_ratio = %.4f, max_min_ratio = %.4f\nOrbLB >> Post LB particle size:: range(%d, %d); max_avg_ratio = %.4f\nOrbLB >>> Summary:: moved %d/%d objs, ratio = %.2f (strategy time = %.2f ms)\n", post_lb_min_load, post_lb_max_load, post_lb_average_load, post_lb_max_avg_ratio, post_lb_max_min_ratio, post_lb_min_size, post_lb_max_size, post_lb_size_max_avg_ratio, migration_ct, centroids.size(), migrate_ratio, (end_time - start_time)*1000 );
}

void OrbLB::cleanUp(){
  centroids.clear();
  pe_loads.clear();
  pe_bgload.clear();
  pe_bgload_share.clear();
  pe_objload.clear();
  pe_obj_size.clear();
  pe_particle_size.clear();
  post_lb_pe_loads.clear();
  post_lb_pe_particle_size.clear();
  map_sorted_idx_to_pe.clear();
}

#include "OrbLB.def.h"
