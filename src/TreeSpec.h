#ifndef PARATREET_TREESPEC_H_
#define PARATREET_TREESPEC_H_

#include "paratreet.decl.h"
#include "Node.h"

class TreeSpec : public CBase_TreeSpec {
public:
    TreeSpec(const paratreet::Configuration& config_)
    : config(config_),
      decomp(nullptr) { }

    void receiveDecomposition(CkMarshallMsg*);
    Decomposition* getDecomposition();

    void receiveConfiguration(const paratreet::Configuration&,CkCallback);
    paratreet::Configuration& getConfiguration();
    void setConfiguration(const paratreet::Configuration& config_) { config = config_; }

    template <typename Data>
    Node<Data>* makeNode(Key key, int depth, int n_particles, Particle* particles, int owner_tp_start, int owner_tp_end, bool is_leaf, Node<Data>* parent, int tp_index) const {
      return new FullNode<Data, 8> (key, depth, n_particles, particles, owner_tp_start, owner_tp_end, is_leaf, parent, tp_index);
    }
    template <typename Data>
    Node<Data>* makeCachedNode(Key key, typename Node<Data>::Type type, SpatialNode<Data> spatial_node, Node<Data>* parent, const Particle* particlesToCopy) {
      Particle* particles = nullptr;
      if (spatial_node.is_leaf && spatial_node.n_particles > 0) {
        particles = new Particle [spatial_node.n_particles];
        std::copy(particlesToCopy, particlesToCopy + spatial_node.n_particles, particles);
      }
      return new FullNode<Data, 8> (key, type, spatial_node.is_leaf, spatial_node, particles, parent);
    }

protected:
    std::unique_ptr<Decomposition> decomp;
    paratreet::Configuration config;
};

#endif
