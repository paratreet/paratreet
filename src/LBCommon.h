#ifndef PARATREET_USER_LBCOMMON_H_
#define PARATREET_USER_LBCOMMON_H_

#include "common.h"

namespace LBCommon{
  enum LBType {
    pt = 1111, // Partition
    st = 2222, // Subtree
    cp = 3333  // TreeCanopy
  };

  struct LBUserData{
    int lb_type;
    int chare_idx;
    int particle_size;
    int particle_sum;
    Vector3D<Real> centroid;
    Key sfc_key;

    void pup(PUP::er &p){
      p | lb_type;
      p | chare_idx;
      p | particle_size;
      p | particle_sum;
      p | centroid;
    };
  };

  struct LBCompareStats{
    int index; // index from objData
    int chare_idx; // index from Subtree or Partition chare arrays
    int particle_size;
    const LDObjData* data_ptr;
    int from_proc;
    Vector3D<Real> centroid;
    inline void pup(PUP::er &p);
  };

};

#endif //PARATREET_USER_LBCOMMON_H_
