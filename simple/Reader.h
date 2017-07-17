#ifndef SIMPLE_READER_H_
#define SIMPLE_READER_H_

#include "simple.decl.h"
#include "common.h"

class Reader : public CBase_Reader {
  public:
    Reader();

    void load(std::string input_file);
};

#endif // SIMPLE_READER_H_
