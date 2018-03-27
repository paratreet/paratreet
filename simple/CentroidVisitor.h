#ifndef SIMPLE_CENTROIDVISITOR_H_
#define SIMPLE_CENTROIDVISITOR_H_

#include "simple.decl.h"
#include "CentroidData.h"
#include "common.h"

extern CProxy_Main mainProxy;
extern CProxy_TreeElement<CentroidData> centroid_calculator;

class CentroidVisitor {
private:
  int tp_index;
  CProxy_TreePiece<CentroidData> tp_proxy;
public:
  CentroidVisitor(CProxy_TreePiece<CentroidData> tp_proxyi, int tp_indexi = -1) : 
  tp_proxy(tp_proxyi), tp_index(tp_indexi) {}
  void leaf(Node<CentroidData>* node) {
    for (int i = 0; i < node->n_particles; i++) {
      Particle p = node->particles[i];
      node->data.moment += node->particles[i].mass * node->particles[i].position;
      node->data.sum_mass += node->particles[i].mass;
    }
  }
  bool node(Node<CentroidData>* node) {
    /*if (key == 1) {
      //CkPrintf("%f %f %f %f\n", cd.sum_mass, cd.moment.x, cd.moment.y, cd.moment.z);
      mainProxy.doneTraversal();
      return false;
    }*/
    if (!node->parent || node->parent->type == Node<CentroidData>::Boundary) {
      centroid_calculator[node->key].receiveData<CentroidVisitor>(tp_proxy, tp_index, node->data);
      return false;
    }
    node->parent->data = node->parent->data + node->data;
    return true;
  }
  void pup(PUP::er& p) {
    p | tp_index;
    p | tp_proxy;
  }
};
#endif //SIMPLE_CENTROIDVISITOR_H_
