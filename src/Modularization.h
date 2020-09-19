#ifndef PARATREET_MODULARIZATION_H_
#define PARATREET_MODULARIZATION_H_

#include "paratreet.decl.h"
#include "Node.h"
#include "Utility.h"

class Tree {
public:
  virtual ~Tree() = default;
  virtual int getBranchFactor() = 0;
  virtual void buildCanopy(int tp_index, const SendProxyFn &fn) = 0;
  virtual void prepParticles(Particle* particles, int n_particles, int depth) {}
  virtual int findChildsLastParticle(Particle* particles, Key child_key, int start, int finish, size_t log_branch_factor) = 0;
  // Find number of particles in child
};

class OctTree : public Tree {
public:
  virtual ~OctTree() = default;
  virtual int getBranchFactor() override {return 8;}
  virtual void buildCanopy(int tp_index, const SendProxyFn &fn) override;
  virtual int findChildsLastParticle(Particle* particles, Key child_key, int start, int finish, size_t log_branch_factor) override {
    Key sibling_splitter = Utility::removeLeadingZeros(child_key + 1, log_branch_factor);
    return Utility::binarySearchGE(sibling_splitter, particles, start, finish);
  }
};

class BinaryTree : public OctTree {
public:
  virtual ~BinaryTree() = default;
  virtual int getBranchFactor() override {return 2;}
};

class KdTree : public BinaryTree {
public:
  virtual ~KdTree() = default;
  virtual void prepParticles(Particle* particles, int n_particles, int depth) override {
    const size_t dim = depth % 3; // dim count
    static auto compX = [] (const Particle& a, const Particle& b) {return a.position.x < b.position.x;};
    static auto compY = [] (const Particle& a, const Particle& b) {return a.position.y < b.position.y;};
    static auto compZ = [] (const Particle& a, const Particle& b) {return a.position.z < b.position.z;};
    if (dim == 0)      std::sort(particles, particles + n_particles, compX);
    else if (dim == 1) std::sort(particles, particles + n_particles, compY);
    else if (dim == 2) std::sort(particles, particles + n_particles, compZ);
  }
  virtual int findChildsLastParticle(Particle* particles, Key child_key, int start, int finish, size_t log_branch_factor) override {
    // something like this? doesnt work
    // we cant just do the median to start because that only applies once were at tp_key level
    // but when we start out we need to put the right particles in the right bucket
    // probably something like this but doesnt work
    // you can tell it doesnt work because the sum # of particles in leaves is << # of global particles
    // similarly, the number of particles in leaves is less than # of particles assigned to the treepiece
    Key splitter = Utility::removeLeadingZeros(child_key, log_branch_factor);
    Key sibling_splitter = Utility::removeLeadingZeros(child_key + 1, log_branch_factor);
    if (particles[finish-1].key > sibling_splitter) {
        return start;
    }
    if (particles[start].key < splitter) {
      return finish;
    }
    return (start + finish + 1) / 2;
  }
};

#endif
