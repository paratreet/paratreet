#include "Writer.h"
#include "TipsyFile.h"

Writer::Writer(std::string of, int n_particles)
  : output_file(of), total_particles(n_particles)
{
  expected_particles = n_particles / CkNumPes();
  if (expected_particles * CkNumPes() != n_particles) {
    ++expected_particles;
    if (thisIndex == CkNumPes() - 1)
      expected_particles = n_particles - thisIndex * expected_particles;
  }
}

void Writer::receive(std::vector<Particle> ps, Real time, CkCallback cb)
{
  // Accumulate received particles
  particles.insert(particles.end(), ps.begin(), ps.end());

  if (particles.size() != expected_particles) return;

  // Received expected number of particles, sort the particles
  std::sort(particles.begin(), particles.end(),
            [](const Particle& left, const Particle& right) {
              return left.order < right.order;
            });

  can_write = true;

  if (prev_written || thisIndex == 0)
    write(time, cb);
}

void Writer::write(Real time, CkCallback cb)
{
  prev_written = true;
  if (can_write) {
    do_write();
    cur_dim = (cur_dim + 1) % 3;
    if (thisIndex != CkNumPes() - 1) thisProxy[thisIndex + 1].write(time, cb);
    else if (cur_dim == 0) cb.send();
    else thisProxy[0].write(time, cb);
  }
}

void Writer::do_write()
{
  // Write particle accelerations to output file
  FILE *fp;
  FILE *fpDen;
  if (thisIndex == 0 && cur_dim == 0) {
    fp = CmiFopen((output_file+".acc").c_str(), "w");
    fprintf(fp, "%d\n", total_particles);
    fpDen = CmiFopen((output_file+".den").c_str(), "w");
    fprintf(fpDen, "%d\n", total_particles);
  } else {
      fp = CmiFopen((output_file+".acc").c_str(), "a");
      fpDen = CmiFopen((output_file+".den").c_str(), "a");
  }
  CkAssert(fp);
  CkAssert(fpDen);

  for (const auto& particle : particles) {
    Real outval;
    if (cur_dim == 0) outval = particle.acceleration.x;
    else if (cur_dim == 1) outval = particle.acceleration.y;
    else if (cur_dim == 2) outval = particle.acceleration.z;
    fprintf(fp, "%.14g\n", outval);
    if (cur_dim == 0) {
        fprintf(fpDen, "%.14g\n", particle.density);
    }
  }

  int result = CmiFclose(fp);
  CkAssert(result == 0);
  result = CmiFclose(fpDen);
  CkAssert(result == 0);
}

TipsyWriter::TipsyWriter(std::string of, int n_particles)
  : output_file(of), total_particles(n_particles)
{
  expected_particles = n_particles / CkNumPes();
  if (expected_particles * CkNumPes() != n_particles) {
    ++expected_particles;
    if (thisIndex == CkNumPes() - 1)
      expected_particles = n_particles - thisIndex * expected_particles;
  }
}

void TipsyWriter::receive(std::vector<Particle> ps, Real time, CkCallback cb)
{
  // Accumulate received particles
  particles.insert(particles.end(), ps.begin(), ps.end());

  if (particles.size() != expected_particles) return;

  // Received expected number of particles, sort the particles
  std::sort(particles.begin(), particles.end(),
            [](const Particle& left, const Particle& right) {
              return left.order < right.order;
            });

  can_write = true;

  if (prev_written || thisIndex == 0)
    write(time, cb);
}

void TipsyWriter::write(Real time, CkCallback cb)
{
  prev_written = true;
  if (can_write) {
    do_write(time);
    if (thisIndex != CkNumPes() - 1) thisProxy[thisIndex + 1].write(time, cb);
    else cb.send();
  }
}

void TipsyWriter::do_write(Real time)
{
  Tipsy::header tipsyHeader;

  tipsyHeader.time = time;
  tipsyHeader.nbodies = total_particles;
  tipsyHeader.nsph = 0;
  tipsyHeader.nstar = total_particles;
  tipsyHeader.ndark = 0;

  bool use_double = sizeof(Real) == 8;
  Tipsy::TipsyWriter w(output_file, tipsyHeader, false, use_double, use_double);

  if(thisIndex == 0) w.writeHeader();
  int prefix_count = (total_particles / CkNumPes()) * thisIndex;
  if(!w.seekParticleNum(prefix_count)) CkAbort("bad seek");

  for (const auto& p : particles) {
    Tipsy::star_particle_t<Real, Real> sp;
    sp.mass = p.mass;
    sp.pos = p.position;
    sp.vel = p.velocity; // dvFac = 1
    if(!w.putNextStarParticle_t(sp)) {
      CkError("[%d] Write star failed, errno %d: %s\n", CkMyPe(), errno, strerror(errno));
      CkAbort("Bad Write");
    }
  }
}



