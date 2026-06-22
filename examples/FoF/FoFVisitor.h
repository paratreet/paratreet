#ifndef PARATREET_FOFVISITOR_H
#define PARATREET_FOFVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include "CentroidData.h"
#include <cmath>
#include <functional>
#include <vector>
#include <queue>
#include <unordered_set>
#include "unionFindLib.h"
#include "Partition.h"
#include "LocalCalcs.h"

extern CProxy_UnionFindLib libProxy;
extern CProxy_Partition<CentroidData> partitionProxy;
extern Real linkingLength;
extern Vector3D<Real> fPeriod;

struct PairHash {
  std::size_t operator()(const std::pair<uint64_t, uint64_t>& p) const {
    std::size_t h1 = std::hash<uint64_t>{}(p.first);
    std::size_t h2 = std::hash<uint64_t>{}(p.second);
    return h1 ^ (h2 * 0x9e3779b97f4a7c15ULL + (h1 << 6) + (h1 >> 2));
  }
};

class FoFVisitor {

private:
  Vector3D<Real> offset;
  int iter;
  CProxy_LocalCalcs<CentroidData> lc_proxy;
  Real linkSq;
  static constexpr int COMPRESS_THRESHOLD = 10000;
  std::unordered_set<std::pair<uint64_t,uint64_t>, PairHash> dedup_set;

public:
  static constexpr const bool CallSelfLeaf = true;
  FoFVisitor() : offset(0, 0, 0), iter(0), linkSq(linkingLength * linkingLength) {}
  FoFVisitor(Vector3D<Real> offseti, int _iter, CProxy_LocalCalcs<CentroidData> lc)
    : offset(offseti), iter(_iter), lc_proxy(lc), linkSq(linkingLength * linkingLength) {}

  void pup(PUP::er& p) {
    p | offset;
    p | iter;
    p | lc_proxy;
    p | linkSq;
    // dedup_set intentionally not PUP'd (local traversal state, starts empty on each PE)
  }

  // Compute maximum squared distance between any two points in boxes a and b,
  // after shifting box b by `offset`.
  static inline Real aabb_max_distance_sq(const OrientedBox<Real>& a, const OrientedBox<Real>& b, const Vector3D<Real>& offset)
  {
    const Vector3D<Real> bl = b.lesser_corner + offset;
    const Vector3D<Real> bg = b.greater_corner + offset;

    Real dx = std::max(std::abs(a.lesser_corner.x - bg.x), std::abs(a.greater_corner.x - bl.x));
    Real dy = std::max(std::abs(a.lesser_corner.y - bg.y), std::abs(a.greater_corner.y - bl.y));
    Real dz = std::max(std::abs(a.lesser_corner.z - bg.z), std::abs(a.greater_corner.z - bl.z));

    return dx*dx + dy*dy + dz*dz;
  }

  // Compute minimum squared distance between two axis-aligned boxes (OrientedBox)
  // after shifting box b by `offset`. Returns 0 if boxes overlap.
  static inline Real aabb_min_distance_sq(const OrientedBox<Real>& a, const OrientedBox<Real>& b, const Vector3D<Real>& offset)
  {
    // shift b by offset (do not modify original)
    const Vector3D<Real> bl = b.lesser_corner + offset;
    const Vector3D<Real> bg = b.greater_corner + offset;
    Real dx = 0.0, dy = 0.0, dz = 0.0;
    if (bg.x < a.lesser_corner.x) dx = a.lesser_corner.x - bg.x;
    else if (bl.x > a.greater_corner.x) dx = bl.x - a.greater_corner.x;

    if (bg.y < a.lesser_corner.y) dy = a.lesser_corner.y - bg.y;
    else if (bl.y > a.greater_corner.y) dy = bl.y - a.greater_corner.y;

    if (bg.z < a.lesser_corner.z) dz = a.lesser_corner.z - bg.z;
    else if (bl.z > a.greater_corner.z) dz = bl.z - a.greater_corner.z;

    return dx*dx + dy*dy + dz*dz;
  }


  bool open(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    // Cheap conservative reject using box-vs-box minimum distance.
    Real minDistSq = aabb_min_distance_sq(source.data.box, target.data.box, offset);
    if (minDistSq > linkSq) return false;

    // Fallback: if boxes are close, check individual particles (exact test)
    bool may_return = false;
    for (int i = 0; i < target.n_particles; i++) {
      Real ballSq = linkSq;
      // adding offset to target needs to be the same in vertex_range_initialized check
      if (Space::intersect(source.data.box, target.particles()[i].position+offset, ballSq))
      {
        may_return = true;
        break;
      }
    }

    if (!may_return) return false;

    return true;
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void do_union(const Particle& sp, const Particle& tp) {
    fof_union_request_count++;
    if (sp.partition_idx == tp.partition_idx) {
      UnionFindLib* local_lib = libProxy[tp.partition_idx].ckLocal();
      if (local_lib != nullptr) local_lib->union_request(sp.vertex_id, tp.vertex_id);
    } else {
      lc_proxy.ckLocalBranch()->cross_partition_union_count++;
      int target_idx = ((tp.partition_idx < sp.partition_idx) ^ (tp.partition_idx & 1))
                       ? tp.partition_idx : sp.partition_idx;
      UnionFindLib* local_lib = libProxy[target_idx].ckLocal();
      if (local_lib != nullptr) {
        local_lib->union_request(sp.vertex_id, tp.vertex_id);
      }
    }
  }

  void do_union_tips(uint64_t vid1, uint64_t vid2) {
    fof_union_request_count++;
    int pid1 = (int)(vid1 >> 32);
    int pid2 = (int)(vid2 >> 32);
    //assumption: if pid1==pid2, this pe is the owner of both chares
    if (pid1 == pid2) {
      UnionFindLib* local_lib = libProxy[pid1].ckLocal();
      if (local_lib != nullptr) local_lib->union_request(vid1, vid2);
    } else {
      lc_proxy.ckLocalBranch()->cross_partition_union_count++;
      int target_idx = std::min(pid1, pid2);
      UnionFindLib* local_lib = libProxy[target_idx].ckLocal();
      if (local_lib != nullptr) local_lib->union_request(vid1, vid2);
    }
  }

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    LocalCalcs<CentroidData>* lc = lc_proxy.ckLocalBranch();
    if (lc->compress_count < LocalCalcs<CentroidData>::MAX_COMPRESSIONS) {
      bool trigger = (lc->compress_count == 0 && lc->cross_partition_union_count > 0)
                     || (lc->cross_partition_union_count >= COMPRESS_THRESHOLD);
      if (trigger) {
        lc->compressLocal();
        dedup_set.clear();
      }
    }
    const bool all_within = (aabb_max_distance_sq(source.data.box, target.data.box, offset) < linkSq);

    if (all_within && source.n_particles > 0 && target.n_particles > 0) {
      // Every particle pair is within linking length. Build a star around the min vertex_id
      // particle, using local tips to skip redundant union requests.
      const Particle* root = &source.particles()[0];
      for (int j = 1; j < source.n_particles; ++j)
        if (source.particles()[j].vertex_id < root->vertex_id) root = &source.particles()[j];
      for (int i = 0; i < target.n_particles; ++i)
        if (target.particles()[i].vertex_id < root->vertex_id) root = &target.particles()[i];

      bool dummy;
      uint64_t root_tip = lc->localFind(root->vertex_id, dummy);

      for (int j = 0; j < source.n_particles; ++j) {
        const Particle& sp = source.particles()[j];
        if (sp.vertex_id == root->vertex_id) continue;
        uint64_t sp_tip = lc->localFind(sp.vertex_id, dummy);
        if (sp_tip == root_tip) continue;
        auto key = std::make_pair(std::min(root_tip, sp_tip), std::max(root_tip, sp_tip));
        if (!dedup_set.insert(key).second) continue;
        do_union_tips(root_tip, sp_tip);
      }
      for (int i = 0; i < target.n_particles; ++i) {
        const Particle& tp = target.particles()[i];
        if (tp.vertex_id == root->vertex_id) continue;
        uint64_t tp_tip = lc->localFind(tp.vertex_id, dummy);
        if (tp_tip == root_tip) continue;
        auto key = std::make_pair(std::min(root_tip, tp_tip), std::max(root_tip, tp_tip));
        if (!dedup_set.insert(key).second) continue;
        do_union_tips(root_tip, tp_tip);
      }
      return;
    }

    // Early skip: if all particles in both leaves share the same local tip they are
    // already in the same component, so every distance-checked union would be a no-op.
    if (source.n_particles > 0 && target.n_particles > 0) {
      bool dummy;
      uint64_t ref_tip = lc->localFind(source.particles()[0].vertex_id, dummy);
      bool all_same = true;
      for (int j = 1; j < source.n_particles && all_same; ++j)
        if (lc->localFind(source.particles()[j].vertex_id, dummy) != ref_tip) all_same = false;
      for (int i = 0; i < target.n_particles && all_same; ++i)
        if (lc->localFind(target.particles()[i].vertex_id, dummy) != ref_tip) all_same = false;
      if (all_same) return;
    }

    for (int i = 0; i < target.n_particles; ++i) {
      const Particle& tp = target.particles()[i];
      for (int j = 0; j < source.n_particles; ++j) {
        const Particle& sp = source.particles()[j];
        if (sp.vertex_id >= tp.vertex_id) continue;
        const Vector3D<Real> d = tp.position - sp.position + offset;
        const Real distSq = d.x*d.x + d.y*d.y + d.z*d.z;
        if (distSq < linkSq) {
          bool dummy;
          uint64_t sp_tip = lc->localFind(sp.vertex_id, dummy);
          uint64_t tp_tip = lc->localFind(tp.vertex_id, dummy);
          if (sp_tip == tp_tip) continue;
          auto key = std::make_pair(std::min(sp_tip, tp_tip), std::max(sp_tip, tp_tip));
          if (!dedup_set.insert(key).second) continue;
          do_union_tips(sp_tip, tp_tip);
        }
      }
    }
  }
  // Read-only variant of leaf() used by helper threads during the parallel
  // traversal phase.  No parent pointer writes: pairs are appended to `out`
  // for deferred application after CmiNodeBarrier.
  // find_tip: read-only path walk (localNodeFind), no compression.
  // dedup_set is used read-only (find only, no insert) so concurrent helpers
  // reading the source PE's dedup_set simultaneously is safe.
  void leafCollect(const SpatialNode<CentroidData>& source,
                   SpatialNode<CentroidData>& target,
                   std::vector<std::pair<uint64_t,uint64_t>>& out,
                   const std::function<uint64_t(uint64_t)>& find_tip) {
    const bool all_within = (aabb_max_distance_sq(source.data.box, target.data.box, offset) < linkSq);

    if (all_within && source.n_particles > 0 && target.n_particles > 0) {
      const Particle* root = &source.particles()[0];
      for (int j = 1; j < source.n_particles; ++j)
        if (source.particles()[j].vertex_id < root->vertex_id) root = &source.particles()[j];
      for (int i = 0; i < target.n_particles; ++i)
        if (target.particles()[i].vertex_id < root->vertex_id) root = &target.particles()[i];

      uint64_t root_tip = find_tip(root->vertex_id);

      for (int j = 0; j < source.n_particles; ++j) {
        const Particle& sp = source.particles()[j];
        if (sp.vertex_id == root->vertex_id) continue;
        uint64_t sp_tip = find_tip(sp.vertex_id);
        if (sp_tip == root_tip) continue;
        auto key = std::make_pair(std::min(root_tip, sp_tip), std::max(root_tip, sp_tip));
        if (dedup_set.find(key) != dedup_set.end()) continue;
        out.push_back({root_tip, sp_tip});
      }
      for (int i = 0; i < target.n_particles; ++i) {
        const Particle& tp = target.particles()[i];
        if (tp.vertex_id == root->vertex_id) continue;
        uint64_t tp_tip = find_tip(tp.vertex_id);
        if (tp_tip == root_tip) continue;
        auto key = std::make_pair(std::min(root_tip, tp_tip), std::max(root_tip, tp_tip));
        if (dedup_set.find(key) != dedup_set.end()) continue;
        out.push_back({root_tip, tp_tip});
      }
      return;
    }

    if (source.n_particles > 0 && target.n_particles > 0) {
      uint64_t ref_tip = find_tip(source.particles()[0].vertex_id);
      bool all_same = true;
      for (int j = 1; j < source.n_particles && all_same; ++j)
        if (find_tip(source.particles()[j].vertex_id) != ref_tip) all_same = false;
      for (int i = 0; i < target.n_particles && all_same; ++i)
        if (find_tip(target.particles()[i].vertex_id) != ref_tip) all_same = false;
      if (all_same) return;
    }

    for (int i = 0; i < target.n_particles; ++i) {
      const Particle& tp = target.particles()[i];
      for (int j = 0; j < source.n_particles; ++j) {
        const Particle& sp = source.particles()[j];
        if (sp.vertex_id >= tp.vertex_id) continue;
        const Vector3D<Real> d = tp.position - sp.position + offset;
        const Real distSq = d.x*d.x + d.y*d.y + d.z*d.z;
        if (distSq < linkSq) {
          uint64_t sp_tip = find_tip(sp.vertex_id);
          auto key = std::make_pair(sp_tip, tp.vertex_id);
          if (dedup_set.find(key) != dedup_set.end()) continue;
          out.push_back({sp_tip, tp.vertex_id});
        }
      }
    }
  }
};

#endif // PARATREET_FOFVISITOR_H_
