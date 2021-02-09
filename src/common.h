#ifndef PARATREET_COMMON_H_
#define PARATREET_COMMON_H_

#include <string>
#include <cinttypes> // For printing keys in hex
#include "Vector3D.h"
#include "SFC.h"

// Floating point type
#ifndef USE_DOUBLE_FP
typedef float Real;
#define REAL_MAX FLT_MAX
#else
typedef double Real;
#define REAL_MAX DBL_MAX
#endif

// Simulation domain dimensions
#define NDIM 3

// Particle key
typedef SFC::Key Key;
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif
#define KEY_BITS (sizeof(Key)*CHAR_BIT)
#define BITS_PER_DIM (KEY_BITS/NDIM)
#define BOXES_PER_DIM (1<<(BITS_PER_DIM))

template<typename T>
void* ckNodeAwareLocal(T& thisProxy, int idx_) {
  auto idx = thisProxy[idx_].ckGetIndex();
  int lastPe = thisProxy.ckLocalBranch()->lastKnown(idx);
  if (CkMyNode() == CkNodeOf(lastPe)) {
    return thisProxy.ckLocalBranchOther(CkRankOf(lastPe))->lookup(idx);
  } else {
    return nullptr;
  }
}

#endif // PARATREET_COMMON_H_
