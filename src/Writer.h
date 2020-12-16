
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
  int cur_dim = 0;

  void do_write();
};

#endif /* _WRITER_H_ */
