#include <cmath>


namespace paratreet {

  static Real dkernelM4(Real ar2) {
    Real adk = sqrt(ar2);
    if (ar2 < 1.0) {
      adk = -3 + 2.25*adk;
    }
    else {
      adk = -0.75*(2.0-adk)*(2.0-adk)/adk;
    }
    return adk;
  }

  static void doSPHCalc(SpatialNode<CentroidData>& leaf, int pi, Real fBall, const Particle& b) {
    auto& a = leaf.particles()[pi];
    static constexpr const Real visc = 0.;
    static constexpr const Real fDivv_Corrector = 1.; // corrects bias wrt the divergence of velocities // RTFORCE
    static constexpr const Real aFac = 1.; // both of these are cosmology
    static constexpr const Real vFac = 1.;
    static constexpr const Real H = 1.; // hubble constant //expansion of the universe, dont need it
    static constexpr const Real gammam1 = 5.0/3.0 - 1.;
    // scale factor of the universe treated as 1
    // poverrho2 = gammam1 / fDensity^2;
    // poverrho2 is pressure over density^2.
    // poverrho2 = gammam1 * p.uPred() / density;
    // poverrho2f is the geometric mean of the two densities
    // density is the sum of the masses (including myself) of the neighborhood
    // does density use the merged neighbor list -> no
    // absMu = measure of how quickly particles are approaching each other

    Real ph = 0.5 * fBall; // fBall is the smoothing length and also the search radius
    Real ih2 = 4. / (fBall * fBall); // invH2 in changa
    Real fNorm1 = 0.5 * M_1_PI * ih2 * ih2 / ph; // 1 over sum of all weights
    auto dx = b.position - a.position; // points from us to our neighbor
    Real dsq = dx.lengthSquared();
   // divvnorm is a factor that you have to weight
    Real rs1 = dkernelM4(dsq * ih2) * fNorm1 * fDivv_Corrector; // rsq / density
    Real rNorm = rs1 * b.mass; // normalized kernel value
    auto dv = b.velocity_predicted - a.velocity_predicted;
    Real dvdotdr = vFac * dot(dv, dx) + dsq * H;
    Real shared_density = gammam1 / (a.density * b.density);
    Real PoverRho2a = a.potential_predicted * gammam1 / (a.density * a.density);
    Real PoverRho2Avg = shared_density * (a.potential_predicted + b.potential_predicted);
    Real work = rNorm * dvdotdr * (PoverRho2a + visc * 0.5);
    leaf.applyGasWork(pi, work);
    auto && accSignless = rNorm * aFac * (PoverRho2Avg + visc) * dx;
    auto acc = accSignless * (a.key == b.key ? 1 : -1);
    leaf.applyAcceleration(pi, acc);
  }

}

