#ifndef PARATREET_EXAMPLE_SIMPE_GRAVITY_H
#define PARATREET_EXAMPLE_SIMPE_GRAVITY_H

#include "paratreet.h"
#include "../../lib/cosmo_utility/structures/TipsyReader.h"

namespace pt = paratreet;

// 1. Element type
struct Particle {
    double m;
    pt::Vector<3> v;
    pt::Vector<3> pos;

    pt::Vector<3> position() const { return pos; }
};

// 2. Reader type
class ParticleReader {
public:
    ParticleReader(std::string filename);
    int         error();
    int         seek(std::size_t i);
    std::size_t getNum();
    int         getNext(Particle& p);
private:
    int num;
    Tipsy::TipsyReader tipsy_reader;
};

// 3. NodeData type
struct CentroidData {

};

// 4. Meta type (to group all configurations)
struct SGMeta {
    static const pt::ImplType IMPL = pt::SFC;
    typedef Particle Element;
    typedef ParticleReader Reader;
    typedef CentroidData NodeData;
};


template <class OutStream>
OutStream& operator<<(OutStream& os, const Particle& p) {
    return os << "{ m=" << p.m << ", v=" << p.v << ", pos=" << p.pos << " }";
}

#endif