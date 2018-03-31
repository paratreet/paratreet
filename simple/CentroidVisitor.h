#ifndef SIMPLE_CENTROIDVISITOR_H_
#define SIMPLE_CENTROIDVISITOR_H_

#include "simple.decl.h"
#include "CentroidData.h"
#include "common.h"

extern CProxy_Main mainProxy;
extern CProxy_TreeElement<CentroidData> centroid_calculator;

class CentroidVisitor {
private:
  CProxy_TreePiece<CentroidData> tp_proxy;
  int tp_index;
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
    if (node->key == 1) {
      CkPrintf("%f %f %f %f\n", node->data.sum_mass, node->data.moment.x, node->data.moment.y, node->data.moment.z);
      mainProxy.doneUp();
      return false;
    }
    if (tp_index == -1 || node->parent->type == Node<CentroidData>::Boundary) {
      if (node->type == Node<CentroidData>::Boundary) 
        centroid_calculator[node->key >> 3].receiveData<CentroidVisitor>(tp_proxy, node->data, -1); // doesn't exist
      else centroid_calculator[node->key].receiveData<CentroidVisitor>(tp_proxy, node->data, tp_index);
      return false;
    }
    node->parent->data = node->parent->data + node->data;
    return true;
  }
  void pup(PUP::er& p) {
    p | tp_proxy;
    p | tp_index;
  }
};
#endif //SIMPLE_CENTROIDVISITOR_H_
