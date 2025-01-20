#include "CollisionVisitor.h"

/* readonly */ extern AstroFields astroConf;

Real CollisionVisitor::getCollideTime(const Particle& a, const Particle& b) {
  auto dx = a.position - b.position;
  auto vRel = a.velocity - b.velocity + (max_timestep / 2.) * (a.acceleration - b.acceleration); // this is kinda wrong cause accelerations are not updated properly
  auto rdotv = dot(dx, vRel);
  Real dx2 = dx.lengthSquared(), vRel2 = vRel.lengthSquared();
  Real sr = 2 * (a.soft + b.soft);
  auto dist = (dx2 - sr * sr);
  Real inside = dist/(rdotv*rdotv) * vRel2;
  Real dt = std::numeric_limits<Real>::max();
  //CkPrintf("rdotv is %lf, inside is %lf, dx2 is %lf, vRel2 is %lf, sr is %lf\n", rdotv, inside, dx2, vRel2, sr);
  if (inside <= 1) {
    Real D = sqrt(1 - inside);
    Real dt1 = -rdotv/vRel2*(1 + D);
    Real dt2 = -rdotv/vRel2*(1 - D);
    //CkPrintf("D is %lf, dt1 is %lf, dt2 is %lf\n", D, dt1, dt2);
    if (dt1 > 0 && dt1 < dt2) dt = dt1;
    else if (dt2 > 0 && dt2 < dt1) dt = dt2;
  }
  return dt;
}
