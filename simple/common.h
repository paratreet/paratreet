#ifndef SIMPLE_COMMON_H_
#define SIMPLE_COMMON_H_

#include <string>
#include <vector>
#include "Vector3D.h"

#ifndef USE_DOUBLE_FP
typedef float Real;
#define REAL_MAX FLT_MAX
#else
typedef double Real;
#define REAL_MAX DBL_MAX
#endif
typedef uint64_t Key;

#endif // SIMPLE_COMMON_H_
