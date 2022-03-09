#include "Modularization.h"
#include "TreeSpec.h"

extern CProxy_TreeSpec treespec;

namespace {
void partialSort(Particle* particles, int n_particles, int dim) {
  static auto compX = [] (const Particle& a, const Particle& b) {return a.position.x < b.position.x;};
  static auto compY = [] (const Particle& a, const Particle& b) {return a.position.y < b.position.y;};
  static auto compZ = [] (const Particle& a, const Particle& b) {return a.position.z < b.position.z;};
  int split_idx = (n_particles + 1) / 2;
  if (dim == 0) {
    std::nth_element(particles, particles + split_idx, particles + n_particles, compX);
  }
  else if (dim == 1) {
    std::nth_element(particles, particles + split_idx, particles + n_particles, compY);
  }
  else if (dim == 2) {
    std::nth_element(particles, particles + split_idx, particles + n_particles, compZ);
  }
}
}

void Tree::buildCanopy(int tp_index, const SendProxyFn &fn) {
    Key tp_key = treespec.ckLocalBranch()->getSubtreeDecomposition()->getTpKey(tp_index);
    Key temp_key = tp_key;
    fn(tp_key, tp_index);
    while (temp_key > 0 && temp_key % getBranchFactor() == 0) {
        temp_key /= getBranchFactor();
        fn(temp_key, -1);
    }
}

void LongestDimTree::prepParticles(Particle* particles, size_t n_particles, int depth) {
  OrientedBox<Real> box;
  Vector3D<Real> unweighted_center;
  for (int i = 0; i < n_particles; i++) {
    box.grow(particles[i].position);
    unweighted_center += particles[i].position;
  }
  unweighted_center /= n_particles;
  auto dims = box.greater_corner - box.lesser_corner;
  int best_dim = 0;
  Real max_dim = 0;
  for (int d = 0; d < NDIM; d++) {
    if (dims[d] > max_dim) {
      max_dim = dims[d];
      best_dim = d;
    }
  }
  partialSort(particles, n_particles, best_dim);
}

void KdTree::prepParticles(Particle* particles, size_t n_particles, int depth) {
  // sort by key
  int dim = depth % NDIM;
  partialSort(particles, n_particles, dim);
}

int OctTree::findChildsLastParticle(const Particle* particles, int start, int finish, Key child_key, size_t log_branch_factor) {
  Key sibling_splitter = Utility::removeLeadingZeros(child_key + 1, log_branch_factor);
  std::function<bool(const Particle&, Key)> compGE = [] (const Particle& a, Key b) {return a.key >= b;};
  // Find number of particles in child
  return Utility::binarySearchComp(sibling_splitter, particles, start, finish, compGE);
}
