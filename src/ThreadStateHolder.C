#include "ThreadStateHolder.h"

void ThreadStateHolder::collectAndResetStats(CkCallback cb) {
#if COUNT_INTERACTIONS
  CkPrintf("%lu particles on pe %d\n", n_partition_particles, CkMyPe());
  unsigned long long intrn_counts [4] = {n_node_ints, n_part_ints, n_opens, n_closes};
  CkPrintf("on PE %d: %llu node-particle interactions, %llu bucket-particle interactions %llu node opens, %llu node closes\n", CkMyPe(), intrn_counts[0], intrn_counts[1], intrn_counts[2], intrn_counts[3]);
  this->contribute(4 * sizeof(unsigned long long), &intrn_counts, CkReduction::sum_ulong_long, cb);
#endif
  reset();
}

void ThreadStateHolder::collectMetaData (const CkCallback & cb) {
  int nParticles = n_subtree_particles;
  const size_t numTuples = 2;
  CkReduction::tupleElement tupleRedn[] = {
    CkReduction::tupleElement(sizeof(nParticles), &nParticles, CkReduction::max_int),
    CkReduction::tupleElement(sizeof(nParticles), &nParticles, CkReduction::sum_int)
  };
  CkReductionMsg * msg = CkReductionMsg::buildFromTuple(tupleRedn, numTuples);
  msg->setCallback(cb);
  this->contribute(msg);
}
