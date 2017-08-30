#ifndef SIMPLE_READER_H_
#define SIMPLE_READER_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"
#include "BoundingBox.h"

class Reader : public CBase_Reader {
  CkVec<Particle> particles;
  BoundingBox box;

  public:
    std::vector<Key> splitters;
    std::vector<int> splitter_counts;

    Reader();
    void load(std::string, const CkCallback&);
    void assignKeys(BoundingBox&, const CkCallback&);
    void count(std::vector<Key>&, const CkCallback&);
    void setSplitters(const std::vector<Key>&, const std::vector<int>&, const CkCallback&);
    const std::vector<Key>& getSplitters();
    void flush();
};

#endif // SIMPLE_READER_H_
