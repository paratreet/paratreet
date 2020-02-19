#ifndef PARATREET_H
#define PARATREET_H

#include "../impl/config.h"

namespace paratreet {

enum ImplType {
    KD,
    SFC,
    OCT
};

/*
concept Meta {
    static const ImplType IMPL;
    typename Element;
    typename Reader;
    static const unsigned D;

    typename NodeData;
};

concept Element {
    Vector<D> position();
};

concept NodeData {

};

concept Reader {
    ParticleReader(std::string filename);
    int         error();
    std::size_t getNum();
    int         seek(std::size_t i);
    int         getNext(Particle& p);
};
*/

template <class Meta, ImplType IMPL = Meta::IMPL>
class Tree;

} // paratreet

#include "../impl/shared/Vector.h"
#include "../impl/shared/Box.h"
#include "../impl/shared/log.h"
#include "../impl/shared/reducers.h"

#include "../impl/sfc/Tree.h"

#endif