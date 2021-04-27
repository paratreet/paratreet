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
    int partical_size;
    double load;
    const LDObjData* data_ptr;
    int from_proc;
    Vector3D<Real> centroid;
    inline void pup(PUP::er &p);
  };

  struct LBCentroidRecord{
    Vector3D<Real> centroid;
    int from_pe;
    inline void pup(PUP::er &p);
  };

  struct LBCentroidCompare{
    Real distance;
    int from_pe;
    inline void pup(PUP::er &p);
  };
};

namespace{
  bool CompareLBStats(const LBCommon::LBCompareStats & a, const LBCommon::LBCompareStats & b)
  {
    // compare chare array index
    return a.chare_idx < b.chare_idx;
  };

  bool ComparePEAndChareidx(const LBCommon::LBCompareStats & a, const LBCommon::LBCompareStats &b)
  {
    // first compare PE
    int a_pe = a.from_proc, b_pe = b.from_proc;
    if (a_pe != b_pe) return a_pe < b_pe;

    // then compare chare array index
    return a.chare_idx < b.chare_idx;
  };

  bool PrefixLBCompareLDObjStats(const LDObjStats & a, const LDObjStats & b)
  {
    // first compare PE
    int a_pe = a.from_proc, b_pe = b.from_proc;
    if (a_pe != b_pe) return a_pe < b_pe;

    // then compare chare array index
    // to_proc is assigned with chare_idx from user data
    return a.to_proc < b.to_proc;
  };

  bool CentroidRecordCompare(const LBCommon::LBCentroidCompare & a, const LBCommon::LBCentroidCompare & b){
    return a.distance < b.distance;
  };

}

#endif //PARATREET_USER_LBCOMMON_H_

