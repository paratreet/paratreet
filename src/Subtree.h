#ifndef _SUBTREE_H_
#define _SUBTREE_H_

#include "paratreet.decl.h"
#include "common.h"
#include "templates.h"
#include "ParticleMsg.h"
#include "NodeWrapper.h"
#include "Node.h"
#include "Utility.h"
#include "Reader.h"
#include "CacheManager.h"
#include "Resumer.h"
#include "Driver.h"
#include "OrientedBox.h"

#include <cstring>
#include <queue>
#include <vector>
#include <fstream>

extern CProxy_TreeSpec treespec_subtrees;
extern CProxy_TreeSpec treespec;
extern CProxy_Reader readers;

template <typename Data>
class Subtree : public CBase_Subtree<Data> {
public:
  std::vector<Particle> particles, incoming_particles;
  std::vector<Node<Data>*> leaves;
  std::vector<Node<Data>*> empty_leaves;

  int n_total_particles;
  int n_subtrees;
  int n_partitions;

  Key tp_key; // Should be a prefix of all particle keys underneath this node
  Node<Data>* local_root; // Root node of this Subtree, TreeCanopies sit above this node

  CProxy_TreeCanopy<Data> tc_proxy;
  CProxy_CacheManager<Data> cm_proxy;

  std::vector<Particle> flushed_particles; // For debugging

  Subtree(const CkCallback&, int, int, int, TCHolder<Data>,
          CProxy_Resumer<Data>, CProxy_CacheManager<Data>, DPHolder<Data>);
  void receive(ParticleMsg*);
  void buildTree();
  void recursiveBuild(Node<Data>*, Particle* node_particles, size_t);
  void populateTree();
  inline void initCache();
  void sendLeaves(CProxy_Partition<Data>);
  void requestNodes(Key, int);
  void print(Node<Data>*);
  void flush(CProxy_Reader);
  void destroy();
  void output(CProxy_Writer w, CkCallback cb);
  void pup(PUP::er& p);

  // For debugging
  void checkParticlesChanged(const CkCallback& cb) {
    bool result = true;
    if (particles.size() != flushed_particles.size()) {
      result = false;
      this->contribute(sizeof(bool), &result, CkReduction::logical_and_bool, cb);
      return;
    }
    for (int i = 0; i < particles.size(); i++) {
      if (!(particles[i] == flushed_particles[i])) {
        result = false;
        break;
      }
    }
    this->contribute(sizeof(bool), &result, CkReduction::logical_and_bool, cb);
  }
};

template <typename Data>
Subtree<Data>::Subtree(const CkCallback& cb, int n_total_particles_,
                       int n_subtrees_, int n_partitions_, TCHolder<Data> tc_holder,
                       CProxy_Resumer<Data> r_proxy_,
                       CProxy_CacheManager<Data> cm_proxy_, DPHolder<Data> dp_holder) {
  n_total_particles = n_total_particles_;
  n_subtrees = n_subtrees_;
  n_partitions = n_partitions_;

  tc_proxy = tc_holder.proxy;
  cm_proxy = cm_proxy_;

  tp_key = treespec_subtrees.ckLocalBranch()->getDecomposition()->
    getTpKey(this->thisIndex);

  // Create TreeCanopies and send proxies
  auto sendProxy =
    [&](Key dest, int tp_index) {
      tc_proxy[dest].recvProxies(TPHolder<Data>(this->thisProxy),
                                 tp_index, cm_proxy, dp_holder);
    };

  treespec_subtrees.ckLocalBranch()->getTree()->buildCanopy(this->thisIndex, sendProxy);

  local_root = nullptr;

  this->contribute(cb);
}

template <typename Data>
void Subtree<Data>::pup(PUP::er& p) {
  p | n_total_particles;
  p | n_subtrees;
  p | n_partitions;
  p | tp_key;
  p | tc_proxy;
  p | cm_proxy;
}

template <typename Data>
void Subtree<Data>::receive(ParticleMsg* msg) {
  // Copy particles to local vector
  // TODO: Remove memcpy by just storing the pointer to msg->particles
  // and using it in tree build
  particles.clear();
  int initial_size = incoming_particles.size();
  incoming_particles.resize(initial_size + msg->n_particles);
  std::memcpy(&incoming_particles[initial_size], msg->particles,
              msg->n_particles * sizeof(Particle));
  delete msg;
}

template <typename Data>
void Subtree<Data>::sendLeaves(CProxy_Partition<Data> part)
{
  std::map<int, std::set<Node<Data>*>> part_idx_to_leaf;
  for (auto && leaf : leaves) {
    for (int pi = 0; pi < leaf->n_particles; pi++) {
      auto partition_idx = leaf->particles()[pi].partition_idx;
      part_idx_to_leaf[partition_idx].insert(leaf);
    }
  }

  for (auto && part_receiver : part_idx_to_leaf) {
    std::vector<NodeWrapper> node_data;
    std::vector<Key> all_particle_keys;
    for (auto && leaf : part_receiver.second) {
      size_t n_particles_here = 0u;
      for (int pi = 0; pi < leaf->n_particles; pi++) {
        if (leaf->particles()[pi].partition_idx == part_receiver.first) {
          n_particles_here++;
          all_particle_keys.push_back(leaf->particles()[pi].key);
        }
      }
      node_data.emplace_back(leaf->key, n_particles_here, leaf->depth);
    }
    part[part_receiver.first].receiveLeaves(node_data, all_particle_keys, this->thisIndex);
  }
}

template <typename Data>
void Subtree<Data>::buildTree() {
  // Copy over received particles
  std::swap(particles, incoming_particles);

  // Sort particles
  std::sort(particles.begin(), particles.end());

  // Clear existing data
  leaves.clear();
  empty_leaves.clear();

  // Create global root and build local tree recursively
#if DEBUG
  CkPrintf("[TP %d] key: 0x%" PRIx64 " particles: %d\n", this->thisIndex, tp_key, particles.size());
#endif
  local_root = treespec_subtrees.ckLocalBranch()->template makeNode<Data>(tp_key, 0,
        particles.size(), particles.data(), 0, n_subtrees - 1, true, nullptr, this->thisIndex);
  Key lbf = log2(local_root->getBranchFactor()); // have to use treespec_subtrees to get BF
  local_root->depth = Utility::getDepthFromKey(tp_key, lbf);
  recursiveBuild(local_root, &particles[0], lbf);

  // Populate the tree structure (including TreeCanopy)
  populateTree();
  // Initialize cache
  initCache();
}

template <typename Data>
void Subtree<Data>::recursiveBuild(Node<Data>* node, Particle* node_particles, size_t log_branch_factor) {
#if DEBUG
  CkPrintf("[Level %d] created node 0x%" PRIx64 " with %d particles\n",
      node->depth, node->key, node->n_particles);
#endif
  // store reference to splitters
  //static std::vector<Splitter>& splitters = readers.ckLocalBranch()->splitters;
  auto config = treespec.ckLocalBranch()->getConfiguration();
  bool is_light = (node->n_particles <= ceil(BUCKET_TOLERANCE * config.max_particles_per_leaf));

  // we can stop going deeper if node is light
  if (is_light) {
    if (node->n_particles == 0) {
      node->type = Node<Data>::Type::EmptyLeaf;
      empty_leaves.push_back(node);
    }
    else {
      node->type = Node<Data>::Type::Leaf;
      leaves.push_back(node);
    }
    return;
  }

  // Create children
  node->type = Node<Data>::Type::Internal;
  node->n_children = node->wait_count = (1 << log_branch_factor);
  node->is_leaf = false;
  Key child_key = (node->key << log_branch_factor);
  int start = 0;
  int finish = start + node->n_particles;

  for (int i = 0; i < node->n_children; i++) {
    int first_ge_idx = OctTree::findChildsLastParticle(node, i, child_key, start, finish, log_branch_factor);
    int n_particles = first_ge_idx - start;

    // Create child and store in vector
    Node<Data>* child = treespec_subtrees.ckLocalBranch()->template makeNode<Data>(child_key, node->depth + 1,
        n_particles, node_particles + start, 0, n_subtrees - 1, true, node, this->thisIndex);
    /*
    Node<Data>* child = new Node<Data>(child_key, node->depth + 1, node->particles + start,
        n_particles, child_owner_start, child_owner_end, node);
    */
    node->exchangeChild(i, child);

    // Recursive tree build
    recursiveBuild(child, node_particles + start, log_branch_factor);

    start = first_ge_idx;
    child_key++;
  }
}

template <typename Data>
void Subtree<Data>::populateTree() {
  // Populates the global tree structure by going up the tree
  std::queue<Node<Data>*> going_up;

  for (auto leaf : leaves) {
    leaf->data = Data(leaf->particles(), leaf->n_particles);
    going_up.push(leaf);
  }
  for (auto empty_leaf : empty_leaves) {
    going_up.push(empty_leaf);
  }
  CkAssert(!going_up.empty());

  while (going_up.size()) {
    Node<Data>* node = going_up.front();
    going_up.pop();
    CkAssert(node);
    if (node->key == tp_key) {
      // We are at the root of the Subtree, send accumulated data to
      // parent TreeCanopy
      int branch_factor = node->getBranchFactor();
      Key tc_key = tp_key / branch_factor;
      if (tc_key > 0) tc_proxy[tc_key].recvData(*node, branch_factor);
    } else {
      // Add this node's data to the parent, and add parent to the queue
      // if all children have contributed
      Node<Data>* parent = node->parent;
      CkAssert(parent);
      parent->data += node->data;
      parent->wait_count--;
      if (parent->wait_count == 0) going_up.push(parent);
    }
  }
}

template <typename Data>
void Subtree<Data>::initCache() {
  cm_proxy.ckLocalBranch()->connect(local_root, false);
}

template <typename Data>
void Subtree<Data>::requestNodes(Key key, int cm_index) {
  Node<Data>* node = local_root->getDescendant(key);
  if (!node) CkPrintf("null found for key %lu on tp %d\n", key, this->thisIndex);
  cm_proxy.ckLocalBranch()->serviceRequest(node, cm_index);
}

template <typename Data>
void Subtree<Data>::flush(CProxy_Reader readers) {
  // debug
  flushed_particles.resize(0);
  flushed_particles.insert(flushed_particles.end(), particles.begin(), particles.end());

  ParticleMsg *msg = new (particles.size()) ParticleMsg(particles.data(), particles.size());
  readers[CkMyPe()].receive(msg);
  particles.resize(0);
}

template <typename Data>
void Subtree<Data>::destroy() {
  this->thisProxy[this->thisIndex].ckDestroy();
}

template <typename Data>
void Subtree<Data>::print(Node<Data>* node) {
  ostringstream oss;
  oss << "tree." << this->thisIndex << ".dot";
  ofstream out(oss.str().c_str());
  CkAssert(out.is_open());
  out << "digraph tree" << this->thisIndex << "{" << endl;
  node->dot(out);
  out << "}" << endl;
  out.close();
}

template <typename Data>
void Subtree<Data>::output(CProxy_Writer w, CkCallback cb) {
  std::vector<Particle> particles;

  for (const auto& leaf : leaves) {
    particles.insert(particles.end(),
                     leaf->particles(), leaf->particles() + leaf->n_particles);
  }

  std::sort(particles.begin(), particles.end(),
            [](const Particle& left, const Particle& right) {
              return left.order < right.order;
            });

  int particles_per_writer = n_total_particles / CkNumPes();
  if (particles_per_writer * CkNumPes() != n_total_particles)
    ++particles_per_writer;

  int particle_idx = 0;
  while (particle_idx < particles.size()) {
    int writer_idx = particles[particle_idx].order / particles_per_writer;
    int first_particle = writer_idx * particles_per_writer;
    std::vector<Particle> writer_particles;

    while (
      particles[particle_idx].order < first_particle + particles_per_writer
      && particle_idx < particles.size()
      ) {
      writer_particles.push_back(particles[particle_idx]);
      ++particle_idx;
    }

    w[writer_idx].receive(writer_particles, cb);
  }

  if (this->thisIndex != n_subtrees - 1) {
    this->thisProxy[this->thisIndex + 1].output(w, cb);
  }
}

#endif /* _SUBTREE_H_ */
