#ifndef PARATREET_TREESPEC_H_
#define PARATREET_TREESPEC_H_

#include "paratreet.decl.h"
#include "Node.h"

class TreeSpec : public CBase_TreeSpec {
public:
    TreeSpec(int tree_type_, int decomp_type_)
    : tree_type(tree_type_),
      decomp_type(decomp_type_),
      decomp(nullptr) { }

    void receiveDecomposition(CkMarshallMsg*);
    Decomposition* getDecomposition();

    template <typename Data>
    Node<Data>* makeCachedCanopy(Key key, typename Node<Data>::Type type, const Data& data, Node<Data>* parent) const {
      return new FullNode<Data, 8> (key, type, data, parent);
    }
    template <typename Data>
    Node<Data>* makeNode(Key key, int depth, int n_particles, Particle* particles, int owner_tp_start, int owner_tp_end, bool is_leaf, Node<Data>* parent, int tp_index) const {
      return new FullNode<Data, 8> (key, depth, n_particles, particles, owner_tp_start, owner_tp_end, is_leaf, parent, tp_index);
    }
    template <typename Data>
    Node<Data>* makeCachedNode(Key key, SpatialNode<Data> spatial_node, Node<Data>* parent, const Particle* particlesToCopy) {
      Particle* particles = nullptr;
      if (spatial_node.is_leaf && spatial_node.n_particles > 0) {
        particles = new Particle [spatial_node.n_particles];
        std::copy(particlesToCopy, particlesToCopy + spatial_node.n_particles, particles);
      }
      auto type = spatial_node.is_leaf ? Node<Data>::Type::CachedRemoteLeaf : Node<Data>::Type::CachedRemote;
      return new FullNode<Data, 8> (key, type, spatial_node.is_leaf, spatial_node, particles, parent);
    }

protected:
    int decomp_type, tree_type;
    std::unique_ptr<Decomposition> decomp;
};

#endif
