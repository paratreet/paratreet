#ifndef SIMPLE_READER_H_
#define SIMPLE_READER_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"
#include "BoundingBox.h"
#include "Splitter.h"

class Reader : public CBase_Reader {
  CkVec<Particle> particles;
  BoundingBox box;
  CkVec<Splitter> splitters;

  public:
    Reader();

    void load(std::string, const CkCallback&);
    void assignKeys(BoundingBox&, const CkCallback&);
    void count(CkVec<Key>&, const CkCallback&);
    void setSplitters(CkVec<Splitter>&, const CkCallback&);
    const CkVec<Splitter>& getSplitters();
    void flush();
};

#endif // SIMPLE_READER_H_
