#include "simple_gravity.decl.h"

#include <vector>
#include <ostream>
#include <fstream>

#include "simple_gravity.h"

struct Main : CBase_Main {
    Main(CkArgMsg* m) {
        thisProxy.run();
    }

    void run() {
        pt::Tree<SGMeta> tree;

        tree.initialize("./data/1k.tipsy");

        CkStartQD(CkCallback(CkIndex_Main::exit(), thisProxy));
    }

    void exit() {
        CkExit();
    }
};


// - ParticleReader -----------------------------------------------------------
ParticleReader::ParticleReader(std::string filename): tipsy_reader(filename) {
    if (tipsy_reader.status()) {
        Tipsy::header tipsyHeader = tipsy_reader.getHeader();
        num = tipsyHeader.nbodies;
    }
}

int ParticleReader::error() { return !tipsy_reader.status(); }
int ParticleReader::seek(std::size_t i) { return !tipsy_reader.seekParticleNum(i); }
std::size_t ParticleReader::getNum() { return num; }

int ParticleReader::getNext(Particle& p) {
    Tipsy::simple_particle sp;
    if (tipsy_reader.getNextSimpleParticle(sp)) {
        p.m = sp.mass;
        p.v = {sp.vel.x, sp.vel.y, sp.vel.z};
        p.pos = {sp.pos.x, sp.pos.y, sp.pos.z};
        return 0;
    } else {
        return -1;
    }
}
// - ParticleReader END -------------------------------------------------------



#include "simple_gravity.def.h"