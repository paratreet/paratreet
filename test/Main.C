#include "simple.decl.h"
#include "common.h"
#include "Test.h"

/* readonly */ CProxy_Main mainProxy;
/* readonly */ CProxy_Test testProxy;

class Main : public CBase_Main {
  public:
  Main(CkArgMsg* m) {
    int num_test = CkNumPes() * 2;
    int nodes_per_test = 2;
    int particles_per_node = 4;

    mainProxy = thisProxy;
    testProxy = CProxy_Test::ckNew(num_test);

    for (int i = 0; i < num_test; i++) {
      std::vector<Node> send_nodes;
      for (int j = 0; j < nodes_per_test; j++) {
        Node node(i, i, NULL, 0, 0, num_test-1, NULL);
        std::vector<Particle> particles;
        for (int k = 0; k < particles_per_node; k++) {
          Particle particle;
          particle.key = 0xFFF0 + k;
          particles.push_back(particle);
        }
        node.particles = &particles[0];
        node.n_particles = particles.size();

        send_nodes.push_back(node);
      }
      testProxy[i].receive(send_nodes);
    }
  }

  void terminate() {
    CkPrintf("Terminating...\n");
    CkExit();
  }
};

#include "simple.def.h"
