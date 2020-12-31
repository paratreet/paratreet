#ifndef _PARTITION_H_
#define _PARTITION_H_

#include <vector>

#include "Particle.h"
#include "Traverser.h"
#include "ParticleMsg.h"
#include "NodeWrapper.h"
#include "MultiData.h"
#include "paratreet.decl.h"

extern CProxy_TreeSpec treespec;
extern CProxy_TreeSpec treespec_subtrees;
extern CProxy_Reader readers;

namespace paratreet {
  extern void perLeafFn(SpatialNode<CentroidData>&);
}

template <typename Data>
struct Partition : public CBase_Partition<Data> {
  std::vector<Node<Data>*> leaves;
  std::vector<Node<Data>*> leaves_owned; // subset of leaves
  std::vector<size_t> locations;

  std::unique_ptr<Traverser<Data>> traverser;
  int n_partitions;

  // filled in during traversal
  std::vector<std::vector<Node<Data>*>> interactions;

  CProxy_TreeCanopy<Data> tc_proxy;
  CProxy_CacheManager<Data> cm_proxy;
  CacheManager<Data> *cm_local;
  CProxy_Resumer<Data> r_proxy;
  Resumer<Data>* r_local;

  Partition(int, CProxy_CacheManager<Data>, CProxy_Resumer<Data>, TCHolder<Data>);

  template<typename Visitor> void startDown();
  template<typename Visitor> void startUpAndDown();
  void goDown();
  void interact(const CkCallback& cb);

  void receiveLeaves(std::vector<NodeWrapper>, std::vector<Key>, int);
  void receiveLeavesAndSubtree(MultiData<Data>, std::vector<NodeWrapper>, std::vector<Key>, int);
  void destroy();
  void reset();
  void perturb(TPHolder<Data>, Real, bool);
  void output(CProxy_Writer w, CkCallback cb);
  void callPerLeafFn(const CkCallback& cb);
  void pup(PUP::er& p);

private:
  void initLocalBranches();
  void copyParticles(std::vector<Particle>& particles);
  void flush(CProxy_Reader, std::vector<Particle>&);
};

template <typename Data>
Partition<Data>::Partition(
  int np, CProxy_CacheManager<Data> cm,
  CProxy_Resumer<Data> rp, TCHolder<Data> tc_holder
  )
{
  n_partitions = np;
  tc_proxy = tc_holder.proxy;
  r_proxy = rp;
  cm_proxy = cm;
  initLocalBranches();
}

template <typename Data>
void Partition<Data>::initLocalBranches() {
  r_local = r_proxy.ckLocalBranch();
  r_local->part_proxy = this->thisProxy;
  r_local->resume_nodes_per_part.resize(n_partitions);
  cm_local = cm_proxy.ckLocalBranch();
  r_local->cm_local = cm_local;
  cm_local->r_proxy = r_proxy;
}

template <typename Data>
template <typename Visitor>
void Partition<Data>::startDown()
{
  initLocalBranches();
  interactions.resize(leaves.size());
  traverser.reset(new DownTraverser<Data, Visitor>(leaves, *this));
  traverser->start();
}

template <typename Data>
template <typename Visitor>
void Partition<Data>::startUpAndDown()
{
  initLocalBranches();
  interactions.resize(leaves.size());
  traverser.reset(new UpnDTraverser<Data, Visitor>(*this));
  traverser->start();
}

template <typename Data>
void Partition<Data>::goDown()
{
  traverser->resumeTrav();
}

template <typename Data>
void Partition<Data>::interact(const CkCallback& cb)
{
  traverser->interact();
  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::receiveLeaves(std::vector<NodeWrapper> data, std::vector<Key> lookup_leaf_keys, int subtree_idx) {
  for (auto && leaf : data) {
    auto particles = new Particle [leaf.particles.size()];
    std::copy(leaf.particles.begin(), leaf.particles.end(), particles);
    auto node = treespec_subtrees.ckLocalBranch()->template makeNode<Data>(
      leaf.key, leaf.depth, leaf.particles.size(), particles,
      subtree_idx, subtree_idx, true, nullptr, subtree_idx
      );
    node->type = Node<Data>::Type::Leaf;
    node->data = Data(node->particles(), node->n_particles);
    leaves_owned.push_back(node);
    leaves.push_back(node);
    locations.push_back(subtree_idx);
  }
  for (auto && leaf_key : lookup_leaf_keys) {
    auto && leaf_lookup = cm_proxy.ckLocalBranch()->leaf_lookup;
    auto it = leaf_lookup.find(leaf_key);
    if (it == leaf_lookup.end()) CkAbort("couldnt find leaf key");
    leaves.push_back(it->second);
    locations.push_back(subtree_idx);
  }
  cm_local->num_buckets += lookup_leaf_keys.size() + data.size();
}

template <typename Data>
void Partition<Data>::receiveLeavesAndSubtree(MultiData<Data> multidata, std::vector<NodeWrapper> leaves, std::vector<Key> lookup_leaf_keys, int subtree_idx) {
  auto && local_tps = cm_proxy.ckLocalBranch()->local_tps;
  if (local_tps.find(multidata.nodes[0].first) == local_tps.end()) {
    cm_proxy.ckLocalBranch()->addSubtree(multidata);
  }
  receiveLeaves(leaves, lookup_leaf_keys, subtree_idx);
}

template <typename Data>
void Partition<Data>::destroy()
{
  reset();
  this->thisProxy[this->thisIndex].ckDestroy();
}

template <typename Data>
void Partition<Data>::reset()
{
  traverser.reset();
  for (auto && leaf : leaves_owned) {
    leaf->freeParticles();
    delete leaf;
    leaf = nullptr;
  }
  leaves_owned.clear();
  leaves.clear();
  locations.clear();
  interactions.clear();
}

template <typename Data>
void Partition<Data>::pup(PUP::er& p)
{
  p | n_partitions;
  p | tc_proxy;
  p | cm_proxy;
  p | r_proxy;
  if (p.isUnpacking()) {
    initLocalBranches();
  }
}

template <typename Data>
void Partition<Data>::perturb(TPHolder<Data> tp_holder, Real timestep, bool if_flush)
{
  std::vector<Particle> particles;
  copyParticles(particles);
  for (auto && p : particles) {
    p.perturb(timestep, readers.ckLocalBranch()->universe.box);
  }

  if (if_flush) {
    flush(readers, particles);
  }
  else {
    auto sendParticles = [&](int dest, int n_particles, Particle* particles) {
      ParticleMsg* msg = new (n_particles) ParticleMsg(particles, n_particles);
      tp_holder.proxy[dest].receive(msg);
    };
    treespec_subtrees.ckLocalBranch()->getDecomposition()->flush(particles, sendParticles);
  }
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
void Partition<Data>::callPerLeafFn(const CkCallback& cb)
{
  for (auto && leaf : leaves) {
    paratreet::perLeafFn(*leaf);
  }
  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::copyParticles(std::vector<Particle>& particles) {
  for (auto && leaf : leaves) {
    particles.insert(particles.end(), leaf->particles(), leaf->particles() + leaf->n_particles);
  }
}

template <typename Data>
void Partition<Data>::output(CProxy_Writer w, CkCallback cb)
{
  std::vector<Particle> particles;
  copyParticles(particles);

  std::sort(particles.begin(), particles.end(),
            [](const Particle& left, const Particle& right) {
              return left.order < right.order;
            });

  int n_total_particles = readers.ckLocalBranch()->universe.n_particles;
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

  if (this->thisIndex != n_partitions - 1)
    this->thisProxy[this->thisIndex + 1].output(w, cb);
}

#endif /* _PARTITION_H_ */
