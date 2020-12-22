#include "Modularization.h"
#include "TreeSpec.h"

extern CProxy_TreeSpec treespec_subtrees;

void Tree::buildCanopy(int tp_index, const SendProxyFn &fn) {
    Key tp_key = treespec_subtrees.ckLocalBranch()->getDecomposition()->getTpKey(tp_index);
    Key temp_key = tp_key;
    fn(tp_key, tp_index);
    while (temp_key > 0 && temp_key % getBranchFactor() == 0) {
        temp_key /= getBranchFactor();
        fn(temp_key, -1);
    }
}

void KdTree::prepParticles(Particle* particles, size_t n_particles, Key parent_key, size_t log_branch_factor) {
  // sort by key
  int depth = Utility::getDepthFromKey(parent_key, log_branch_factor);
  int dim = depth % NDIM;
  static auto compX = [] (const Particle& a, const Particle& b) {return a.position.x < b.position.x;};
  static auto compY = [] (const Particle& a, const Particle& b) {return a.position.y < b.position.y;};
  static auto compZ = [] (const Particle& a, const Particle& b) {return a.position.z < b.position.z;};
  if (dim == 0)      std::sort(particles, particles + n_particles, compX);
  else if (dim == 1) std::sort(particles, particles + n_particles, compY);
  else if (dim == 2) std::sort(particles, particles + n_particles, compZ);
}

int OctTree::findChildsLastParticle(const Particle* particles, int start, int finish, Key child_key, size_t log_branch_factor) {
  Key sibling_splitter = Utility::removeLeadingZeros(child_key + 1, log_branch_factor);
  std::function<bool(const Particle&, Key)> compGE = [] (const Particle& a, Key b) {return a.key >= b;};
  // Find number of particles in child
  return Utility::binarySearchComp(sibling_splitter, particles, start, finish, compGE);
}
