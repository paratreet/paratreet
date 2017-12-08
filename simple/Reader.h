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
    /*
    std::vector<Key> tp_keys;
    std::vector<Key> splitters;
    std::vector<int> splitter_counts;
    */

    Reader();
    void load(std::string, const CkCallback&);
    void assignKeys(BoundingBox&, const CkCallback&);
    void count(CkVec<Key>&, const CkCallback&);
    void setSplitters(CkVec<Splitter>&, const CkCallback&);
    void flush(CProxy_TreePiece);
    //void setSplitters(const std::vector<Key>&, const CkCallback&);
    //void setSplitters(const std::vector<Key>&, const std::vector<int>&, const CkCallback&);
};

#endif // SIMPLE_READER_H_
