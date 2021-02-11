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
  SpatialNode(const Data& _data, int _n_particles, bool _is_leaf, Particle* _particles, int _depth)
    : data(_data), n_particles(_n_particles), is_leaf(_is_leaf), particles_(_particles), depth(_depth), home_pe(CkMyPe())
  {
  }
  SpatialNode(const SpatialNode<Data>& other, Particle* _particles)
    : data(other.data), n_particles(other.n_particles), is_leaf(other.is_leaf), particles_(_particles), depth(other.depth), home_pe(other.home_pe)
  {
  }
  virtual ~SpatialNode() = default;

  void applyAcceleration(int index, Vector3D<Real> accel) {
    particles_[index].acceleration += accel;
  }
  void applyGasWork(int index, Real work) {
    particles_[index].pressure_dVolume += work;
  }
  void setDensity(int index, Real density) { // these are NOT general enough!
    particles_[index].density = density;
  }
  void setAcceleration(int index, Vector3D<Real> accel) {
    particles_[index].acceleration = accel;
  }
  void setGasWork(int index, Real work) {
    particles_[index].pressure_dVolume = work;
  }
  void freeParticles() {
    if (n_particles > 0) {
      delete[] particles_;
    }
  }
  void pup (PUP::er& p) {
    p | depth;
    p | data;
    p | n_particles;
    p | is_leaf;
    p | home_pe;
    if (p.isUnpacking()) {
      particles_ = nullptr;
    }
  }

public:
  Data      data;
  int       n_particles = 0;
  bool      is_leaf     = false;
  int       depth       = 0;
  int       home_pe     = -1;
  inline const Particle* particles() const {return particles_;}

private:
    Particle* particles_ = nullptr;

};

template <typename Data>
class Node : public SpatialNode<Data>
{
public:
  virtual Node* getChild(int child_idx) const = 0;
  virtual Node* exchangeChild(int child_idx, Node* child) = 0;
  virtual size_t getBranchFactor() const = 0;

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

  Node(const Data& data, int _n_particles, Particle* _particles, int _depth,
        int _n_children, Node* _parent, Type _type, Key _key,
        int _owner_tp_start, int _owner_tp_end, int _tp_index, int _cm_index)
    : SpatialNode<Data>(data, _n_particles, _n_children == 0, _particles, _depth),
      n_children(_n_children),
      parent(_parent),
      type(_type),
      key(_key),
      owner_tp_start(_owner_tp_start),
      owner_tp_end(_owner_tp_end),
      wait_count(_n_children),
      tp_index(_tp_index),
      cm_index(_cm_index)
  {
  }

  Node(Key _key, typename Node<Data>::Type _type, int _n_children, const SpatialNode<Data>& _spatial_node, Particle* _particles, Node<Data>* _parent)
    : SpatialNode<Data>(_spatial_node, _particles),
      n_children(_n_children),
      parent(_parent),
      type(_type),
      key(_key)
  {
  }

  virtual ~Node() {
    if (type == Type::CachedRemoteLeaf) {
      this->freeParticles();
    }
  }

public:
  int n_children; // Subtree's recursiveBuild prevents the constness
  Node* parent;   // CacheManager's insertNode  prevents the constness
  Type type;      // Subtree's recursiveBuild prevents the constness
  const Key key;

  // this stuff gets edited:
  int owner_tp_start = -1;
  int owner_tp_end   = -1;
  int wait_count     = -1;
  int tp_index       = -1;
  int cm_index       = -1;
  std::atomic<bool> requested = ATOMIC_VAR_INIT(false);
  std::atomic<size_t> num_buckets_finished = ATOMIC_VAR_INIT(0);

public:
  Node<Data>* getDescendant(Key to_find) {
    std::vector<int> remainders;
    Key temp = to_find;
    auto branch_factor = getBranchFactor();
    while (temp >= branch_factor * this->key) {
      remainders.push_back(temp % branch_factor);
      temp /= branch_factor;
    }
    Node<Data>* node = this;
    for (int i = remainders.size()-1; i >= 0; i--) {
      if (node && remainders[i] < node->n_children) node = node->getChild(remainders[i]);
      else return nullptr;
    }
    return node;
  }
 
  void finish(size_t num_buckets) {
    num_buckets_finished += num_buckets;
  }

  bool isCached() const {
    return type == Type::CachedRemote
        || type == Type::CachedBoundary
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
    out << "[" << owner_tp_start << ", " << owner_tp_end << "]";
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

  FullNode(Key _key, typename Node<Data>::Type _type, bool _is_leaf, const SpatialNode<Data>& _spatial_node, Particle* _particles, Node<Data>* _parent) // for cached non boundary nodes
  : Node<Data>(_key, _type, _is_leaf ? 0 : BRANCH_FACTOR, _spatial_node, _particles, _parent)
  {
    initChildren();
  }

  FullNode(Key _key, int _depth, int _n_particles, Particle* _particles, int _owner_tp_start, int _owner_tp_end, bool _is_leaf, Node<Data>* _parent, int _tp_index)
    : Node<Data>(Data(), _n_particles, _particles, _depth, _is_leaf ? 0 : BRANCH_FACTOR, _parent, Node<Data>::Type::Invalid, _key, _owner_tp_start, _owner_tp_end, _tp_index, -1)
  {
    initChildren();
  }

  void initChildren() {
    for (auto && child : children) child.store(nullptr);
  }
  virtual ~FullNode() = default;
 
  virtual Node<Data>* getChild(int child_idx) const override {
    CkAssert(child_idx < this->n_children);
    return children[child_idx].load();
  }
  virtual Node<Data>* exchangeChild(int child_idx, Node<Data>* child) override {
    CkAssert(child_idx < this->n_children);
    return children[child_idx].exchange(child);
  }
  virtual size_t getBranchFactor() const override {
    return BRANCH_FACTOR;
  }

private:
  std::array<std::atomic<Node<Data>*>, BRANCH_FACTOR> children; 
};

#endif // PARATREET_NODE_H_
