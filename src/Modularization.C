#include "Modularization.h"
#include "TreeSpec.h"

extern CProxy_TreeSpec treespec;

namespace {
void partialSort(IParticleViewer* particles, int dim) {
  static auto compX = [] (const Vector3D<Real>& a, const Vector3D<Real>& b) {return a.x < b.x;};
  static auto compY = [] (const Vector3D<Real>& a, const Vector3D<Real>& b) {return a.y < b.y;};
  static auto compZ = [] (const Vector3D<Real>& a, const Vector3D<Real>& b) {return a.z < b.z;};
  int split_idx = (particles->size() + 1) / 2;
  if (dim == 0) {
    particles->nthElementByPosition(split_idx, compX);
  }
  else if (dim == 1) {
    particles->nthElementByPosition(split_idx, compY);
  }
  else if (dim == 2) {
    particles->nthElementByPosition(split_idx, compZ);
  }
}
}

void Tree::buildCanopy(Key tp_key, int tp_index, std::vector<std::pair<Key, int>>& destinations) {
    Key temp_key = tp_key;
    destinations.emplace_back(tp_key, tp_index);
    while (temp_key > 0 && temp_key % getBranchFactor() == 0) {
        temp_key /= getBranchFactor();
	destinations.emplace_back(temp_key, -1);
    }
}

void LongestDimTree::prepParticles(IParticleViewer* particles, int depth) {
  OrientedBox<Real> box;
  Vector3D<Real> unweighted_center;
  for (int i = 0; i < particles->size(); i++) {
    box.grow(particles->position(i));
    unweighted_center += particles->position(i);
  }
  unweighted_center /= particles->size();
  auto dims = box.greater_corner - box.lesser_corner;
  int best_dim = 0;
  Real max_dim = 0;
  for (int d = 0; d < NDIM; d++) {
    if (dims[d] > max_dim) {
      max_dim = dims[d];
      best_dim = d;
    }
  }
  partialSort(particles, best_dim);
}

void KdTree::prepParticles(IParticleViewer* particles, int depth) {
  // sort by key
  int dim = depth % NDIM;
  partialSort(particles, dim);
}

int OctTree::findChildsLastParticle(const IParticleViewer* particles, int start, int finish, Key child_key, size_t log_branch_factor) {
  Key sibling_splitter = Utility::removeLeadingZeros(child_key + 1, log_branch_factor);
  std::function<bool(const IParticleViewer*, int, Key)> compGE = [] (const IParticleViewer* particles, int a, Key b) {return particles->key(a) >= b;};
  // Find number of particles in child
  return Utility::binarySearchComp(sibling_splitter, particles, start, finish, compGE);
}
