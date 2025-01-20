#ifndef ASTRO_CENTROIDDATA_H_
#define ASTRO_CENTROIDDATA_H_

#include "common.h"
#include <vector>
#include <queue>
#include "Particle.h"
#include "BoundingBox.h"
#include "MultipoleMoments.h"
#include "Paratreet.h"

struct CentroidData {
  using Particle = ::Particle;
  using BoundingBox = ::BoundingBox;
  static void loadParticlesFromFile(int reader_index, int n_readers, const paratreet::Configuration& config, std::vector<Particle>& particles);
  static void addParticleToBox(const Particle& p, BoundingBox& box);
  static void adjustParticleForUniverse(Particle& p, const BoundingBox& box);
  static void outputToFile(int writer_index, int particle_index, const BoundingBox& box, int iter, const std::string& output_file, const std::vector<Particle>& particles, int indicator);

  MultipoleMoments multipoles;
  OrientedBox<Real> box;
  int count = 0;
  Real size_sm = 0;
  Real max_rad = 0.0;

  CentroidData() = default;
  /// Construct centroid from particles.
  CentroidData(const Particle* particles, int n_particles, int depth) : CentroidData() {
    for (int i = 0; i < n_particles; i++) {
      multipoles += particles[i];
      box.grow(particles[i].position);
    }
    size_sm = 0.5*(box.size()).length();
    count = n_particles;
    if(count > 1) {
      calculateRadiusBox(multipoles, box);
    }
    else {
      multipoles.radius = 1.0; // single particle boxes don't need scaling.
    }
  }

  const CentroidData& operator+=(const CentroidData& cd) { // needed for upward traversal
    box.grow(cd.box);
    multipoles += cd.multipoles;
    count += cd.count;
    size_sm = 0.5*(box.size()).length();
    if(count > 1) {
      calculateRadiusFarthestCorner(multipoles, box);
    }
    else {
      multipoles.radius = 1.0; // Single particle boxes don't need scaling
    }
    return *this;
  }

  CentroidData& operator=(const CentroidData&) = default;

  void pup(PUP::er& p) {
    p | multipoles;
    p | box;
    p | count;
    p | size_sm;
    p | max_rad;
  }

};

#endif // ASTRO_CENTROID_H_
