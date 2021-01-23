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
  // Returns start + n_particles
  virtual int findChildsLastParticle(const Particle* particles, int start, int finish, Key child_key, size_t log_branch_factor) = 0;
};

class KdTree : public Tree {
public:
  virtual ~KdTree() = default;
  virtual int getBranchFactor() override {return 2;}
  virtual int findChildsLastParticle(const Particle* particles, int start, int finish, Key child_key, size_t log_branch_factor) override {
    return (start + finish + 1) / 2; // heavy on the left
  }
  virtual void prepParticles(Particle* particles, size_t n_particles, Key parent_key, size_t log_branch_factor) override;
};

class OctTree : public Tree {
public:
  virtual ~OctTree() = default;
  virtual int getBranchFactor() override {return 8;}
  virtual int findChildsLastParticle(const Particle* particles, int start, int finish, Key child_key, size_t log_branch_factor) override;
};

class BinaryTree : public OctTree {
public:
  virtual ~BinaryTree() = default;
  virtual int getBranchFactor() override {return 2;}
};

class CoMTree : public Tree {
public:
  virtual ~CoMTree() = default;
  virtual int getBranchFactor() override {return 8;}
  virtual int findChildsLastParticle(const Particle* particles, int start, int finish, Key child_key, size_t log_branch_factor) override;
  virtual void prepParticles(Particle* particles, size_t n_particles, Key parent_key, size_t log_branch_factor) override;
};


#endif
