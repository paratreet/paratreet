#ifndef SIMPLE_READER_H_
#define SIMPLE_READER_H_

#include "simple.decl.h"
#include "common.h"
#include "Particle.h"

class Reader : public CBase_Reader {
  std::vector<Particle> particles;

  public:
    Reader();

    void load(std::string);
};

#endif // SIMPLE_READER_H_
