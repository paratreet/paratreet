#include "TreePiece.h"
#include "CentroidData.h"
#include "CentroidVisitor.h"
#include <map>
#include <queue>

extern CProxy_TreeElement<CentroidVisitor, CentroidData> centroid_calculator;

template <>
void TreePiece::calculateData<CentroidData>() {
  std::queue<Node*> nodes;
  nodes.push(root);
  std::map<Node*, CentroidData> centroid_info;
  while (nodes.size()) {
    Node* node = nodes.front();
    nodes.pop();
    if (!centroid_info.count(node)) {
      CentroidData cd;
      if (node->type == Node::Leaf) {
        // change cd to particle data
      }
      centroid_info.insert(std::make_pair(node, cd));
    }
    if (node->type == Node::Leaf || node->wait_count == 0) {
      if (node->parent) {
        centroid_info[node->parent] = centroid_info[node->parent] + centroid_info[node];
        if (node->parent->wait_count < 0) node->parent->wait_count = 8;
        node->parent->wait_count--;
        if (node->parent->wait_count == 0) nodes.push(node->parent);
      }
    }
    else {
      for (int i = 0; i < node->children.size(); i++) {
        nodes.push(node->children[i]);
      }
    }
  }
/*
  CentroidData cd;
  for (int i = 0; i < particles.size(); i++) {
    cd.moment += particles[i].position * particles[i].mass;
    cd.sum_mass += particles[i].mass;
  }
*/
  //CkCallback cb(CkReductionTarget(Main, summass), mainProxy);
  //contribute(sizeof(float),&sum_mass,CkReduction::sum_float, cb);
  
  CentroidVisitor v;
  v.leaf(centroid_calculator, centroid_info[root], tp_key); 
} 

