#ifndef PARATREET_FOFVISITOR_H
#define PARATREET_FOFVISITOR_H_

#include "paratreet.decl.h"
#include "common.h"
#include "Space.h"
#include "CentroidData.h"
#include <cmath>
#include <vector>
#include <queue>
#include "unionFindLib.h"
#include "Partition.h"

extern CProxy_UnionFindLib libProxy;
extern CProxy_Partition<CentroidData> partitionProxy;
extern Real linkingLength;
extern Vector3D<Real> fPeriod;

class FoFVisitor {

private:
  Vector3D<Real> offset;
public:
  static constexpr const bool CallSelfLeaf = true;
  FoFVisitor() : offset(0, 0, 0) {}
  FoFVisitor(Vector3D<Real> offseti) : offset(offseti) {}

  void pup(PUP::er& p) {
    p | offset;
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
    const Real linkSq = linkingLength * linkingLength;
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
      */
    }
    
    return true;
  }

  void node(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {}

  void leaf(const SpatialNode<CentroidData>& source, SpatialNode<CentroidData>& target) {
    //int counter = 0;
    const Real linkSq = linkingLength * linkingLength;
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
          //counter++;
          libProxy[tp.partition_idx].union_request(sp.vertex_id, tp.vertex_id);
        }
      }
    }
  }
};

#endif // PARATREET_FOFVISITOR_H_
