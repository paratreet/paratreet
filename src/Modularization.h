#ifndef PARATREET_MODULARIZATION_H_
#define PARATREET_MODULARIZATION_H_

#include "paratreet.decl.h"
#include "Node.h"
#include "Utility.h"

class Tree {
public:
  virtual ~Tree() = default;
  virtual int getBranchFactor() = 0;
  virtual void buildCanopy(int tp_index, const SendProxyFn &fn);
  virtual void prepParticles(Particle* particles, size_t n_particles, Key parent_key, size_t log_branch_factor) {};
  virtual int findChildsLastParticle(const Particle* particles, int start, int finish, Key child_key, size_t log_branch_factor) = 0;
};

class KdTree : public Tree {
public:
  virtual ~KdTree() = default;
  virtual int getBranchFactor() override {return 2;}
  virtual int findChildsLastParticle(const Particle* particles, int start, int finish, Key child_key, size_t log_branch_factor) override {
    return (start + finish) / 2; // might be +1
  }
  virtual void prepParticles(Particle* particles, size_t n_particles, Key parent_key, size_t log_branch_factor) override {
    // sort by key
    auto depth = Utility::getDepthFromKey(parent_key, log_branch_factor);
    auto dim = depth % UNIVERSE_NDIM;
    auto comp = [dim] (const Particle& a, const Particle& b) {return a.position[dim] < b.position[dim];};
    std::sort(particles, particles + n_particles, comp);
  };
};


class OctTree : public Tree {
public:
  virtual ~OctTree() = default;
  virtual int getBranchFactor() override {
    return 8;
  }

  // Returns start + n_particles
  virtual int findChildsLastParticle(const Particle* particles, int start, int finish, Key child_key, size_t log_branch_factor) override {
    Key sibling_splitter = Utility::removeLeadingZeros(child_key + 1, log_branch_factor);
    std::function<bool(const Particle&, Key)> compGE = [] (const Particle& a, Key b) {return a.key >= b;};
    // Find number of particles in child
    return Utility::binarySearchComp(sibling_splitter, particles, start, finish, compGE);
  }
};

class BinaryTree : public OctTree {
public:
  virtual ~BinaryTree() = default;
  virtual int getBranchFactor() override {
    return 2;
  }
};

#endif
