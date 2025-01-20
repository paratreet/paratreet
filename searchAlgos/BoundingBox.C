#include "BoundingBox.h"
#include <iomanip>

CkReduction::reducerType BoundingBox::boxReducer;

BoundingBox::BoundingBox(){
  reset();
}

void BoundingBox::reset(){
  n_particles = 0;
  box.reset();
  mass = 0.0;
}

void BoundingBox::grow(const Vector3D<Real> &v){
  box.grow(v);
}

/*
 * This method is called when performing a reduction over
 * BoundingBox's. It subsumes the bounding box of the 'other'
 * and accumulates its energy in its own. If a PE has no
 * particles, its contributions are not counted.
*/
void BoundingBox::grow(const BoundingBox &other){
  if(other.n_particles == 0) return;
  if(n_particles == 0){
    *this = other;
  }
  else{
    updated_time = std::max(updated_time, other.updated_time);
    box.grow(other.box);
    n_particles += other.n_particles;
    mass += other.mass;
  }
}

void BoundingBox::expand(Real pad){
  box.greater_corner = box.greater_corner*pad+box.greater_corner;
  box.lesser_corner = box.lesser_corner-pad*box.lesser_corner;
}

void BoundingBox::finalizeUniverse() {
  Vector3D<Real> bsize = box.size();
  Real max = (bsize.x > bsize.y) ? bsize.x : bsize.y;
  max = (max > bsize.z) ? max : bsize.z;
  Vector3D<Real> bcenter = box.center();
  // The magic number below is approximately 2^(-19)
  const Real fEps = 1.0 + 1.91e-6;  // slop to ensure keys fall between 0 and 1.
  bsize = Vector3D<Real>(fEps*0.5*max);
  box = OrientedBox<Real>(bcenter-bsize, bcenter+bsize);
}

void BoundingBox::pup(PUP::er &p){
  p | box;
  p | n_particles;
  p | mass;
  p | updated_time;
}

ostream &operator<<(ostream &os, const BoundingBox &bb){
  os << "<"
     << bb.n_particles << ", "
     << std::fixed << std::setprecision(3)
     << bb.mass << ", "
     << bb.box
     << ">";

  return os;
}

CkReductionMsg* BoundingBox::reduceFn(int n_msgs, CkReductionMsg** msgs) {
  BoundingBox* b = static_cast<BoundingBox*>(msgs[0]->getData());
  if (n_msgs > 1) {
    BoundingBox* msgb;
    for (int i = 1; i < n_msgs; i++) {
      msgb = static_cast<BoundingBox*>(msgs[i]->getData());
      *b += *msgb;
    }
  }

  return CkReductionMsg::buildNew(sizeof(BoundingBox), b);
}
