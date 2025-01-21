#ifndef _PARTITION_H_
#define _PARTITION_H_

#include <vector>

#include "CoreFunctions.h"
#include "Traverser.h"
#include "MultiData.h"
#include "paratreet.decl.h"
#include "LBCommon.h"
#include "StatisticsTracker.h"

CkpvExtern(int, _lb_obj_index);
extern CProxy_TreeSpec treespec;
using namespace LBCommon;

template <typename Data>
class Partition : public CBase_Partition<Data> {
private:
  std::mutex receive_lock;

public:
  std::vector<Node<Data>*> leaves;
  std::vector<Node<Data>*> tree_leaves;
  std::vector<typename Data::Particle> saved_particles;
  bool matching_decomps;

  std::vector<std::unique_ptr<Traverser<Data>>> traversers;
  int n_partitions;

  std::map<int, std::vector<Key>> lookup_leaf_keys;

  // filled in during traversal

  CProxy_Reader<Data> readers;
  CProxy_TreeCanopy<Data> tc_proxy;
  CProxy_CacheManager<Data> cm_proxy;
  CacheManager<Data> *cm_local = nullptr;
  CProxy_Resumer<Data> r_proxy;
  Resumer<Data>* r_local = nullptr;
  CProxy_StatisticsTracker s_proxy;
  StatisticsTracker* s_local = nullptr;

public: // acceptable for users to access
  OppositeEffectsManager<Data>* opposite_effects_manager = nullptr;

  Partition(int, CProxy_Reader<Data>, CProxy_CacheManager<Data>, CProxy_StatisticsTracker, CProxy_OppositeEffectsManager<Data>, CProxy_Resumer<Data>, TCHolder<Data>, CProxy_Driver<Data> driver, bool);
  Partition(CkMigrateMessage * msg){delete msg;};

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
  void rebuild(typename Data::BoundingBox, TPHolder<Data>, bool);
  void globalSortToReader(int n_total_particles);
  void callPerLeafFn(paratreet::PerLeafAble<Data>&, const CkCallback&);
  void deleteParticleOfOrder(int order) {particle_delete_order.insert(order);}
  void requestParticleUpdates(int cm_index, std::vector<Key> pKeys);
  void applyOpposingEffects(std::vector<std::pair<Key, typename Data::Particle::Effect>> effects);
  void pup(PUP::er& p);
  void makeLeaves(int);
  void pauseForLB(){
    this->AtSync();
  }
  void ResumeFromSync(){
    return;
  };

private:
  std::set<int> particle_delete_order;

private:
  void initLocalBranches();
  void erasePartition();
  void copyParticlesFromLeaves(std::vector<typename Data::Particle>& particles, bool check_delete);
  void startNewTraverser() {
    traversers.back()->start();
    if (traversers.back()->wantsPause()) {
      //CkPrintf("pausing trav %d\n", this->thisIndex);
      this->thisProxy[this->thisIndex].resumeAfterPause(traversers.size() - 1);
    }
  }
  void makeLeaves(const std::vector<Key>&, int);
};

template <typename Data>
Partition<Data>::Partition(
  int np, CProxy_Reader<Data> rdp,
  CProxy_CacheManager<Data> cm, CProxy_StatisticsTracker s,
  CProxy_OppositeEffectsManager<Data> oem,
  CProxy_Resumer<Data> rp, TCHolder<Data> tc_holder,
  CProxy_Driver<Data> driver, bool matching_decomps_
  )
{
  this->usesAtSync = true;
  n_partitions = np;
  tc_proxy = tc_holder.proxy;
  readers = rdp;
  r_proxy = rp;
  cm_proxy = cm;
  s_proxy = s;
  matching_decomps = matching_decomps_;
  initLocalBranches();
  driver.partitionLocation(this->thisIndex, CkMyPe());
  opposite_effects_manager = oem.ckLocalBranch();
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
  s_local = s_proxy.ckLocalBranch();
}

template <typename Data>
template <typename Visitor>
void Partition<Data>::startDown(Visitor v)
{
  initLocalBranches();
  traversers.emplace_back(new TransposedDownTraverser<Data, Visitor>(v, traversers.size(), leaves, *this, s_local));
  startNewTraverser();
}

template <typename Data>
void Partition<Data>::resumeAfterPause(size_t travIdx)
{
  traversers[travIdx]->resumeAfterPause();
  if (traversers[travIdx]->wantsPause()) {
    //CkPrintf("pausing trav %d\n", this->thisIndex);
    this->thisProxy[this->thisIndex].resumeAfterPause(travIdx);
  }
}

template <typename Data>
template <typename Visitor>
void Partition<Data>::startBasicDown(Visitor v)
{
  initLocalBranches();
  traversers.emplace_back(new BasicDownTraverser<Data, Visitor>(v, traversers.size(), leaves, *this, s_local));
  startNewTraverser();
}

template <typename Data>
template <typename Visitor>
void Partition<Data>::startUpAndDown(Visitor v)
{
  initLocalBranches();
  traversers.emplace_back(new UpnDTraverser<Data, Visitor>(v, traversers.size(), *this, s_local));
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
  std::vector<typename Data::Particle> particles_sending;
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
void Partition<Data>::applyOpposingEffects(std::vector<std::pair<Key, typename Data::Particle::Effect>> effects) {
  std::map<Key, typename Data::Particle::Effect> effects_map (effects.begin(), effects.end());
  for (auto& leaf : leaves) {
    for (int pi = 0; pi < leaf->n_particles; pi++) {
      auto it = effects_map.find(leaf->particles()[pi].key);
      if (it != effects_map.end()) {
        leaf->applyEffect(pi, it->second);
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
      std::vector<typename Data::Particle> leaf_particles;
      for (int pi = 0; pi < leaf->n_particles; pi++) {
        if (leaf->particles()[pi].partition_idx == this->thisIndex) {
          leaf_particles.push_back(leaf->particles()[pi]);
        }
      }
      if (leaf_particles.size() == leaf->n_particles) {
        new_leaves.push_back(leaf);
      }
      else {
        auto particles = new typename Data::Particle [leaf_particles.size()];
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
    leaf->kick(timestep);
  }
  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::perturb(Real timestep, CkCallback cb)
{
  copyParticlesFromLeaves(saved_particles, true);

  #if CMK_LB_USER_DATA
  Real zero = 0.0;
  Vector3D<Real> centroid(zero, zero, zero);

  int size = saved_particles.size();
  for (auto& p : saved_particles){
    centroid += p.position;
  }
  if (size > 0) {
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

  typename Data::BoundingBox box;
  for (auto && p : saved_particles) {
    p.perturb(timestep);
    Data::addParticleToBox(p, box);
  }
  this->contribute(sizeof(typename Data::BoundingBox), &box, Data::BoundingBox::reducer(), cb);
}

template <typename Data>
void Partition<Data>::rebuild(typename Data::BoundingBox universe, TPHolder<Data> tp_holder, bool flush_to_reader)
{
  s_local->countPartitionParticles(saved_particles.size());
  for (auto && p : saved_particles) {
    Data::adjustParticleForUniverse(p, universe);
  }

  if (flush_to_reader) {
    readers.ckLocalBranch()->receive(saved_particles);
  }
  else {
    readers.ckLocalBranch()->flushToSubtreesHelper(saved_particles, tp_holder.proxy);
  }
  saved_particles.clear();
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
void Partition<Data>::copyParticlesFromLeaves(std::vector<typename Data::Particle>& particles, bool check_delete) {
  for (auto && leaf : leaves) {
    for (int i = 0; i < leaf->n_particles; i++) {
      if (!check_delete || particle_delete_order.find(leaf->particles()[i].order) == particle_delete_order.end()) {
        particles.emplace_back(leaf->particles()[i]);
      }
    }
  }
}

template <typename Data>
void Partition<Data>::globalSortToReader(int n_total_particles)
{
  std::vector<typename Data::Particle> particles;
  copyParticlesFromLeaves(particles, false);

  std::sort(particles.begin(), particles.end(),
            [](const typename Data::Particle& left, const typename Data::Particle& right) {
              return left.order < right.order;
            });
  auto n_readers = readers.ckLocalBranch()->numReaders();
  int particles_per_reader = n_total_particles / n_readers;
  if (particles_per_reader * n_readers != n_total_particles) {
    ++particles_per_reader;
  }
  for (int begin = 0; begin < particles.size();) {
    int reader_idx = particles[begin].order / particles_per_reader;
    int cutoff_order = particles_per_reader * (1 + reader_idx);
    auto num_contiguous = std::find_if(particles.begin() + begin, particles.end(), [cutoff_order] (auto && p) {
		    return p.order >= cutoff_order;}) - (particles.begin() + begin);
    readers[reader_idx].receive(std::vector<typename Data::Particle>(particles.data() + begin, particles.data() + begin + num_contiguous));
    begin += num_contiguous;
  }
}

#endif /* _PARTITION_H_ */
