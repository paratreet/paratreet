#ifndef PARATREET_CORE_FUNCTIONS_H_
#define PARATREET_CORE_FUNCTIONS_H_

#include "common.h"

template<typename T>
class Partition;

template<typename T>
class ProxyPack;

template<typename T>
class SpatialNode;

namespace paratreet {
    template <typename T>
    inline Real getTimestep(const typename T::BoundingBox& box, Real max_velocity);

    template<typename T>
    inline void preTraversalFn(ProxyPack<T>& pack);

    template<typename T>
    inline void traversalFn(const typename T::BoundingBox&, ProxyPack<T>& pack, int iter);

    template<typename T>
    inline void postIterationFn(const typename T::BoundingBox&, ProxyPack<T>& pack, int iter);

    template<typename T>
    class PerLeafAble: public PUP::able {
      public:  
        PerLeafAble(void) = default;
        PerLeafAble(CkMigrateMessage *m): PUP::able(m) {}

        virtual void pup(PUP::er &p) override { PUP::able::pup(p); }

        virtual void operator()(SpatialNode<T>& node, Partition<T>* partition) = 0;
    };
}

#endif
