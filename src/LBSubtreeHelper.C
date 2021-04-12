#include "DistributedPrefixLB.h"

void DistributedPrefixLB::subtreeLBInits(){
  subtree_migrate_out_ct = std::vector<int> (CkNumPes(), 0);
  total_subtree_migrates = 0;
  broadcastLocalPartitionCentroids();
}

void DistributedPrefixLB::subtreeLBCleanUp(){
  global_partition_centroids.clear();
  subtree_migrate_out_ct.clear();
  local_partition_centroids.clear();
  recv_incoming_subtree_counts = 1;
  incoming_subtree_migrations = 0;
  recv_pe_centroids_ct = 0;
}

void DistributedPrefixLB::sendPEParitionCentroids(int sender_pe, std::vector<Vector3D<Real>> centroids){
  for (auto c : centroids){
    global_partition_centroids.push_back(
        LBCentroidRecord{c, sender_pe}
        );
  }
  recv_pe_centroids_ct ++;
  if(recv_pe_centroids_ct == CkNumPes()){
    makeSubtreeMoves();
  }
}

void DistributedPrefixLB::broadcastLocalPartitionCentroids(){
  for (int i = 0; i < CkNumPes(); i++){
    thisProxy[i].sendPEParitionCentroids(my_pe, local_partition_centroids);
  }
}

void DistributedPrefixLB::sendSubtreeMigrationDecisions(int count){
    incoming_subtree_migrations += count;
    recv_incoming_subtree_counts += 1;
    if (_lb_args.debug() >= 3)
      CkPrintf("PE[%d] recived decisions from %d PEs, incoming_subtree_migrations = %d\n", my_pe, recv_incoming_subtree_counts, incoming_subtree_migrations);
    if (recv_incoming_subtree_counts == CkNumPes()){
      migrates_expected = incoming_subtree_migrations;
      if (_lb_args.debug() >= 1)
        CkPrintf("PE[%d] migrates_expected = %d; migrate_out = %d; total_obj = %d\n", my_pe, migrates_expected, total_subtree_migrates, st_ct);
      PackAndMakeMigrateMsgs(total_subtree_migrates, st_ct);
    }
}

void DistributedPrefixLB::makeSubtreeMoves(){
  for (LBCompareStats & oStat : st_obj_map){
    int to_pe = calculateTargetPE(oStat.centroid, oStat.particle_size);
    if (my_pe != to_pe){
      total_subtree_migrates ++;
      if (total_subtree_migrates == st_ct){
        subtree_migrate_out_ct[my_pe] ++;
        total_subtree_migrates --;
        continue;
      }
      subtree_migrate_out_ct[to_pe] ++;
      MigrateInfo * move = new MigrateInfo;
      move->obj = oStat.data_ptr->handle;
      move->from_pe = my_pe;
      move->to_pe = to_pe;
      migrate_records.push_back(move);
    }else{
      subtree_migrate_out_ct[my_pe] ++;
    }
  }
  if (_lb_args.debug() >= 2)
    CkPrintf("PE[%d] start to broadcast decisions\n");

  // broadcast subtree migrate decisions
  for (int i = 0; i < CkNumPes(); i++){
    if (i == my_pe) continue;
    thisProxy[i].sendSubtreeMigrationDecisions(subtree_migrate_out_ct[i]);
  }

}

int DistributedPrefixLB::calculateTargetPE(Vector3D<Real> c, int size){
  std::vector<LBCentroidCompare> tmp_centroid_records;
  for (auto & r : global_partition_centroids){
    //ckout << r.centroid << "; " << c << "; " <<r.centroid - c << endl;
    tmp_centroid_records.push_back(LBCentroidCompare{(r.centroid - c).lengthSquared(), r.from_pe});
  }

  // select the top nearestK
  std::nth_element(tmp_centroid_records.begin(), tmp_centroid_records.begin()+nearestK, tmp_centroid_records.end(), CentroidRecordCompare);

  // sort the top nearestK
  std::sort(tmp_centroid_records.begin(), tmp_centroid_records.begin()+nearestK, CentroidRecordCompare);

  // target_pe = my_pe if it is in the top nearestK list
  // target_pe = the pe with the closest partition centroid
  int target_pe = tmp_centroid_records[0].from_pe;
  for (int i = 1; i < nearestK; i++){
    if (tmp_centroid_records[i].from_pe == my_pe){
      target_pe = my_pe;
    }
  }

  if (_lb_args.debug() >= 1 && (target_pe != my_pe)) CkPrintf("PE[%d] subtree size = %d; targetPE is:: %d; top %d nearestK are [%d %d %d %d %d], distances are [%.4f %.4f %.4f %.4f %.4f]\n", my_pe, size, target_pe, nearestK,
      tmp_centroid_records[0].from_pe,
      tmp_centroid_records[1].from_pe,
      tmp_centroid_records[2].from_pe,
      tmp_centroid_records[3].from_pe,
      tmp_centroid_records[4].from_pe,
      tmp_centroid_records[0].distance,
      tmp_centroid_records[1].distance,
      tmp_centroid_records[2].distance,
      tmp_centroid_records[3].distance,
      tmp_centroid_records[4].distance);
  return target_pe;
}
