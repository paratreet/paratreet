#ifndef SIMPLE_TREEELEMENTS_H_
#define SIMPLE_TREEELEMENTS_H_

#include "simple.decl.h"
#include "common.h"

class TreeElements : public CBase_TreeElements {
private:
  bool if_leaf;
  int wait_count = 0;
public:
  TreeElements() {
    wait_count = -1;
  }
  virtual void exist(bool if_leafi) {
    if (wait_count < 0) {
      if_leaf = if_leafi;
      wait_count = (if_leaf) ? 1 : 8;
    }
  }
  bool received() {
    wait_count--;
    return wait_count <= 0;
  }
  virtual void node(Key) {} // idealy pure virtual, but can't
  virtual void leaf() {} // maybe CProxy_TreePiece as parameter?
  virtual void hit() {} // book-keeping purposes
  virtual void miss() {}
};

#endif
