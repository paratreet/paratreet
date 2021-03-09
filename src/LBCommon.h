#ifndef PARATREET_USER_LBCOMMON_H_
#define PARATREET_USER_LBCOMMON_H_
enum LBType {
  pt = 1111, // Partition
  cp = 2222 // TreeCanopy
};

struct LBUserData{
  int lb_type;
  int chare_idx;
  int particle_size;
  int particle_sum;

  void pup(PUP::er &p){
    p | lb_type;
    p | chare_idx;
    p | particle_size;
    p | particle_sum;
  }
};



//bool compareLDObjStats(const LDObjStats & a, const LDObjStats & b)
//{
//  // first compare PE
//  int a_pe = a.from_proc, b_pe = b.from_proc;
//  if (a_pe != b_pe) return a_pe < b_pe;
//
//  // then compare chare array index
//  // to_proc is assigned with chare_idx from user data
//  return a.to_proc < b.to_proc;
//};

#endif //PARATREET_USER_LBCOMMON_H_
