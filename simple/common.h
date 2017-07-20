#ifndef SIMPLE_COMMON_H_
#define SIMPLE_COMMON_H_

#include <string>
#include "Vector3D.h"

#ifndef USE_DOUBLE_FP
typedef float Real;
#define REAL_MAX FLT_MAX
#else
typedef double Real;
#define REAL_MAX DBL_MAX
#endif

#define NDIM 3

typedef uint64_t Key;
#define KEY_BITS (sizeof(Key)*CHAR_BIT)
#define BITS_PER_DIM (KEY_BITS/NDIM)
#define BOXES_PER_DIM (1<<(BITS_PER_DIM))

#endif // SIMPLE_COMMON_H_
