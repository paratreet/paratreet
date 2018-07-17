#ifndef PARATREET_SFC_H
#define PARATREET_SFC_H

#include "sfc.h"
#include "DataManager.h"
#include "TreePiece.h"

#include "../shared/log.h"

#include <string>

namespace paratreet {

template <class Meta>
class Tree<Meta, SFC> {
public:
    Tree() {}

    void initialize(std::string filename);

    template <template <class Node> class Visitor>
    void traverse() {}
};


template <class Meta>
void Tree<Meta, SFC>::initialize(std::string filename) {
    using namespace sfc;
    CkReductionMsg* m;

    CProxy_DataManager<Meta> dataManager = CProxy_DataManager<Meta>::ckNew();
    dataManager.load(filename, CkCallbackResumeThread());

    dataManager.countElements(CkCallbackResumeThread((void*&)m));
    log::info("Elements count: ", *static_cast<std::size_t*>(m->getData()));
    delete m;

    dataManager.computeBoundingBox(CkCallbackResumeThread((void*&)m));
    log::info("Bouding box: ", *static_cast<Box<3>*>(m->getData()));
    delete m;
}

} // paratreet


#endif
