#ifndef PARATREET_SEARCHDATA_H_
#define PARATREET_SEARCHDATA_H_

#include "common.h"
#include <vector>
#include <queue>
#include "BasicParticle.h"
#include "BasicBoundingBox.h"
#include "Paratreet.h"

struct SearchData {
  using Particle = paratreet::BasicParticle;
  using BoundingBox = paratreet::BasicBoundingBox;
  static void loadParticlesFromFile(int reader_index, int n_readers, const paratreet::Configuration& config, std::vector<Particle>& particles);
  static void addParticleToBox(const Particle& p, BoundingBox& box);
  static void adjustParticleForUniverse(Particle& p, const BoundingBox& box);
  static void outputToFile(int writer_index, int particle_index, const BoundingBox& box, int iter, const std::string& output_file, const std::vector<Particle>& particles, int indicator);

  Vector3D<Real> moment;
  Real sum_mass;
  Vector3D<Real> centroid; // too slow to compute this on the fly
  OrientedBox<Real> box;
  int count;

  SearchData() :
  moment(Vector3D<Real> (0,0,0)), sum_mass(0), count(0) {}

  /// Construct centroid from particles.
  SearchData(const Particle* particles, int n_particles, int depth) : SearchData() {
    for (int i = 0; i < n_particles; i++) {
      moment += particles[i].mass * particles[i].position;
      sum_mass += particles[i].mass;
      box.grow(particles[i].position);
    }
    centroid = moment / sum_mass;
    count = n_particles;
  }

  const SearchData& operator+=(const SearchData& cd) { // needed for upward traversal
    moment += cd.moment;
    sum_mass += cd.sum_mass;
    centroid = moment / sum_mass;
    box.grow(cd.box);
    count += cd.count;
    return *this;
  }

  SearchData& operator=(const SearchData&) = default;

  void pup(PUP::er& p) {
    p | moment;
    p | sum_mass;
    p | centroid;
    p | box;
    p | count;
  }

};

#endif // PARATREET_SEARCH_H_
