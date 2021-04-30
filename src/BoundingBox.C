#include "BoundingBox.h"
#include <iomanip>

CkReduction::reducerType BoundingBox::boxReducer;

BoundingBox::BoundingBox(){
  reset();
}

void BoundingBox::reset(){
  n_particles = 0;
  n_dark = n_sph = n_star = 0;
  box.reset();
  pe = 0.0;
  ke = 0.0;
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
    box.grow(other.box);
    n_particles += other.n_particles;
    n_dark += other.n_dark;
    n_sph += other.n_sph;
    n_star += other.n_star;
    pe += other.pe;
    ke += other.ke;
    mass += other.mass;
  }
}

void BoundingBox::expand(Real pad){
  box.greater_corner = box.greater_corner*pad+box.greater_corner;
  box.lesser_corner = box.lesser_corner-pad*box.lesser_corner;
}

void BoundingBox::pup(PUP::er &p){
  p | box;
  p | n_particles;
  p | n_dark;
  p | n_sph;
  p | n_star;
  p | pe;
  p | ke;
  p | mass;
}

ostream &operator<<(ostream &os, const BoundingBox &bb){
  os << "<"
     << bb.n_particles << ", "
     << std::fixed << std::setprecision(3)
     << bb.mass << ", "
     << bb.pe + bb.ke << ", "
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
