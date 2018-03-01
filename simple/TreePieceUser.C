#include "TreePiece.h"

#define CK_TEMPLATES_ONLY
#include "user.def.h"
#undef CK_TEMPLATES_ONLY

template <>
void TreePiece::calculateData<CentroidData>(CentroidData d) {
  CentroidData cd;
  for (int i = 0; i < particles.size(); i++) {
    cd.moment += particles[i].position * particles[i].mass;
    cd.sum_mass += particles[i].mass;
  }
  //CkCallback cb(CkReductionTarget(Main, summass), mainProxy);
  //contribute(sizeof(float),&sum_mass,CkReduction::sum_float, cb);
  CentroidVisitor v;
  v.leaf(cd, tp_key); 
} 
