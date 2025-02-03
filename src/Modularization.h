#ifndef PARATREET_MODULARIZATION_H_
#define PARATREET_MODULARIZATION_H_

#include "paratreet.decl.h"
#include "Node.h"
#include "Utility.h"
#include "SortingInterfaces.h"

class Tree {
public:
  virtual ~Tree() = default;
  virtual int getBranchFactor() = 0;
  virtual void buildCanopy(Key tp_key, int tp_index, std::vector<std::pair<Key, int>>& destinations);
  virtual void prepParticles(IParticleViewer* particles, int depth) {}
  // Returns start + n_particles
  virtual int findChildsLastParticle(const IParticleViewer* particles, int start, int finish, Key child_key, size_t log_branch_factor) = 0;
};

class KdTree : public Tree {
public:
  virtual ~KdTree() = default;
  virtual int getBranchFactor() override {return 2;}
  virtual int findChildsLastParticle(const IParticleViewer* particles, int start, int finish, Key child_key, size_t log_branch_factor) override {
    return (start + finish + 1) / 2; // heavy on the left
  }
  virtual void prepParticles(IParticleViewer* particles, int depth);
};

class LongestDimTree : public Tree {
public:
  virtual ~LongestDimTree() = default;
  virtual int getBranchFactor() override {return 2;}
  virtual int findChildsLastParticle(const IParticleViewer* particles, int start, int finish, Key child_key, size_t log_branch_factor) override {
    return (start + finish + 1) / 2; // heavy on the left
  }
  virtual void prepParticles(IParticleViewer* particles, int depth);
};

class OctTree : public Tree {
public:
  virtual ~OctTree() = default;
  virtual int getBranchFactor() override {return 8;}
  virtual int findChildsLastParticle(const IParticleViewer* particles, int start, int finish, Key child_key, size_t log_branch_factor) override;
};

class BinaryOctTree : public OctTree {
public:
  virtual ~BinaryOctTree() = default;
  virtual int getBranchFactor() override {return 2;}
};

#endif
