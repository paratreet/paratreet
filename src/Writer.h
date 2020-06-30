
#ifndef _WRITER_H_
#define _WRITER_H_

#include "paratreet.decl.h"
#include <vector>

struct Writer : public CBase_Writer {
  Writer(int n_treepieces, std::string of);
  void receive(std::vector<Particle> particles, CkCallback cb);

private:
  std::vector<Particle> particles;
  int num_treepieces;
  std::string output_file;
};

Writer::Writer(int n_treepieces, std::string of)
  : num_treepieces(n_treepieces), output_file(of)
{}

void Writer::receive(std::vector<Particle> ps, CkCallback cb)
{
  --num_treepieces;
  particles.insert(particles.end(), ps.begin(), ps.end());
  if (num_treepieces != 0) return;

  std::sort(particles.begin(), particles.end(),
            [](const Particle& left, const Particle& right) {
              return left.order < right.order;
            });

  FILE *fp = CmiFopen(output_file.c_str(), "w");
  CkAssert(fp);
  fprintf(fp, "%lu\n", particles.size());

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

  cb.send();
}

#endif /* _WRITER_H_ */
