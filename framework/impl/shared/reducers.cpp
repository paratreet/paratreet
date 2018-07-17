#include "Box.h"
#include "reducers.h"

namespace paratreet {

CkReduction::reducerType Reducer::sum_size;
CkReduction::reducerType Reducer::sum_box3;

void register_reducers() {
    Reducer::sum_size = CkReduction::addReducer(sum_size, true);
    Reducer::sum_box3 = CkReduction::addReducer(sum_box3, true);
}

CkReductionMsg* sum_size(int nMsg, CkReductionMsg **msgs) {
    std::size_t* data = static_cast<std::size_t*>(msgs[0]->getData());
    for (int i = 1; i < nMsg; i++) {
        CkAssert(msgs[i]->getSize() == sizeof(std::size_t));
        *data += *static_cast<std::size_t*>(msgs[i]->getData());
    }
    return CkReductionMsg::buildNew(sizeof(std::size_t), data, Reducer::sum_size, msgs[0]);
}

CkReductionMsg* sum_box3(int nMsg, CkReductionMsg **msgs) {
    Box<3>* data = static_cast<Box<3>*>(msgs[0]->getData());
    for (int i = 1; i < nMsg; i++) {
        CkAssert(msgs[i]->getSize() == sizeof(Box<3>));
        *data += *static_cast<Box<3>*>(msgs[i]->getData());
    }
    return CkReductionMsg::buildNew(sizeof(Box<3>), data, Reducer::sum_box3, msgs[0]);
}

} // paratreet