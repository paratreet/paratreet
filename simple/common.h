#ifndef SIMPLE_COMMON_H_
#define SIMPLE_COMMON_H_

#include <string>
#include <vector>
#include "Vector3D.h"
#include "SFC.h"

/* Floating point type */
#ifndef USE_DOUBLE_FP
typedef float Real;
#define REAL_MAX FLT_MAX
#else
typedef double Real;
#define REAL_MAX DBL_MAX
#endif

/* Tree types */
#define OCT_TREE 10
#define SFC_TREE 11

#define NDIM 3

#define BRANCH_FACTOR 2
#define LOG_BRANCH_FACTOR 1 // binary tree

typedef SFC::Key Key;
#ifndef CHAR_BIT
#define CHAR_BIT 8
#endif
#define KEY_BITS (sizeof(Key)*CHAR_BIT)
#define BITS_PER_DIM (KEY_BITS/NDIM)
#define BOXES_PER_DIM (1<<(BITS_PER_DIM))

#define DECOMP_TOLERANCE 1.0
#define BUCKET_TOLERANCE 1.0

#endif // SIMPLE_COMMON_H_
