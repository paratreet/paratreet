#ifndef ASTRO_FIELDS_H_
#define ASTRO_FIELDS_H_

struct AstroFields {
  // filename representing output conditions
  std::string output_file;
  // Periodic boundary conditions
  bool periodic = false;
  // Period lengths
  Vector3D<double> fPeriod;
  // Number of replicas for Ewald summation
  int nReplicas = 0;
  // Set a gravitational softening for all the particles
  double dSoft = 0.;
  // Use the dual tree algorithm for Gravity
  bool dual_tree = false;
  // Opening criteria
  Real theta = 0.7;
  // When to start checking collisions
  int iter_start_collision = 0;
  // Self explanatory
  Real max_timestep = 1e-5;

  void pup(PUP::er &p) {
    p | output_file;
    p | periodic;
    p | fPeriod;
    p | nReplicas;
    p | dSoft;
    p | dual_tree;
    p | theta;
    p | iter_start_collision;
    p | max_timestep;
  }
};

#endif // ASTRO_FIELDS_H_
