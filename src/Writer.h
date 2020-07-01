
#ifndef _WRITER_H_
#define _WRITER_H_

#include "paratreet.decl.h"
#include <vector>

struct Writer : public CBase_Writer {
  Writer(std::string of, int n_particles);
  void receive(std::vector<Particle> particles, CkCallback cb);
  void write(CkCallback cb);

private:
  std::vector<Particle> particles;
  std::string output_file;
  int total_particles = 0;
  int expected_particles = 0;
  bool can_write = false;
  bool prev_written = false;

  void do_write();
};

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

void Writer::receive(std::vector<Particle> ps, CkCallback cb)
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

  if (prev_written || thisIndex == 0) {
    do_write();
    if (thisIndex != CkNumPes() - 1) thisProxy[thisIndex + 1].write(cb);
    else cb.send();
  }
}

void Writer::write(CkCallback cb)
{
  prev_written = true;
  if (can_write) {
    do_write();
    if (thisIndex != CkNumPes() - 1) thisProxy[thisIndex + 1].write(cb);
    else cb.send();
  }
}

void Writer::do_write()
{
  // Write particle accelerations to output file
  FILE *fp;
  if (thisIndex == 0) fp = CmiFopen(output_file.c_str(), "w");
  else fp = CmiFopen(output_file.c_str(), "a");
  CkAssert(fp);
  if (thisIndex == 0) fprintf(fp, "%d\n", total_particles);

  for (int dim = 0; dim < 3; ++dim) {
    for (const auto& particle : particles) {
      Real outval;
      if (dim == 0) outval = particle.acceleration.x;
      else if (dim == 1) outval = particle.acceleration.y;
      else if (dim == 2) outval = particle.acceleration.z;
      fprintf(fp, "%.14g\n", outval);
    }
  }

  int result = CmiFclose(fp);
  CkAssert(result == 0);
}

#endif /* _WRITER_H_ */
