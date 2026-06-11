#ifndef PARATREET_FOFVISITOR_H
#define PARATREET_FOFVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include "CentroidData.h"
#include <cmath>
#include <vector>
#include <queue>
#include <unordered_set>
#include "unionFindLib.h"
#include "Partition.h"

extern CProxy_UnionFindLib libProxy;
extern CProxy_Partition<CentroidData> partitionProxy;
extern Real linkingLength;
extern Vector3D<Real> fPeriod;

class FoFVisitor {

  struct PairHash {
    size_t operator()(const std::pair<uint64_t, uint64_t>& p) const {
      size_t h = std::hash<uint64_t>{}(p.first);
      h ^= std::hash<uint64_t>{}(p.second) + 0x9e3779b9 + (h << 6) + (h >> 2);
      return h;
    }
  };

private:
  Vector3D<Real> offset;
  int iter; // current paratreet iteration
  Real linkSq;
  std::unordered_set<std::pair<uint64_t, uint64_t>, PairHash> seen_edges;
public:
  static constexpr const bool CallSelfLeaf = true;
  FoFVisitor() : offset(0, 0, 0), iter(0), linkSq(linkingLength * linkingLength) {}
  FoFVisitor(Vector3D<Real> offseti) : offset(offseti), iter(0), linkSq(linkingLength * linkingLength) {}
  FoFVisitor(Vector3D<Real> offseti, int _iter) : offset(offseti), iter(_iter), linkSq(linkingLength * linkingLength) {}

  void pup(PUP::er& p) {
    p | offset;
    p | iter;
    p | linkSq;
    // seen_edges is local traversal state — intentionally not PUP'd
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

    //old logic for box box reject
    /*
    Real r_bucket = target.data.size_sm + linkingLength;
    if (!Space::intersect(source.data.box, target.data.box.center()+offset, r_bucket*r_bucket))
      return false;
    */

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
    
    // Vertex ID range optimization: we only process pairs where sp.vertex_id < tp.vertex_id
    // If both nodes have initialized vertex ranges, check for potential early termination
    /*
    if (source.vertex_range_initialized && target.vertex_range_initialized) {
      bool should_skip = false;
      
      // If all source particles have vertex_id >= all target particles, skip this interaction
      // (no valid sp.vertex_id < tp.vertex_id pairs possible)
      if (source.particle_min_index >= target.particle_max_index) {
        should_skip = true;
      }
      
      if (should_skip) {
        return false;
      }
      
      /* DEBUG: Uncomment to enable optimization statistics
      // Debug: track optimization attempts
      static int total_checks = 0;
      static int skipped_interactions = 0;
      static int overlapping_ranges = 0;
      total_checks++;
      
      // Count overlapping ranges for statistics
      if (!(source.particle_min_index >= target.particle_max_index || 
            target.particle_max_index <= source.particle_min_index)) {
        overlapping_ranges++;
      }
      
      if (should_skip) {
        skipped_interactions++;
      }
      
      if (total_checks % 10000 == 0) {
        CkPrintf("FoF Stats: %d checks, %d skipped (%.2f%%), %d overlapping (%.2f%%)\n", 
                 total_checks, skipped_interactions, 
                 100.0 * skipped_interactions / total_checks,
                 overlapping_ranges, 100.0 * overlapping_ranges / total_checks);
        CkPrintf("  Example ranges: source[%lu,%lu] target[%lu,%lu]\n",
                 source.particle_min_index, source.particle_max_index,
                 target.particle_min_index, target.particle_max_index);
      }
    }
      */
    
    return true;
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void do_union(const Particle& sp, const Particle& tp) {
    if (sp.partition_idx == tp.partition_idx) {
      // intra-partition pair: only process in iter=1
      //if (iter == 1) {
        UnionFindLib* local_lib = libProxy[tp.partition_idx].ckLocal();
        if (local_lib != nullptr) {
          fof_union_request_count++;
          local_lib->union_request(sp.vertex_id, tp.vertex_id);
        }
      //}
    } else {
      // cross-partition pair: deduplicate via seen_edges before sending remote request
      uint64_t sp_root = sp.vertex_id;
      UnionFindLib* sp_lib = libProxy[sp.partition_idx].ckLocal();
      if (sp_lib != nullptr) sp_root = sp_lib->get_parent(sp.vertex_id);

      auto edge = std::make_pair(sp_root, tp.vertex_id);
      if (!seen_edges.insert(edge).second) return;

      //if (iter == 2) {
        int target_idx = ((tp.partition_idx < sp.partition_idx) ^ (tp.partition_idx & 1))
                         ? tp.partition_idx : sp.partition_idx;
        UnionFindLib* local_lib = libProxy[target_idx].ckLocal();
        if (local_lib != nullptr) {
          fof_union_request_count++;
          local_lib->union_request(sp.vertex_id, tp.vertex_id);
        } else {
          //libProxy[target_idx].union_request(sp.vertex_id, tp.vertex_id);
        }
      //}
    }
  }

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    const bool all_within = (aabb_max_distance_sq(source.data.box, target.data.box, offset) < linkSq);

    if (all_within && source.n_particles > 0 && target.n_particles > 0) {
      // Every particle pair is within linking length. Instead of O(N*M) unions,
      // build a star: find the particle with minimum vertex_id across both buckets
      // and union all others to it. This gives O(N+M) unions (a spanning tree).

      if (source.vertex_range_initialized && target.vertex_range_initialized
          && source.particle_min_index > target.particle_min_index) {
        return; // the symmetric leaf(target, source) call handles this pair
      }

      const Particle* root = &source.particles()[0];
      for (int j = 1; j < source.n_particles; ++j)
        if (source.particles()[j].vertex_id < root->vertex_id) root = &source.particles()[j];
      for (int i = 0; i < target.n_particles; ++i)
        if (target.particles()[i].vertex_id < root->vertex_id) root = &target.particles()[i];

      for (int j = 0; j < source.n_particles; ++j) {
        const Particle& sp = source.particles()[j];
        if (sp.vertex_id == root->vertex_id) continue;
        do_union(*root, sp);
      }
      const bool is_self_leaf = source.vertex_range_initialized && target.vertex_range_initialized
          && source.particle_min_index == target.particle_min_index
          && source.n_particles == target.n_particles;
      if (!is_self_leaf) {
        for (int i = 0; i < target.n_particles; ++i) {
          const Particle& tp = target.particles()[i];
          if (tp.vertex_id == root->vertex_id) continue;
          do_union(*root, tp);
        }
      }
    return;
    }
      for (int i = 0; i < target.n_particles; ++i) {
        const Particle& tp = target.particles()[i];
        for (int j = 0; j < source.n_particles; ++j) {
          const Particle& sp = source.particles()[j];
          // avoid union of same pair twice by comparing vertex_id instead of order
          // This should be more effective since vertex_id has spatial locality
          if (sp.vertex_id >= tp.vertex_id) continue;
          // squared distance (avoid sqrt)
          const Vector3D<Real> d = tp.position - sp.position + offset;
          const Real distSq = d.x*d.x + d.y*d.y + d.z*d.z;
          if (distSq < linkSq) {
            do_union(sp, tp);
          }
        }
      }
    }

};

#endif // PARATREET_FOFVISITOR_H_
