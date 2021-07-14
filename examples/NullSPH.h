#ifndef EXAMPLES_NULL_SPH_H_
#define EXAMPLES_NULL_SPH_H_

// defines empty implementations of these functions to build non-SPH examples.
// this could be eliminated by having example-specific macros that selectively
// enable per-leaf functions, e.g., ( #ifdef SPH ).

void FirstSphFn::perLeafFn(SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {}
void SecondSphFn::perLeafFn(SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {}
void ThirdSphFn::perLeafFn(SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition) {}

#endif
