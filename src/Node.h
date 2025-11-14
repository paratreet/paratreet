#ifndef PARATREET_NODE_H_
#define PARATREET_NODE_H_ 
#include "common.h"
#include "Particle.h"
#include <array>
#include <atomic>
#include <climits>

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
    p | particle_min_index;
    p | particle_max_index;
  }

public:
  Data      data;
  int       n_particles = -1; // non-leaves will have this as -1
  int       depth = -1;
  uint64_t  particle_min_index = UINT64_MAX;
  uint64_t  particle_max_index = 0;
  bool      vertex_range_initialized = false;
  // Track particle order ranges for FoF optimization
  int       particle_min_order = INT_MAX;
  int       particle_max_order = INT_MIN;
  bool      order_range_initialized = false;
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
  /*void kick(Real timestep) {
    for (int i = 0; i < n_particles; i++) {
      particles_[i].kick(timestep);
    }
  }*/
  /*void perturb(Real timestep) {
    for (int i = 0; i < n_particles; i++) {
      particles_[i].perturb(timestep);
    }
  }*/
  // Sets fields used in Friends-of-Friends (FoF) clustering
  void setParticleGroupNumber(int i, long group_number) {
    particles_[i].group_number = group_number;
  }
  void setParticleVertexID(int i, uint64_t vertex_id) {
    particles_[i].vertex_id = vertex_id;
    
    // Update vertex ID range
    if (!vertex_range_initialized) {
      // First particle - initialize both min and max to this vertex ID
      particle_min_index = vertex_id;
      particle_max_index = vertex_id;
      vertex_range_initialized = true;
    } else {
      // Update range based on this vertex ID
      if (vertex_id < particle_min_index) {
        particle_min_index = vertex_id;
      }
      if (vertex_id > particle_max_index) {
        particle_max_index = vertex_id;
      }
    }
    
    // Update order range using existing particle order
    int particle_order = particles_[i].order;
    if (!order_range_initialized) {
      // First particle - initialize both min and max to this order
      particle_min_order = particle_order;
      particle_max_order = particle_order;
      order_range_initialized = true;
    } else {
      // Update range based on this particle order
      if (particle_order < particle_min_order) {
        particle_min_order = particle_order;
      }
      if (particle_order > particle_max_order) {
        particle_max_order = particle_order;
      }
    }
  }
  
  // Update this node's min/max vertex IDs based on a child's range
  void updateVertexIDRange(uint64_t child_min, uint64_t child_max) {
    if (!vertex_range_initialized) {
      // First child - initialize both min and max to child's range
      particle_min_index = child_min;
      particle_max_index = child_max;
      vertex_range_initialized = true;
    } else {
      // Update range based on child's range
      if (child_min < particle_min_index) {
        particle_min_index = child_min;
      }
      if (child_max > particle_max_index) {
        particle_max_index = child_max;
      }
    }
  }
  
  // Update this node's min/max order based on a child's range
  void updateOrderRange(int child_min_order, int child_max_order) {
    if (!order_range_initialized) {
      // First child - initialize both min and max to child's range
      particle_min_order = child_min_order;
      particle_max_order = child_max_order;
      order_range_initialized = true;
    } else {
      // Update range based on child's range
      if (child_min_order < particle_min_order) {
        particle_min_order = child_min_order;
      }
      if (child_max_order > particle_max_order) {
        particle_max_order = child_max_order;
      }
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
  
  // Propagate vertex ID ranges from children up to this node
  virtual void propagateVertexIDRanges() = 0;

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

  virtual void propagateVertexIDRanges() override {
    // For leaf nodes, the range is already set by setParticleVertexID
    if (this->isLeaf()) {
      return;
    }
    
    // For internal nodes, first recursively propagate from children
    // then update this node's ranges based on children's ranges
    bool has_valid_vertex_child = false;
    bool has_valid_order_child = false;
    uint64_t min_vertex_range = UINT64_MAX;
    uint64_t max_vertex_range = 0;
    int min_order_range = INT_MAX;
    int max_order_range = INT_MIN;
    
    for (int i = 0; i < this->n_children; i++) {
      Node<Data>* child = getChild(i);
      if (child != nullptr) {
        // Recursively propagate from child first
        child->propagateVertexIDRanges();
        
        // Update vertex ID range based on this child if child has valid range
        if (child->vertex_range_initialized) {
          if (!has_valid_vertex_child) {
            // First valid child - initialize our vertex range
            min_vertex_range = child->particle_min_index;
            max_vertex_range = child->particle_max_index;
            has_valid_vertex_child = true;
          } else {
            // Subsequent valid children - update vertex range
            if (child->particle_min_index < min_vertex_range) {
              min_vertex_range = child->particle_min_index;
            }
            if (child->particle_max_index > max_vertex_range) {
              max_vertex_range = child->particle_max_index;
            }
          }
        }
        
        // Update order range based on this child if child has valid range
        if (child->order_range_initialized) {
          if (!has_valid_order_child) {
            // First valid child - initialize our order range
            min_order_range = child->particle_min_order;
            max_order_range = child->particle_max_order;
            has_valid_order_child = true;
          } else {
            // Subsequent valid children - update order range
            if (child->particle_min_order < min_order_range) {
              min_order_range = child->particle_min_order;
            }
            if (child->particle_max_order > max_order_range) {
              max_order_range = child->particle_max_order;
            }
          }
        }
      }
    }
    
    // Update this node's vertex range if we found valid children
    if (has_valid_vertex_child) {
      this->particle_min_index = min_vertex_range;
      this->particle_max_index = max_vertex_range;
      this->vertex_range_initialized = true;
    }
    
    // Update this node's order range if we found valid children
    if (has_valid_order_child) {
      this->particle_min_order = min_order_range;
      this->particle_max_order = max_order_range;
      this->order_range_initialized = true;
    }
    
    #ifdef DEBUG_VERTEX_IDS
    if (has_valid_child) {
      CkPrintf("Node %lu: Updated vertex ID range [%lu, %lu] from children\n", 
               this->key, this->particle_min_index, this->particle_max_index);
    }
    #endif
  }


private:
  std::array<std::atomic<Node<Data>*>, BRANCH_FACTOR> children; 
};

#endif // PARATREET_NODE_H_
