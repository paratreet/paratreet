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

// Decomposition types
#define OCT_DECOMP 10
#define SFC_DECOMP 11

// Tree types
#define OCT_TREE 20
#define BINARY_OCT_TREE 21

// Hyperparameters
#define LOCAL_CACHE_SIZE 5000
#define DECOMP_TOLERANCE 1.0
#define BUCKET_TOLERANCE 1.0

// Particle key
typedef SFC::Key Key;
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif
#define KEY_BITS (sizeof(Key)*CHAR_BIT)
#define BITS_PER_DIM (KEY_BITS/NDIM)
#define BOXES_PER_DIM (1<<(BITS_PER_DIM))

#define UP_ONLY 7

#endif // PARATREET_COMMON_H_
