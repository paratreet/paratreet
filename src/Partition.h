#ifndef _PARTITION_H_
#define _PARTITION_H_

#include <vector>

#include "Particle.h"
#include "Traverser.h"
#include "ParticleMsg.h"
#include "NodeWrapper.h"
#include "paratreet.decl.h"

extern CProxy_TreeSpec treespec;
extern CProxy_Reader readers;

template <typename Data>
struct Partition : public CBase_Partition<Data> {
  std::vector<Particle> received_particles;
  std::vector<Particle> particles;
  std::vector<Node<Data>*> leaves;

  Traverser<Data> *traverser;
  int n_partitions;

  int received_part_index = 0;

  // filled in during traversal
  std::vector<std::vector<Node<Data>*>> interactions;

  CProxy_TreeCanopy<Data> tc_proxy;
  CProxy_CacheManager<Data> cm_proxy;
  CacheManager<Data> *cm_local;
  CProxy_Resumer<Data> r_proxy;
  Resumer<Data>* r_local;

  Partition(int, CProxy_CacheManager<Data>, CProxy_Resumer<Data>, TCHolder<Data>);

  template<typename Visitor> void startDown();
  void goDown(Key);
  void interact(const CkCallback& cb);

  void receiveLeaves(std::vector<NodeWrapper<Data>>, std::vector<Key>, int, size_t);
  void receive(ParticleMsg*);
  void destroy();
  void reset();
  void perturb(Real, bool);
  void flush(CProxy_Reader);
  void output(CProxy_Writer w, CkCallback cb);
  void initLocalBranches();
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
  received_part_index = 0;
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
  received_part_index = 0; // reset
  initLocalBranches();
  if (received_particles.size() != particles.size()) {
    CkAbort("Partition did not receive all of its leaves from Subtrees");
  }
  received_particles.clear();
  interactions.resize(leaves.size());
  traverser = new DownTraverser<Data, Visitor>(*this);
  traverser->start();
}

template <typename Data>
void Partition<Data>::goDown(Key new_key)
{
  traverser->resumeTrav(new_key);
}

template <typename Data>
void Partition<Data>::interact(const CkCallback& cb)
{
  traverser->interact();
  this->contribute(cb);
}

template <typename Data>
void Partition<Data>::receiveLeaves(
  std::vector<NodeWrapper<Data>> data, std::vector<Key> all_particle_keys,
  int subtree_idx, size_t branch_factor
  )
{
  for (int i = 0; i < all_particle_keys.size(); i++) {
    bool found = false;
    for (int j = received_part_index + i; j < particles.size(); j++) {
      if (particles[j].key == all_particle_keys[i]) {
        std::swap(particles[received_part_index + i], particles[j]);
        found = true;
        break;
      }
    }
    if (!found) CkAbort("couldnt find particle key");
  }
  for (const NodeWrapper<Data>& leaf : data) {
    Node<Data> *node = treespec.ckLocalBranch()->template makeNode<Data>(
      leaf.key, leaf.depth, leaf.n_particles, &particles[received_part_index],
      subtree_idx, subtree_idx, leaf.is_leaf, nullptr, subtree_idx
      );
    received_part_index += leaf.n_particles;
    node->type = Node<Data>::Type::Leaf;
    node->data = Data(node->particles(), node->n_particles);
    leaves.push_back(node);
  }
}

template <typename Data>
void Partition<Data>::receive(ParticleMsg *msg)
{
  received_particles.insert(received_particles.end(),
                   msg->particles, msg->particles + msg->n_particles);
  delete msg;
  std::sort(received_particles.begin(), received_particles.end());
}

template <typename Data>
void Partition<Data>::destroy()
{
  this->thisProxy[this->thisIndex].ckDestroy();
}

template <typename Data>
void Partition<Data>::reset()
{
  leaves.clear();
  interactions.clear();
  particles.clear();
}

template <typename Data>
void Partition<Data>::perturb(Real timestep, bool if_flush)
{
  // TODO: how do we decide when to send particles to other partitions?
  for (auto& p : particles)
    p.perturb(timestep, readers.ckLocalBranch()->universe.box);
  if (if_flush)
    flush(readers);
}

template <typename Data>
void Partition<Data>::flush(CProxy_Reader readers)
{
  ParticleMsg *msg = new (particles.size()) ParticleMsg(
    particles.data(), particles.size()
    );
  readers[CkMyPe()].receive(msg);
}

template <typename Data>
void Partition<Data>::output(CProxy_Writer w, CkCallback cb)
{
  std::vector<Particle> particles = this->particles;

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
