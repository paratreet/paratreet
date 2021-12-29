#ifndef PARATREET_NODE_H_
#define PARATREET_NODE_H_ 
#include "common.h"
#include "Particle.h"
#include <array>
#include <atomic>

template <typename Data>
class SpatialNode
{
public:
  SpatialNode() = default;
  SpatialNode(Particle* _particles, int _n_particles, int _depth)
  : data(_particles, _n_particles, _depth),
    n_particles(_n_particles), depth(_depth), particles_(_particles)
  {
  }
  SpatialNode(int _depth, int _n_particles) : data(), n_particles(_n_particles), depth(_depth), particles_(nullptr)
  {
  }
  SpatialNode(const SpatialNode<Data>& other, Particle* _particles)
    : data(other.data), n_particles(other.n_particles), depth(other.depth), particles_(_particles)
  {
  }
  virtual ~SpatialNode() = default;

  void changeParticle(int index, const Particle& part) {
    particles_[index] = part;
  }
  void applyAcceleration(int index, Vector3D<Real> accel) {
    particles_[index].acceleration += accel;
  }
  void applyGasWork(int index, Real work) {
    particles_[index].pressure_dVolume += work;
  }
  void applyPotential(int index, Real pot) {
    particles_[index].potential += pot;
  }

  void pup (PUP::er& p) {
    p | depth;
    p | data;
    p | n_particles;
  }

public:
  Data      data;
  int       n_particles = -1; // non-leaves will have this as -1
  int       depth = -1;
  inline const Particle* particles() const {return particles_;}

private:
  Particle* particles_ = nullptr;

public:
  void freeParticles() {
    if (n_particles > 0 && particles_) {
      delete[] particles_;
      particles_ = nullptr;
    }
  }
  void kick(Real timestep) {
    for (int i = 0; i < n_particles; i++) {
      particles_[i].kick(timestep);
    }
  }
  void perturb(Real timestep) {
    for (int i = 0; i < n_particles; i++) {
      particles_[i].perturb(timestep);
    }
  }
};

template <typename Data>
class Node : public SpatialNode<Data>
{
public:
  virtual Node* getChild(int child_idx) const = 0;
  virtual Node* exchangeChild(int child_idx, Node* child) = 0;
  virtual Node<Data>* getDescendant(Key to_find) = 0;

  enum class Type {
    Invalid = 0,
    Leaf,
    EmptyLeaf,
    RemoteLeaf,
    RemoteEmptyLeaf,
    Remote,
    Internal,
    Boundary,
    RemoteAboveTPKey,
    CachedRemote,
    CachedRemoteLeaf,
    CachedBoundary
  };

  Node(int _depth, int _n_children, Node* _parent, Type _type, Key _key, int _tp_index, int _cm_index)
    : SpatialNode<Data>(_depth, (_n_children > 0) ? -1 : 0),
      n_children(_n_children),
      parent(_parent),
      type(_type),
      key(_key),
      wait_count(_n_children),
      tp_index(_tp_index),
      cm_index(_cm_index)
  {
  }

  Node(int _n_particles, Particle* _particles, int _depth,
       Node* _parent, Type _type, Key _key,
        int _tp_index, int _cm_index)
    : SpatialNode<Data>(_particles, _n_particles, _depth),
      n_children(0),
      parent(_parent),
      type(_type),
      key(_key),
      wait_count(0),
      tp_index(_tp_index),
      cm_index(_cm_index)
  {
  }


  Node(Key _key, typename Node<Data>::Type _type, int _n_children,
        const SpatialNode<Data>& _spatial_node, Particle* _particles,
        Node<Data>* _parent, int _tp_index, int _cm_index)
    : SpatialNode<Data>(_spatial_node, _particles),
      n_children(_n_children),
      parent(_parent),
      type(_type),
      key(_key),
      wait_count(-1),
      tp_index(_tp_index),
      cm_index(_cm_index)
  {
  }

  virtual ~Node() {
    if (type == Type::CachedRemoteLeaf) {
      this->freeParticles();
    }
  }

public:
  const int n_children;
  Node* parent = nullptr; // CacheManager's insertNode  prevents the constness
  const Type type;
  const Key key;

  // this stuff gets edited:
  int wait_count = -1;
  const int tp_index;
  const int cm_index;
  std::atomic<unsigned long long> requested = ATOMIC_VAR_INIT(0ull);
  // functions either as a boolean or as an indicator
  // as to whether it's requested on that pe

public:
  bool isCached() const {
    return type == Type::CachedRemote
        || type == Type::CachedBoundary
        || type == Type::CachedRemoteLeaf;
  }
  bool isLeaf() const {
    return type == Type::Leaf
        || type == Type::EmptyLeaf
        || type == Type::CachedRemoteLeaf;
  }

  void triggerFree() {
    if (type == Type::Internal || type == Type::Boundary ||
        type == Type::CachedRemote || type == Type::CachedBoundary)
    {
      for (int i = 0; i < n_children; i++) {
        auto child = getChild(i);
        if (child == nullptr) continue;
        child->triggerFree();
        delete child;
        exchangeChild(i, nullptr);
      }
    }
  }

  static std::string TypeDotColor(Type type){
    switch(type){
      case Type::Invalid:               return "firebrick1";
      case Type::Internal:              return "darkolivegreen1";
      case Type::Leaf:                  return "darkolivegreen3";
      case Type::EmptyLeaf:             return "darksalmon";
      case Type::Boundary:              return "darkkhaki";
      case Type::Remote:                return "deepskyblue1";
      case Type::RemoteLeaf:            return "dodgerblue4";
      case Type::RemoteEmptyLeaf:       return "deeppink";
      default:                          return "black";
    }
  }

  void dot(std::ostream& out) const {
    out << key << " [";

    out << "label=\"";
    out << key << ", ";
    out << this->n_particles << ", ";
    //out << "\\n" << payload_;
    //out << "\\n" << tp_;
    out << "\",";

    out << "color=\"" << TypeDotColor(type) << "\", ";
    out << "style=\"filled\"";

    out << "];" << std::endl;

    if (type == Type::Leaf || type == Type::EmptyLeaf || type == Type::Internal)
      return;

    for (int i = 0; i < n_children; i++) {
      auto child = getChild(i);
      out << key << " -> " << child->key << ";" << std::endl;
      child->dot(out);
    }
  }
};

template <class Data, size_t BRANCH_FACTOR>
class FullNode : public Node<Data>
{
public:
  FullNode() = default;
  virtual ~FullNode() = default;

  FullNode(Key _key, typename Node<Data>::Type _type, const SpatialNode<Data>& _spatial_node, Particle* _particles, Node<Data>* _parent, int _tp_index, int _cm_index) // for cached non boundary nodes
  : Node<Data>(_key, _type, (_spatial_node.n_particles >= 0) ? 0 : BRANCH_FACTOR, _spatial_node, _particles, _parent, _tp_index, _cm_index)
  {
    initChildren();
  }

  FullNode(Key _key, typename Node<Data>::Type _type, int _depth, int _n_particles, Particle* _particles, Node<Data>* _parent, int _tp_index, int _cm_index)
    : Node<Data>(_n_particles, _particles, _depth, _parent, _type, _key, _tp_index, _cm_index)
  {
  }

  FullNode(Key _key, typename Node<Data>::Type _type, int _depth, Node<Data>* _parent, int _tp_index, int _cm_index)
    : Node<Data>(_depth, (_type == Node<Data>::Type::EmptyLeaf) ? 0 : BRANCH_FACTOR, _parent, _type, _key, _tp_index, _cm_index)
  {
    initChildren();
  }

  void initChildren() {
    for (auto && child : children) child.store(nullptr);
  }
 
  virtual Node<Data>* getChild(int child_idx) const override {
    CkAssert(child_idx < this->n_children);
    return children[child_idx].load(std::memory_order_relaxed);
  }
  virtual Node<Data>* exchangeChild(int child_idx, Node<Data>* child) override {
    CkAssert(child_idx < this->n_children);
    return children[child_idx].exchange(child, std::memory_order_relaxed);
  }
  virtual Node<Data>* getDescendant(Key to_find) override {
    std::vector<int> remainders;
    Key temp = to_find;
    while (temp >= BRANCH_FACTOR * this->key) {
      remainders.push_back(temp % BRANCH_FACTOR);
      temp /= BRANCH_FACTOR;
    }
    Node<Data>* node = this;
    for (int i = remainders.size()-1; i >= 0; i--) {
      if (node && remainders[i] < node->n_children) node = node->getChild(remainders[i]);
      else return nullptr;
    }
    return node;
  }


private:
  std::array<std::atomic<Node<Data>*>, BRANCH_FACTOR> children; 
};

#endif // PARATREET_NODE_H_
