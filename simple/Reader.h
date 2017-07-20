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
    Reader();

    void load(std::string, const CkCallback&);
    void assignKeys(BoundingBox&, const CkCallback&);
};

#endif // SIMPLE_READER_H_
