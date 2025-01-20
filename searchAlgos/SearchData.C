#include "SearchData.h"
#include "TipsyFile.h"

void SearchData::loadParticlesFromFile(int reader_index, int n_readers, const paratreet::Configuration& config, std::vector<Particle>& particles) {

  // Open tipsy file
  Tipsy::TipsyReader r(config.input_file);
  if (!r.status()) {
    CkPrintf("Reader %d failed to open tipsy file %s\n", reader_index, config.input_file.c_str());
    CkAbort("Tipsy reading failure in Reader -- see stdout");
  }

  // Read header and count particles
  Tipsy::header tipsyHeader = r.getHeader();
  int n_total = tipsyHeader.nbodies;
  int n_sph = tipsyHeader.nsph;
  int n_dark = tipsyHeader.ndark;
  int n_star = tipsyHeader.nstar;

  int n_particles = n_total / n_readers;
  int excess = n_total % n_readers;
  unsigned int start_particle = n_particles * reader_index;
  if (reader_index < (unsigned int)excess) {
    n_particles++;
    start_particle += reader_index;
  } else {
    start_particle += excess;
  }

  // Reserve space
  particles.resize(n_particles);

  // Read particles and grow bounding box
  if (!r.seekParticleNum(start_particle)) {
    CkAbort("Could not seek to particle\n");
  }

  Tipsy::gas_particle gp;
  Tipsy::dark_particle dp;
  Tipsy::star_particle sp;

  for (unsigned int i = 0; i < n_particles; i++) {
    if (start_particle + i < (unsigned int)n_sph) {
      if (!r.getNextGasParticle(gp)) {
        CkAbort("Could not read gas particle\n");
      }
      particles[i].mass = gp.mass;
      particles[i].position = gp.pos;
      particles[i].velocity = gp.vel;
    }
    else if (start_particle + i < (unsigned int)n_sph + (unsigned int)n_dark) {
      if (!r.getNextDarkParticle(dp)) {
        CkAbort("Could not read dark particle\n");
      }
      particles[i].mass = dp.mass;
      particles[i].position = dp.pos;
      particles[i].velocity = dp.vel;
    }
    else {
      if (!r.getNextStarParticle(sp)) {
        CkAbort("Could not read star particle\n");
      }
      particles[i].mass = sp.mass;
      particles[i].position = sp.pos;
      particles[i].velocity = sp.vel;
    }
    particles[i].updated_time = tipsyHeader.time;
    particles[i].order = start_particle + i;
  }
}

void SearchData::addParticleToBox(const Particle& p, BoundingBox& box) {
  box.n_particles++;
  box.grow(p.position);
  box.mass += p.mass;
  box.updated_time = std::max(box.updated_time, p.updated_time);
}

void SearchData::adjustParticleForUniverse(Particle& p, const BoundingBox& box) {
  p.adjustForUniverse(box.boxCorners());
}

void SearchData::outputToFile(int writer_index, int particle_index, const BoundingBox& box, int iter, const std::string& output_file, const std::vector<Particle>& particles, int indicator) {
  CkAbort("writing to file unexpectedly");
}
