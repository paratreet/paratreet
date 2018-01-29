#ifndef SIMPLE_READER_H_
#define SIMPLE_READER_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"
#include "BoundingBox.h"
#include "Splitter.h"

class Reader : public CBase_Reader {
  public:
    CkVec<Particle> particles;
    BoundingBox box;
    CkVec<Splitter> splitters;

    std::vector<ParticleMsg*> particle_messages;

    //std::vector<Key> tp_keys;
    //std::vector<Key> ksplitters;
    //std::vector<int> splitter_counts;

    Reader();
    void load(std::string, const CkCallback&);
    void assignKeys(BoundingBox&, const CkCallback&);
    void countOct(std::vector<Key>&, const CkCallback&);
    void countSfc(const std::vector<Key>&, const CkCallback&);
    void pickSamples(const int, const CkCallback&);
    void prepMessages(const std::vector<Key>&, const CkCallback&);
    void redistribute();
    void receiveMessage(ParticleMsg* msg);
    void setSplitters(const CkVec<Splitter>&, const CkCallback&);
    //void flush(CProxy_TreePiece);
};

#endif // SIMPLE_READER_H_
