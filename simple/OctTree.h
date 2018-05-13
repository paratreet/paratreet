#ifndef SIMPLE_OCTTREE_H
#define SIMPLE_OCTTREE_H

#include "common.h"
#include "BoundingBox.h"
#include "simple.decl.h"

class OctTree {
public:
    OctTree(int max_particles_per_leaf, int max_particles_per_tp, Real expand_ratio,
            CProxy_Reader& readers);
    CProxy_TreePiece<CentroidData> treepieces_proxy();
    void redistribute();

private:
    int max_particles_per_tp;
    int max_particles_per_leaf;
    Real expand_ratio;
    CProxy_Reader readers;
    CProxy_TreePiece<CentroidData> treepieces;
    BoundingBox universe;

    void build();
    void destroy();
};

#endif