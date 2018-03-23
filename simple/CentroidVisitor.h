#ifndef SIMPLE_CENTROIDVISITOR_H_
#define SIMPLE_CENTROIDVISITOR_H_

#include "simple.decl.h"
#include "CentroidData.h"
#include "common.h"

extern CProxy_Main mainProxy;
extern CProxy_TreeElement<CentroidVisitor, CentroidData> centroid_calculator;

class CentroidVisitor {
private:
  Key tp_key;
public:
  CentroidVisitor(Key tp_keyi = 0) : tp_key(tp_keyi) {}
  void leaf(Node<CentroidData>* node) {
    for (int i = 0; i < node->n_particles; i++) {
      Particle p = node->particles[i];
      node->data->moment += node->particles[i].mass * node->particles[i].position;
      node->data->sum_mass += node->particles[i].mass;
    }
  }
  bool node(Node<CentroidData>* node) {
    /*if (key == 1) {
      //CkPrintf("%f %f %f %f\n", cd.sum_mass, cd.moment.x, cd.moment.y, cd.moment.z);
      mainProxy.doneTraversal();
      return false;
    }*/
    if (tp_key && node->key == tp_key >> 3) {
      centroid_calculator[node->key].receiveData(*node->data, true);
      return false;
    }
    *node->parent->data = *node->parent->data + *node->data;
    return true;
  }
  void pup(PUP::er& p) {
    p | tp_key;
  }
};
#endif //SIMPLE_CENTROIDVISITOR_H_
