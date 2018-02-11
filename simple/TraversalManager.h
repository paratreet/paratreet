#ifndef SIMPLE_TRAVERSALMANAGER_H_
#define SIMPLE_TRAVERSALMANAGER_H_

#include "simple.decl.h"
#include "common.h"

class TraversalManager : public CBase_TraversalManager {
public:
  TraversalManager() {}
  void getNext(CProxy_TreeElements, Key, int);
};

#endif
