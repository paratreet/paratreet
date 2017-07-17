#ifndef TAGGED_VECTOR3D_H_
#define TAGGED_VECTOR3D_H_
#include "charm++.h"
#include "lbdb.h"
#include "Vector3D.h"
#include "pup.h"
#include "pup_stl.h"

class TaggedVector3D{
  public: 
  Vector3D<float> vec;
  LDObjHandle handle;
  unsigned int myNumParticles, numActiveParticles;
  unsigned int activeRung;
  int prevActiveRung;
  int tp;
  int tag;

  TaggedVector3D(Vector3D<float> &v, LDObjHandle &i) : vec(v), handle(i){}
  TaggedVector3D(Vector3D<float> &v, LDObjHandle &i, unsigned int numAP, unsigned int myNP, unsigned int aR, unsigned int pAR) : vec(v), handle(i), numActiveParticles(numAP), myNumParticles(myNP), activeRung(aR), prevActiveRung(pAR){}
  TaggedVector3D() :numActiveParticles(0), myNumParticles(0), activeRung(0), prevActiveRung(0){}
  void pup(PUP::er &p){
    p|vec;
    p|handle;
    p|myNumParticles;
    p|numActiveParticles;
    p|activeRung;
    p|prevActiveRung;
    p|tp;
    p|tag;
  }
};
#endif // TAGGED_VECTOR3D_H_
