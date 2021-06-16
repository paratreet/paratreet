#ifndef PARATREET_CORE_FUNCTIONS_H_
#define PARATREET_CORE_FUNCTIONS_H_

#include "common.h"

class BoundingBox;

template<typename T>
class Partition;

template<typename T>
class ProxyPack;

template<typename T>
class SpatialNode;

namespace paratreet {
    inline Real getTimestep(BoundingBox& box, Real real);

    template<typename T>
    inline void preTraversalFn(ProxyPack<T>& pack);

    template<typename T>
    inline void traversalFn(BoundingBox& box, ProxyPack<T>& pack, int iter);

    template<typename T>
    inline void postIterationFn(BoundingBox& box, ProxyPack<T>& pack, int iter);

    template<typename T>
    inline void perLeafFn(int indicator, SpatialNode<T>& node, Partition<T>* partition);
}

#endif
