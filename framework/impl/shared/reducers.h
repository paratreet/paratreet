#ifndef PARATREET_SHARED_REDUCERS_H
#define PARATREET_SHARED_REDUCERS_H

#include "charm++.h"

#include <cstddef>

namespace paratreet {

struct Reducer {
    static CkReduction::reducerType sum_size;
    static CkReduction::reducerType sum_box3;
};

CkReductionMsg* sum_size(int nMsg, CkReductionMsg **msgs);
CkReductionMsg* sum_box3(int nMsg, CkReductionMsg **msgs);

void register_reducers();

} // paratreet

#endif