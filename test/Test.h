#ifndef SIMPLE_TEST_H_
#define SIMPLE_TEST_H_

#include "simple.decl.h"
#include "common.h"
#include "Node.h"

extern CProxy_Main mainProxy;

class Test : public CBase_Test {
  public:
  Test() {}

  void receive(std::vector<Node>& new_nodes) {
    for (int i = 0; i < new_nodes.size(); i++) {
      for (int j = 0; j < new_nodes[i].n_particles; j++) {
      CkPrintf("[PE %d, Test %d, Node %d] particle %d key: 0x%" PRIx64 "\n", CkMyPe(), thisIndex, i, j, new_nodes[i].particles[j].key);
      }
    }

    CkCallback cb(CkReductionTarget(Main, terminate), mainProxy);
    contribute(cb);
  }
};

#endif // SIMPLE_TEST_H_
