#ifndef SIMPLE_CONFIG_H_
#define SIMPLE_CONFIG_H_

#include <string>

struct Config {
  std::string input_file;
  double decomp_tolerance;
  int max_particles_per_tp;
  int max_particles_per_leaf;
  int decomp_type;
  int tree_type;

  void pup (PUP::er& p) {
    p | input_file;
    p | decomp_tolerance;
    p | max_particles_per_tp;
    p | max_particles_per_leaf;
    p | decomp_type;
    p | tree_type;
  }
};

#endif