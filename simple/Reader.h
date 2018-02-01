#ifndef SIMPLE_READER_H_
#define SIMPLE_READER_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"
#include "BoundingBox.h"
#include "Splitter.h"

class Reader : public CBase_Reader {
  BoundingBox box;
  std::vector<Particle> particles;
  std::vector<ParticleMsg*> particle_messages;
  int particle_index;

  public:
    std::vector<Splitter> splitters;

    Reader();

    // loading particles and assigning keys
    void load(std::string, const CkCallback&);
    void assignKeys(BoundingBox&, const CkCallback&);

    // OCT decomposition
    void countOct(std::vector<Key>&, const CkCallback&);
    void setSplitters(const std::vector<Splitter>&, const CkCallback&);

    // SFC decomposition
    //void countSfc(const std::vector<Key>&, const CkCallback&);
    void pickSamples(const int, const CkCallback&);
    void prepMessages(const std::vector<Key>&, const CkCallback&);
    void redistribute();
    void receive(ParticleMsg*);
    void localSort(const CkCallback&);
    void checkSort(const Key, const CkCallback&);

    // sending particles to home TreePieces
    void flush(CProxy_TreePiece);
};

#endif // SIMPLE_READER_H_
