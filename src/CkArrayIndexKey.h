#ifndef _CKARRAYINDEXKEY_H_
#define _CKARRAYINDEXKEY_H_

#include "common.h"
#include "charm++.h"

class CkArrayIndexKey : public CkArrayIndex {
private:
  Key* key;
public:
  CkArrayIndexKey(const Key &in) {
    key = new (index) Key(in);
    nInts = sizeof(Key) / sizeof(int);
  }
};

#endif