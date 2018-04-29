#ifndef SIMPLE_DENSITYVISITOR_H_
#define SIMPLE_DENSITYVISITOR_H_

#include "simple.decl.h"
#include "common.h"
#include <cmath>
#include <vector>
#include <queue>

struct DensityVisitor {
// in leaf check for not same particle plz
private:
  Real distsq(Vector3D<Real> p1, Vector3D<Real> p2) {
    Real dsq = (p1.x - p2.x) * (p1.x - p2.x);
    dsq += (p1.y - p2.y) * (p1.y - p2.y);
    dsq += (p1.z - p2.z) * (p1.z - p2.z);
    return dsq;
  }

public:
  bool node(Node<CentroidData>* from, Node<CentroidData>* on) {
    if (!on->data.neighbors.size()) {
      on->data.neighbors.reserve(on->n_particles);
      for (int i = 0; i < on->n_particles; i++) {
        particle_comp c (on->particles[i]);
        std::priority_queue<Particle, std::vector<Particle>, particle_comp> pq;
        on->data.neighbors.push_back(pq);
      }
    }
    const int k = 5;
    if (on->data.neighbors[0].size() < k) return true; // they all fill first k at the same time
    const Real total_volume = 3290.05;
    Real dist = std::sqrt(distsq(from->data.getCentroid(), on->data.getCentroid()));
    Real s_on = std::pow(total_volume, 1/3.) * std::pow(2, -1 * Utility::getDepthFromKey(on->key));
    Real s_from = std::pow(total_volume, 1/3.) * std::pow(2, -1 * Utility::getDepthFromKey(from->key));
    Real radius = std::sqrt(distsq(on->data.neighbors[0].top().position, from->data.getCentroid())); // not perfect but we give enough wiggle room
    if (dist - std::sqrt(2) * (s_from + s_on) > radius) return false;
    for (int i = 0; i < on->n_particles; i++) {
      Real subradius = std::sqrt(distsq(on->data.neighbors[i].top().position, from->data.getCentroid()));
      Real subdist = std::sqrt(distsq(from->data.getCentroid(), on->particles[i].position));
      if (subdist - std::sqrt(2) * s_from < subradius) return true;
    }   
    return false;
  }

  void leaf(Node<CentroidData>* from, Node<CentroidData>* on) {
    const int k = 5;
    on->data.neighbors.resize(on->n_particles);
    for (int i = 0; i < on->n_particles; i++) {
      for (int j = 0; j < from->n_particles; j++) {
        if (on->data.neighbors[i].size() < k) on->data.neighbors[i].push(from->particles[j]);
        Real radius2 = distsq(on->data.neighbors[i].top().position, from->particles[j].position);
        if (distsq(on->particles[i].position, from->particles[j].position) < radius2) {
          on->data.neighbors[i].push(from->particles[j]);
        }   
      }   
    }   
  }
};

#endif // SIMPLE_DENSITYVISITOR_H_
