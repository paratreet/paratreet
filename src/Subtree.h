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
#include "LBCommon.h"

#include <cstring>
#include <queue>
#include <vector>
#include <fstream>

CkpvExtern(int, _lb_obj_index);
extern CProxy_TreeSpec treespec;
extern CProxy_Reader readers;
using namespace LBCommon;

template <typename Data>
class Subtree : public CBase_Subtree<Data> {
public:
  std::vector<Particle> particles, incoming_particles;
  std::vector<Node<Data>*> leaves;
  std::vector<Node<Data>*> empty_leaves;

  int n_total_particles;
  int n_subtrees;
  int n_partitions;
  int count = 0;

  #if CMK_LB_USER_DATA
  Vector3D<Real> centroid;
  #endif

  bool matching_decomps;

  Key tp_key; // Should be a prefix of all particle keys underneath this node
  Node<Data>* local_root; // Root node of this Subtree, TreeCanopies sit above this node
  MultiData<Data> flat_subtree;

  CProxy_TreeCanopy<Data> tc_proxy;
  CProxy_CacheManager<Data> cm_proxy;
  CProxy_Resumer<Data> r_proxy;
  Resumer<Data>* r_local = nullptr;
  CacheManager<Data>* cm_local = nullptr;

  std::unique_ptr<Traverser<Data>> traverser;

  std::vector<Particle> flushed_particles; // For debugging

  Subtree(const CkCallback&, int, int, int, TCHolder<Data>,
          CProxy_Resumer<Data>, CProxy_CacheManager<Data>, DPHolder<Data>, bool);
  Subtree(CkMigrateMessage * msg){
    delete msg;
  };
  void receive(ParticleMsg*);
  void buildTree(CProxy_Partition<Data>, CkCallback);
  void recursiveBuild(Node<Data>*, Particle* node_particles, size_t);
  void populateTree();
  inline void initCache();
  void sendLeaves(CProxy_Partition<Data>);
  template <typename Visitor> void startDual();
  void goDown();
  void requestNodes(Key, int);
  void requestCopy(int, PPHolder<Data>);
  void print(Node<Data>*);
  void destroy();
  void reset();
  void output(CProxy_Writer w, CkCallback cb);
  void pup(PUP::er& p);
  void collectMetaData(const CkCallback & cb);
  void addNodeToFlatSubtree(Node<Data>* node);
  void pauseForLB(){
    #if CMK_LB_USER_DATA
    // incoming_particles contains the new set of particles after perturb
    for (auto p : incoming_particles ){
      centroid += p.position;
    }
    if (incoming_particles.size() == 0){
      Real zero = 0.0;
      centroid = Vector3D<Real>(zero, zero, zero);
    }else{
      centroid /= (Real) incoming_particles.size();
    }

    ckout << incoming_particles.size() << "::" << centroid <<endl;
    if (CkpvAccess(_lb_obj_index) != -1) {
      void *data = this->getObjUserData(CkpvAccess(_lb_obj_index));
      LBUserData lb_data{
        st, this->thisIndex, (int)incoming_particles.size(),
        n_total_particles, centroid
      };
      *(LBUserData *) data = lb_data;
    }
    #endif
    this->AtSync();
  }

  void ResumeFromSync(){
    return;
  };

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
                       CProxy_CacheManager<Data> cm_proxy_, DPHolder<Data> dp_holder,
                       bool matching_decomps_){
  if (!matching_decomps_) this->usesAtSync = true;
  n_total_particles = n_total_particles_;
  n_subtrees = n_subtrees_;
  n_partitions = n_partitions_;

  tc_proxy = tc_holder.proxy;
  cm_proxy = cm_proxy_;
  r_proxy  = r_proxy_;

  matching_decomps = matching_decomps_;

  tp_key = treespec.ckLocalBranch()->getSubtreeDecomposition()->
    getTpKey(this->thisIndex);

  // Create TreeCanopies and send proxies
  auto sendProxy =
    [&](Key dest, int tp_index) {
      tc_proxy[dest].recvProxies(TPHolder<Data>(this->thisProxy),
                                 tp_index, cm_proxy, dp_holder);
    };

  treespec.ckLocalBranch()->getTree()->buildCanopy(this->thisIndex, sendProxy);

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
  p | r_proxy;
  p | count;
  p | incoming_particles;
  p | matching_decomps;
}

template <typename Data>
void Subtree<Data>::receive(ParticleMsg* msg) {
  // Copy particles to local vector
  // TODO: Remove memcpy by just storing the pointer to msg->particles
  // and using it in tree build
  int initial_size = incoming_particles.size();
  incoming_particles.resize(initial_size + msg->n_particles);
  std::memcpy(&incoming_particles[initial_size], msg->particles,
              msg->n_particles * sizeof(Particle));
  delete msg;
}

template <typename Data>
void Subtree<Data>::collectMetaData (const CkCallback & cb) {
  Real maxVelocity = 0.0;
  for (auto& particle : particles){
    if (particle.velocity.lengthSquared() > maxVelocity)
      maxVelocity = particle.velocity.lengthSquared();
  }
  maxVelocity = std::sqrt(maxVelocity);

  const size_t numTuples = 1;
  CkReduction::tupleElement tupleRedn[] = {
    CkReduction::tupleElement(sizeof(Real), &maxVelocity, CkReduction::max_float)
  };

  CkReductionMsg * msg = CkReductionMsg::buildFromTuple(tupleRedn, numTuples);
  msg->setCallback(cb);
  this->contribute(msg);
};

template <typename Data>
void Subtree<Data>::sendLeaves(CProxy_Partition<Data> part)
{
  // When Subtree and Partition have the same decomp type
  // there is a consistant 1-on-1 mapping
  // partical.partition_idx is ignored
  if (matching_decomps) {
    auto it = cm_proxy.ckLocalBranch()->partition_lookup.find(this->thisIndex);
    it->second->addLeaves(leaves, this->thisIndex);
    return;
  }

  // When Subtree and Parition has different decomp types
  std::map<int, std::set<Node<Data>*>> part_idx_to_leaf;
  for (auto && leaf : leaves) {
    for (int pi = 0; pi < leaf->n_particles; pi++) {
      auto partition_idx = leaf->particles()[pi].partition_idx;
      part_idx_to_leaf[partition_idx].insert(leaf);
    }
  }


  for (auto && part_receiver : part_idx_to_leaf) {
    auto it = cm_proxy.ckLocalBranch()->partition_lookup.find(part_receiver.first);
    if (it != cm_proxy.ckLocalBranch()->partition_lookup.end()) {
      std::vector<Node<Data>*> leaf_ptrs (part_receiver.second.begin(), part_receiver.second.end());
      it->second->addLeaves(leaf_ptrs, this->thisIndex);
    }
    else {
      std::vector<Key> lookup_leaf_keys;
      for (auto && leaf : part_receiver.second) {
	lookup_leaf_keys.push_back(leaf->key);
      }
      part[part_receiver.first].receiveLeaves(lookup_leaf_keys, tp_key, this->thisIndex, this->thisProxy);
    }
  }
}

template <typename Data>
template <typename Visitor>
void Subtree<Data>::startDual() {
  r_local = r_proxy.ckLocalBranch();
  r_local->resume_nodes_per_part.resize(n_subtrees);
  r_local->subtree_proxy = this->thisProxy;
  r_local->use_subtree = true;
  cm_local = cm_proxy.ckLocalBranch();
  traverser.reset(new DualTraverser<Data, Visitor>(*this));
  traverser->start();
}

template <typename Data>
void Subtree<Data>::goDown() {
  traverser->resumeTrav();
}

template <typename Data>
void Subtree<Data>::addNodeToFlatSubtree(Node<Data>* node) {
  SpatialNode<Data> sn (*node);
  flat_subtree.nodes.emplace_back(node->key, sn);
  for (int i = 0; i < node->n_children; i++) {
    addNodeToFlatSubtree(node->getChild(i));
  }
}

template <typename Data>
void Subtree<Data>::requestCopy(int cm_index, PPHolder<Data> pp_holder) {
  if (flat_subtree.nodes.empty()) addNodeToFlatSubtree(local_root);
  cm_proxy[cm_index].receiveSubtree(flat_subtree, pp_holder);
}

template <typename Data>
void Subtree<Data>::buildTree(CProxy_Partition<Data> part, CkCallback cb) {
  count += 1;
  // Copy over received particles
  std::swap(particles, incoming_particles);
  srand (this->thisIndex);

  //if (count == 10){
  //  for (auto p : particles){
  //    if (rand() % 10000 == 1) ckout << "ST " << this->thisIndex << " Centroid : " << p.position << endl;
  //  }
  //}

  // Sort particles
  std::sort(particles.begin(), particles.end());

  // Clear existing data
  leaves.clear();
  empty_leaves.clear();

  // Create global root and build local tree recursively
#if DEBUG
  CkPrintf("[TP %d] key: 0x%" PRIx64 " particles: %d\n", this->thisIndex, tp_key, particles.size());
#endif
  local_root = treespec.ckLocalBranch()->template makeNode<Data>(tp_key, 0,
        particles.size(), particles.data(), 0, n_subtrees - 1, true, nullptr, this->thisIndex);
  Key lbf = log2(local_root->getBranchFactor()); // have to use treespec to get BF
  local_root->depth = Utility::getDepthFromKey(tp_key, lbf);
  recursiveBuild(local_root, &particles[0], lbf);

  flat_subtree.tp_index  = this->thisIndex;
  flat_subtree.cm_index  = cm_proxy.ckLocalBranch()->thisIndex;
  flat_subtree.particles = particles;

  // Populate the tree structure (including TreeCanopy)
  populateTree();
  r_proxy.ckLocalBranch()->countSubtreeParticles(particles.size());
  initCache();

  this->contribute(cb);
  sendLeaves(part);
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
  auto tree   = treespec.ckLocalBranch()->getTree();
  bool is_light = (node->n_particles <= config.max_particles_per_leaf);

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

  tree->prepParticles(node_particles, node->n_particles, node->key, log_branch_factor);
  for (int i = 0; i < node->n_children; i++) {
    int first_ge_idx = finish;
    if (i < node->n_children - 1) {
      first_ge_idx = tree->findChildsLastParticle(node_particles, start, finish, child_key, log_branch_factor);
    }
    int n_particles = first_ge_idx - start;

    // Create child and store in vector
    Node<Data>* child = treespec.ckLocalBranch()->template makeNode<Data>(child_key, node->depth + 1,
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
  cm_proxy.ckLocalBranch()->connect(local_root);
}

template <typename Data>
void Subtree<Data>::requestNodes(Key key, int cm_index) {
  if (cm_index == cm_proxy.ckLocalBranch()->thisIndex) return;
  Node<Data>* node = local_root->getDescendant(key);
  if (!node) CkPrintf("null found for key %lu on tp %d\n", key, this->thisIndex);
  cm_proxy.ckLocalBranch()->serviceRequest(node, cm_index);
}

template <typename Data>
void Subtree<Data>::reset() {
  particles.clear();
  flat_subtree.clear();
}

template <typename Data>
void Subtree<Data>::destroy() {
  reset();
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

#endif /* _SUBTREE_H_ */
