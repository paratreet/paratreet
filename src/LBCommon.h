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

  struct DorbPartitionRec{
    int dim;
    float load;
    int n_objs;
    int left; //pe index
    int right;
    double low;
    double high;
    float granularity;
    Vector3D<Real> lower_coords;
    Vector3D<Real> upper_coords;
    void pup(PUP::er &p){
      p | dim;
      p | load;
      p | n_objs;
      p | left;
      p | right;
      p | low;
      p | high;
      p | granularity;
      p | lower_coords;
      p | upper_coords;
    };
  };

  struct LBObjToken{
    Vector3D<Real> centroid;
    int idx;
    int from_pe;
    int to_pe;
    float load;
    float distance;
    void pup(PUP::er &p){
      p | centroid;
      p | idx;
      p | from_pe;
      p | to_pe;
      p | load;
      p | distance;
    };
  };

  struct LBGroup{
    Vector3D<Real> centroid;
    int id;
    int size;
    float load;
    float distance;
    float load_per_pe;
    void pup(PUP::er &p){
      p | centroid;
      p | id;
      p | size;
      p | load;
      p | distance;
      p | load_per_pe;
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

  struct LBCentroidAndIndexRecord{
    Vector3D<Real> centroid;
    int idx;
    int particle_size;
    int from_pe;
    float load;
    float distance;
    int to_pe;
    LDObjData obj_data;
    // inline void pup(PUP::er &p);
    void pup(PUP::er &p){
      p | centroid;
      p | idx;
      p | particle_size;
      p | from_pe;
      p | load;
      p | distance;
      p | to_pe;
      p | obj_data;
    };
  };

  struct LBCentroidCompare{
    Real distance;
    int from_pe;
    inline void pup(PUP::er &p);
  };

  struct LBShortCmp{
    float coord;
    float load;
    void pup(PUP::er &p){
      p | coord;
      p | load;
    }
  };
};

namespace{
  bool CompareLBShortCmp(const LBCommon::LBShortCmp & a, const LBCommon::LBShortCmp & b){
    return a.coord < b.coord;
  }

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

  // largest distance at front
  bool CentroidRecordCompare(const LBCommon::LBCentroidCompare & a, const LBCommon::LBCentroidCompare & b){
    return a.distance > b.distance;
  };

  bool DiffusionCompareCentroidDistance(const LBCommon::LBCentroidAndIndexRecord & a, const LBCommon::LBCentroidAndIndexRecord & b){
    return a.distance < b.distance;
  }

  bool DiffusionCompareGroupDistance(const LBCommon::LBGroup & a, const LBCommon::LBGroup & b){
    return a.distance < b.distance;
  }

  bool CompareTokenDistance(const LBCommon::LBObjToken & a, const LBCommon::LBObjToken & b){
    return a.distance > b.distance;
  }

  bool DiffusionCompareGroupLoads(const LBCommon::LBGroup & a, const LBCommon::LBGroup & b){
    return a.load_per_pe < b.load_per_pe;
  }

  struct NdComparator{
    int dim = -1;
    NdComparator(int dim_){
      dim = dim_;
    };

    bool operator() (LBCommon::LBCentroidAndIndexRecord a, LBCommon::LBCentroidAndIndexRecord b){
      return a.centroid[dim] < b.centroid[dim];
    };
  };

}

#endif //PARATREET_USER_LBCOMMON_H_

