#ifndef _PARTITION_H_
#define _PARTITION_H_

#include <vector>

#include "CoreFunctions.h"

#include "Particle.h"
#include "Traverser.h"
#include "ParticleMsg.h"
#include "MultiData.h"
#include "ThreadStateHolder.h"
#include "paratreet.decl.h"
#include "LBCommon.h"
#include "unionFindLib.h"
#ifdef FOF
#include "LocalCalcs.h"
#include "FoFHooks.h"
#endif

CkpvExtern(int, _lb_obj_index);
extern CProxy_TreeSpec treespec;
extern CProxy_Reader readers;
extern CProxy_ThreadStateHolder thread_state_holder;
extern CProxy_UnionFindLib libProxy;

using namespace LBCommon;

template <typename Data>
struct Partition : public CBase_Partition<Data> {
  std::mutex receive_lock;
  std::vector<Node<Data>*> leaves;
  std::vector<Node<Data>*> tree_leaves;
  std::vector<Particle> saved_particles;
  bool matching_decomps;
  Real load;

  std::vector<std::unique_ptr<Traverser<Data>>> traversers;
  int n_partitions;

  std::map<int, std::vector<Key>> lookup_leaf_keys;

  unionFindVertex *libVertices;

  CProxy_TreeCanopy<Data> tc_proxy;
  CProxy_CacheManager<Data> cm_proxy;
  CacheManager<Data> *cm_local;
  CProxy_Resumer<Data> r_proxy;
  Resumer<Data>* r_local;


  Partition(int, CProxy_CacheManager<Data>, CProxy_Resumer<Data>, TCHolder<Data>, CProxy_Driver<Data> driver, bool);
  Partition(CkMigrateMessage * msg){delete msg;};

  void passUnionFindLib(CProxy_UnionFindLib ufl);

  template<typename Visitor> void startDown(Visitor v);
  template<typename Visitor> void startBasicDown(Visitor v);
  template<typename Visitor> void startUpAndDown(Visitor v);
  void goDown(size_t travIdx);
  void resumeAfterPause(size_t travIdx);
  void interact(const CkCallback& cb);

  void addLeaves(const std::vector<Node<Data>*>&, int);
  void receiveLeaves(std::vector<Key>, Key, int, TPHolder<Data>);
  void destroy();
  void reset();
  void kick(Real, CkCallback);
  void perturb(Real, CkCallback);
  void rebuild(BoundingBox, TPHolder<Data>, bool);
  void output(CProxy_Writer w, int n_total_particles, CkCallback cb);
  void output(CProxy_TipsyWriter w, int n_total_particles, CkCallback cb);
  void callPerLeafFn(paratreet::PerLeafAble<Data>&, const CkCallback&);
  void deleteParticleOfOrder(int order) {particle_delete_order.insert(order);}
  void requestParticleUpdates(int cm_index, std::vector<Key> pKeys);
  void applyOpposingEffects(std::vector<std::pair<Key, Particle::Effect>> effects);
  void pup(PUP::er& p);
  void makeLeaves(int);
  void pauseForLB(){
    this->AtSync();
  }
  void ResumeFromSync(){
    return;
  };
  void UserSetLBLoad()
  {
    Real total_volume = 0.0;
    int total_particles = 0;
    
    for (auto && leaf : leaves) {
      total_volume += leaf->data.box.volume();
      total_particles += leaf->n_particles;
    }
    
    if (total_volume > 0.0 && total_particles > 0) {
      // Higher density = higher computational load for FoF
      Real density = (Real)total_particles / total_volume;
      this->load = density;
    } else if (total_particles > 0) {
      this->load = (Real)total_particles;
    } else {
      this->load = 0.0;
    }
    
    this->setObjTime(this->load);

  }
  void initializeLibVertices(const CkCallback &cb);
  void propagateVertexIDRanges(const CkCallback &cb);
  void validateVertexIDRanges(const CkCallback &cb);
  inline uint64_t encodeChareAndArrayIndex(int particles_so_far);
  static std::pair<int, int> getLocationFromID(uint64_t vid);
  void unionRequest(int sp_order, int tp_order);
  void getConnectedComponents(const CkCallback& cb);
  void resetUnionRequestCounter(const CkCallback& cb);
  void reportUnionRequestCount(const CkCallback& cb);
#ifdef FOF
  void depositBucketPointers(CProxy_LocalCalcs<Data> lc_proxy, CProxy_LocalNodeCalcs<Data> lnc_proxy, const CkCallback& cb);
#endif

  Real time_advanced = 0;
  int iter = 1;

private:
  std::set<int> particle_delete_order;

private:
  void initLocalBranches();
  void erasePartition();
  void copyParticles(std::vector<Particle>& particles, bool check_delete);
  void startNewTraverser() {
    traversers.back()->start();
    if (traversers.back()->wantsPause()) {
      //CkPrintf("pausing trav %d\n", this->thisIndex);
      this->thisProxy[this->thisIndex].resumeAfterPause(traversers.size() - 1);
    }
  }
  void flush(CProxy_Reader, std::vector<Particle>&);
  void makeLeaves(const std::vector<Key>&, int);
  template <typename WriterProxy> void doOutput(WriterProxy w, int n_total_particles, CkCallback cb);
};

template <typename Data>
Partition<Data>::Partition(
  int np, CProxy_CacheManager<Data> cm,
  CProxy_Resumer<Data> rp, TCHolder<Data> tc_holder,
  CProxy_Driver<Data> driver, bool matching_decomps_
  )
{
  this->usesAtSync = true;
  this->load = 0.0;
  this->usesAutoMeasure = false;
  n_partitions = np;
  tc_proxy = tc_holder.proxy;
  r_proxy = rp;
  cm_proxy = cm;
  matching_decomps = matching_decomps_;
  initLocalBranches();
  time_advanced = readers.ckLocalBranch()->start_time;
  driver.partitionLocation(this->thisIndex, CkMyPe());
}

template <typename Data>
void Partition<Data>::initLocalBranches() {
  r_local = r_proxy.ckLocalBranch();
  r_local->part_proxy = this->thisProxy;
  cm_local = cm_proxy.ckLocalBranch();
  cm_local->lockMaps();
  cm_local->partition_lookup.emplace(this->thisIndex, this);
  cm_local->unlockMaps();
  r_local->cm_local = cm_local;
  cm_local->r_proxy = r_proxy;
}

template <typename Data>
void Partition<Data>::passUnionFindLib(CProxy_UnionFindLib ufl)
{
  libProxy = ufl;
}

template <typename Data>
template <typename Visitor>
void Partition<Data>::startDown(Visitor v)
{
  initLocalBranches();
  traversers.emplace_back(new TransposedDownTraverser<Data, Visitor>(v, traversers.size(), leaves, *this));
#ifdef FOF
  if (paratreet::fof_register_traverser)
    paratreet::fof_register_traverser(traversers.back().get(),
                                      this->thisIndex,
                                      traversers.size() - 1);
#endif
  startNewTraverser();
}

template <typename Data>
void Partition<Data>::resumeAfterPause(size_t travIdx)
{
#ifdef FOF
  if (paratreet::fof_on_resume)
    paratreet::fof_on_resume(traversers[travIdx].get(), this->thisIndex, travIdx);
  // If helpers are collectively working on this traverser's queue, skip:
  // the source PE re-triggers resumeAfterPause after the parallel phase ends.
  if (traversers[travIdx]->parallel_phase_active) return;
#endif
  traversers[travIdx]->resumeAfterPause();
#ifdef FOF
  if (paratreet::fof_update_traversal_work)
    paratreet::fof_update_traversal_work(traversers[travIdx]->pausedWorkSize());
#endif
  if (traversers[travIdx]->wantsPause()) {
    this->thisProxy[this->thisIndex].resumeAfterPause(travIdx);
  }
}

template <typename Data>
template <typename Visitor>
void Partition<Data>::startBasicDown(Visitor v)
{
  initLocalBranches();
  traversers.emplace_back(new BasicDownTraverser<Data, Visitor>(v, traversers.size(), leaves, *this));
  startNewTraverser();
}

template <typename Data>
template <typename Visitor>
void Partition<Data>::startUpAndDown(Visitor v)
{
  initLocalBranches();
  traversers.emplace_back(new UpnDTraverser<Data, Visitor>(v, traversers.size(), *this));
  startNewTraverser();
}

template <typename Data>
void Partition<Data>::goDown(size_t travIdx)
{
  traversers[travIdx]->resumeTrav();
}

template <typename Data>
void Partition<Data>::interact(const CkCallback& cb)
{
  for (auto& trav : traversers) trav->interact();
  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::requestParticleUpdates(int cm_index, std::vector<Key> pKeys) {
  std::set<Key> keySet (pKeys.begin(), pKeys.end());
  std::vector<Particle> particles_sending;
  for (auto& leaf : leaves) {
    for (int pi = 0; pi < leaf->n_particles; pi++) {
      if (keySet.count(leaf->particles()[pi].key)) {
        particles_sending.push_back(leaf->particles()[pi]);
      }
    }
  }
  cm_proxy[cm_index].receiveParticleUpdates(particles_sending);
}

template <typename Data>
void Partition<Data>::applyOpposingEffects(std::vector<std::pair<Key, Particle::Effect>> effects) {
  std::map<Key, Particle::Effect> effects_map (effects.begin(), effects.end());
  for (auto& leaf : leaves) {
    for (int pi = 0; pi < leaf->n_particles; pi++) {
      auto it = effects_map.find(leaf->particles()[pi].key);
      if (it != effects_map.end()) {
        //leaf->applyAcceleration(pi, it->second.first);
        leaf->applyGasWork(pi, it->second.second);
      }
    }
  }
}

template <typename Data>
void Partition<Data>::addLeaves(const std::vector<Node<Data>*>& leaf_ptrs, int subtree_idx) {
  decltype(leaves) new_leaves;
  if (!matching_decomps) {
    // When Subtree and Partition have the same decomp type
    // the tree structures will be identical
    // reuse leaf without checks and modifications
    new_leaves.reserve(leaf_ptrs.size());
    for (auto leaf : leaf_ptrs) {
      std::vector<Particle> leaf_particles;
      for (int pi = 0; pi < leaf->n_particles; pi++) {
        if (leaf->particles()[pi].partition_idx == this->thisIndex) {
          leaf_particles.push_back(leaf->particles()[pi]);
        }
      }
      if (leaf_particles.size() == leaf->n_particles) {
        new_leaves.push_back(leaf);
      }
      else {
        auto particles = new Particle [leaf_particles.size()];
        std::copy(leaf_particles.begin(), leaf_particles.end(), particles);
        auto node = cm_local->makeNode(leaf->key, Node<Data>::Type::Leaf, leaf->depth,
          leaf_particles.size(), particles, nullptr, subtree_idx, cm_local->thisIndex);
        // note here: cm_index is of the old home, not the new home. not sure about this
        new_leaves.push_back(node);
      }
    }
  }
  receive_lock.lock();
  tree_leaves.insert(tree_leaves.end(), leaf_ptrs.begin(), leaf_ptrs.end());
  if (matching_decomps) {
    leaves.insert(leaves.end(), leaf_ptrs.begin(), leaf_ptrs.end());
  }
  else leaves.insert(leaves.end(), new_leaves.begin(), new_leaves.end());
  receive_lock.unlock();
}

template <typename Data>
void Partition<Data>::receiveLeaves(std::vector<Key> leaf_keys, Key tp_key, int subtree_idx, TPHolder<Data> tp_holder) {
  cm_local->lockMaps();
  auto && local_tps = cm_proxy.ckLocalBranch()->local_tps;
  bool found = local_tps.find(tp_key) != local_tps.end();
  if (found) {
    cm_local->unlockMaps();
    makeLeaves(leaf_keys, subtree_idx);
  }
  else {
    lookup_leaf_keys[subtree_idx] = leaf_keys;
    auto& out = cm_local->subtree_copy_started[subtree_idx];
    bool should_request = out.empty();
    out.push_back(this->thisIndex);
    cm_local->unlockMaps();
    if (should_request) {
      tp_holder.proxy[subtree_idx].requestCopy(cm_local->thisIndex, this->thisProxy);
    }
  }
}

template <typename Data>
void Partition<Data>::makeLeaves(const std::vector<Key>& keys, int subtree_idx) {
  cm_local->lockMaps();
  std::vector<Node<Data>*> leaf_ptrs;
  for (auto && k : keys) {
    auto it = cm_local->leaf_lookup.find(k);
    CkAssert(it != cm_local->leaf_lookup.end());
    leaf_ptrs.push_back(it->second);
  }
  cm_local->unlockMaps();
  addLeaves(leaf_ptrs, subtree_idx);
}

template <typename Data>
void Partition<Data>::makeLeaves(int subtree_idx) {
  auto keys = lookup_leaf_keys[subtree_idx];
  makeLeaves(keys, subtree_idx);
}

template <typename Data>
void Partition<Data>::destroy()
{
  readers.ckLocalBranch()->start_time = time_advanced;
  reset();
  erasePartition();
  this->thisProxy[this->thisIndex].ckDestroy();
}

template <typename Data>
void Partition<Data>::reset()
{
  traversers.clear();
  for (int i = 0; i < leaves.size(); i++) {
    if (leaves[i] != tree_leaves[i]) {
      leaves[i]->freeParticles();
    }
  }
  lookup_leaf_keys.clear();
  leaves.clear();
  tree_leaves.clear();
}

template <typename Data>
void Partition<Data>::pup(PUP::er& p)
{
  p | n_partitions;
  p | tc_proxy;
  p | cm_proxy;
  p | r_proxy;
  p | matching_decomps;
  p | load;
  if (p.isUnpacking()) {
    initLocalBranches();
  }
  else {
    erasePartition();
  }
}

template <typename Data>
void Partition<Data>::erasePartition() {
  cm_local->lockMaps();
  cm_local->partition_lookup.erase(this->thisIndex);
  cm_local->unlockMaps();
}

template <typename Data>
void Partition<Data>::kick(Real timestep, CkCallback cb)
{
  for (auto && leaf : leaves) {
    //leaf->kick(timestep);
  }
  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::perturb(Real timestep, CkCallback cb)
{
  time_advanced += timestep;
  iter += 1;
  BoundingBox box;
  copyParticles(saved_particles, true);

  #if CMK_LB_USER_DATA
  Real zero = 0.0;
  Vector3D<Real> centroid(zero, zero, zero);

  int size = saved_particles.size();
  for (auto& p : saved_particles){
    centroid += p.position;
  }
  if (size > 0){
    centroid /= (Real) size;
  }
  if (CkpvAccess(_lb_obj_index) != -1) {
    void *data = this->getObjUserData(CkpvAccess(_lb_obj_index));
    LBUserData lb_data{
      pt, this->thisIndex,
      size, 0,
      centroid
    };

    *(LBUserData *) data = lb_data;
  }
  #endif

  for (auto && p : saved_particles) {
    //p.perturb(timestep);
    box.grow(p.position);
    box.mass += p.mass;
    box.ke += 0.5 * p.mass * p.velocity.lengthSquared();
    if (p.isGas()) box.n_sph++;
    if (p.isDark()) box.n_dark++;
    if (p.isStar()) box.n_star++;
  }
  box.n_particles = saved_particles.size();
  this->contribute(sizeof(BoundingBox), &box, BoundingBox::reducer(), cb);
}

template <typename Data>
void Partition<Data>::rebuild(BoundingBox universe, TPHolder<Data> tp_holder, bool if_flush)
{
  thread_state_holder.ckLocalBranch()->countPartitionParticles(saved_particles.size());
  for (auto && p : saved_particles) {
    p.adjustNewUniverse(universe.box);
  }

  if (if_flush) {
    flush(readers, saved_particles);
  }
  else {
    auto sendParticles = [&](int dest, int n_particles, Particle* particles) {
      ParticleMsg* msg = new (n_particles) ParticleMsg(particles, n_particles);
      tp_holder.proxy[dest].receive(msg);
    };
    treespec.ckLocalBranch()->getSubtreeDecomposition()->flush(saved_particles, sendParticles);
  }
  saved_particles.clear();
}

template <typename Data>
void Partition<Data>::flush(CProxy_Reader readers, std::vector<Particle>& particles)
{
  ParticleMsg *msg = new (particles.size()) ParticleMsg(
    particles.data(), particles.size()
    );
  readers[CkMyPe()].receive(msg);
}

template <typename Data>
void Partition<Data>::callPerLeafFn(paratreet::PerLeafAble<Data>& perLeafFn, const CkCallback& cb)
{
  for (auto && leaf : leaves) {
    perLeafFn(*leaf, this);
  }

  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::copyParticles(std::vector<Particle>& particles, bool check_delete) {
  for (auto && leaf : leaves) {
    for (int i = 0; i < leaf->n_particles; i++) {
      if (!check_delete || particle_delete_order.find(leaf->particles()[i].order) == particle_delete_order.end()) {
        particles.emplace_back(leaf->particles()[i]);
      }
    }
  }
}

template <typename Data>
void Partition<Data>::output(CProxy_Writer w, int n_total_particles, CkCallback cb)
{
  doOutput(w, n_total_particles, cb);
}


template <typename Data>
void Partition<Data>::output(CProxy_TipsyWriter w, int n_total_particles, CkCallback cb)
{
  doOutput(w, n_total_particles, cb);
}
template <typename Data>
template <typename WriterProxy>
void Partition<Data>::doOutput(WriterProxy w, int n_total_particles, CkCallback cb)
{
  std::vector<Particle> particles;
  copyParticles(particles, false);

  // sort particles into original order and sends them to writer class
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

    w[writer_idx].receive(writer_particles, time_advanced, iter);
  }
}

// -------------------
// Friends-of-Friends (FoF) functions
// -------------------
#ifdef FOF

template <typename Data>
void Partition<Data>::depositBucketPointers(CProxy_LocalCalcs<Data> lc_proxy, CProxy_LocalNodeCalcs<Data> lnc_proxy, const CkCallback& cb) {
  LocalCalcs<Data>* lc_local = lc_proxy.ckLocalBranch();
  lc_local->cross_partition_union_count = 0;
  lc_local->compress_count = 0;
  int nParticles = 0;
  for (auto leaf : leaves) {
    lc_local->depositBucket(leaf);
    nParticles += leaf->n_particles;
  }
  UnionFindLib* lib = libProxy[this->thisIndex].ckLocal();
  if (lib) {
    lc_local->depositVertexArray(this->thisIndex, lib->return_vertices(), nParticles);
    lnc_proxy.ckLocalBranch()->depositVertexArraysNode(this->thisIndex, lib->return_vertices(), nParticles);
  }
  this->contribute(cb);
}

// Per-PE counters for union_request calls.  thread_local gives one instance
// per PE (each Charm++ PE owns one OS thread in SMP mode).  static gives
// internal linkage so the definition is safe in this header.
static thread_local long long fof_union_request_count = 0;
static thread_local bool fof_count_reported = false;

inline void fof_reset_union_request_counter() {
  fof_union_request_count = 0;
  fof_count_reported = false;
}

inline long long fof_get_union_request_count() {
  return fof_union_request_count;
}
/**
 * @brief Initializes an instance of unionFindLib by reading in all particles
 * stored on this partition. Must be called after partitions are initialized
 * with particles
 * 
 * @param cb the callback that will be invoked when this function finishes
 */
template <typename Data>
void Partition<Data>::initializeLibVertices(const CkCallback& cb) {
  int n_particles_on_partition = 0;
  std::vector<const Particle*> particles;
  for (auto && leaf : leaves) {
    n_particles_on_partition += leaf->n_particles;
  }
  libVertices = new unionFindVertex[n_particles_on_partition];

  int particles_so_far = 0;
  for (auto && leaf : leaves) {
    for (int i = 0; i < leaf->n_particles; i++) {
      // encodes chare index and array index into vertexID
      uint64_t vertexID = encodeChareAndArrayIndex(particles_so_far);
      leaf->setParticleVertexID(i, vertexID);
      libVertices[particles_so_far].vertexID = vertexID;

      #ifndef ANCHOR_ALGO
      libVertices[particles_so_far].parent = -1;
      #else
      libVertices[particles_so_far].parent = libVertices[particles_so_far].vertexID;
      #endif
      particles_so_far++;
    }
  }

  UnionFindLib *libPtr = libProxy[this->thisIndex].ckLocal();

  libPtr->initialize_vertices(libVertices, n_particles_on_partition);
  libPtr->registerGetLocationFromID(getLocationFromID);
  this->contribute(cb);
}

/**
 * @brief Propagates vertex ID ranges from leaf nodes up through the tree hierarchy.
 * This should be called after initializeLibVertices and before any traversals.
 * Each internal node will have its particle_min_index and particle_max_index
 * set to encompass all vertex IDs in its subtree.
 * 
 * @param cb Callback to execute after function finishes executing
 */
template <typename Data>
void Partition<Data>::propagateVertexIDRanges(const CkCallback& cb) {
  // Propagate vertex ID ranges from leaves up through tree_leaves
  // tree_leaves contains the original tree structure before filtering for this partition
  std::set<Node<Data>*> processed_roots;
  
  #ifdef DEBUG_VERTEX_IDS
  CkPrintf("Partition %d: Starting vertex ID range propagation for %lu tree leaves\n", 
           this->thisIndex, tree_leaves.size());
  #endif
  
  for (auto && tree_leaf : tree_leaves) {
    // Find the root of this tree by going up through parents
    Node<Data>* current = tree_leaf;
    Node<Data>* root = current;
    
    // Find root node by traversing up parent pointers
    while (current && current->parent != nullptr) {
      root = current->parent;
      current = current->parent;
    }
    
    // Only propagate from this root once (avoid duplicates)
    if (processed_roots.find(root) == processed_roots.end()) {
      #ifdef DEBUG_VERTEX_IDS
      CkPrintf("Partition %d: Propagating from root node %lu\n", 
               this->thisIndex, root->key);
      #endif
      root->propagateVertexIDRanges();
      processed_roots.insert(root);
    }
  }
  
  #ifdef DEBUG_VERTEX_IDS
  CkPrintf("Partition %d: Completed vertex ID range propagation for %lu roots\n", 
           this->thisIndex, processed_roots.size());
  #endif
  
  this->contribute(cb);
}

/**
 * @brief Validates that vertex ID ranges are correctly propagated through the tree.
 * Prints information about the ranges for debugging purposes.
 * 
 * @param cb Callback to execute after function finishes executing
 */
template <typename Data>
void Partition<Data>::validateVertexIDRanges(const CkCallback& cb) {
  /* DEBUG: Uncomment to enable range validation output
  CkPrintf("Partition %d: Vertex ID Range Validation\n", this->thisIndex);
  CkPrintf("  Tree leaves: %lu, Partition leaves: %lu\n", 
           tree_leaves.size(), leaves.size());
  
  // Check ranges in leaves
  for (size_t i = 0; i < leaves.size() && i < 5; i++) {  // Limit output to first 5 for brevity
    auto leaf = leaves[i];
    CkPrintf("  Leaf %lu: vertex_range [%lu, %lu], order_range [%d, %d], particles: %d\n", 
             leaf->key, leaf->particle_min_index, leaf->particle_max_index, 
             leaf->particle_min_order, leaf->particle_max_order, leaf->n_particles);
  }
  
  // Check ranges in some tree nodes by following parents
  if (!tree_leaves.empty()) {
    auto sample_leaf = tree_leaves[0];
    Node<Data>* current = sample_leaf;
    int level = 0;
    while (current != nullptr && level < 3) {  // Check up to 3 levels
      CkPrintf("  Level %d Node %lu: vertex_range [%lu, %lu], order_range [%d, %d], particles: %d, children: %d\n", 
               level, current->key, current->particle_min_index, 
               current->particle_max_index, current->particle_min_order,
               current->particle_max_order, current->n_particles, current->n_children);
      current = current->parent;
      level++;
    }
  }
  */
  
  this->contribute(cb);
}

/**
 * @brief given the overall sequential order of the particle on this partition,
 * returns a vertexID encoding the vertex's location (chare and array index)
 * 
 * returns a vertexID that encodes
 * the chare index of the vertex in the 32 most significant bits
 * and the local array index of the vertex in the 32 least significant bits
 * The vertex is stored locally in the myVertices array of unionfind
 * and has index equal to the overall sequential order of the particle
 * on this partition
 * 
 * @param particles_so_far overall sequential order of the particle
 * on this partition
 * 
 * @return uint64_t vertexID encoding location of vertex
 */
template <typename Data>
inline uint64_t Partition<Data>::encodeChareAndArrayIndex(int particles_so_far) {
  
  uint64_t x = (((uint64_t)this->thisIndex) << 32) | particles_so_far;
  
  return x;
}

/**
 * @brief decodes a vertexID and returns its location (chare and array index)
 * 
 * @param vid the ID of a vertex. Assumes it is of type uint64_t
 * @return std::pair<int, int> 
 */
// Used with unionFindLib. Given a vertexID, returns the chare index and local array index where the particle (vertex) is stored
// Assumes parameter vid stores the chare index in the 32 most significant bits and the array index in the 32 least significant bits
template <typename Data>
std::pair<int, int> Partition<Data>::getLocationFromID(uint64_t vid) {
  // decodes chare index and array index from vertexID
  int chareIdx = (vid >> 32);
  int arrIdx = vid & 0xffffffff;
  return std::make_pair(chareIdx, arrIdx);
}

/**
 * @brief Assigns component (group) number to particles after unions are
 * performed between all particles on this partition
 * 
 * @param cb Callback to execute after function finishes executing
 */
template <typename Data>
void Partition<Data>::getConnectedComponents(const CkCallback& cb) {
  int particles_so_far = 0;
  for (auto && leaf : leaves) {
    for (int i = 0; i < leaf->n_particles; i++) {
      long startingGroupNumberOffset = 1;  // so particles that are not part of a cluster have groupID 0 (currently -1 in unionFind implementation)
      long groupID = libVertices[particles_so_far].componentNumber + startingGroupNumberOffset;
      leaf->setParticleGroupNumber(i, groupID);
      particles_so_far++;
    }
  }
  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::resetUnionRequestCounter(const CkCallback& cb) {
  fof_reset_union_request_counter();
  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::reportUnionRequestCount(const CkCallback& cb) {
  long long count = 0;
  if (!fof_count_reported) {
    fof_count_reported = true;
    count = fof_union_request_count;
    CkPrintf("[PE %d] union_request calls: %lld\n", CkMyPe(), count);
  }
  this->contribute(sizeof(long long), &count, CkReduction::sum_long, cb);
}
#endif // FOF

#endif /* _PARTITION_H_ */
