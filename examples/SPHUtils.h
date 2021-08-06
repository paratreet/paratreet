#include <cmath>


namespace paratreet {

/* Standard M_4 Kernel */
/**
 * @brief kernelM4 is a scaled version of the standard M4 cubic SPH kernel
 * 
 * This returns a scaled version of the standard SPH kernel (W) of Monaghan 1992
 * The kernel W(q) is defined as:
 *  W(q) = (1/(pi h^3)) (1 - 1.5q^2 + 0.75q^3)       for 0 < q < 1
 *  W(q) = (1/(4 pi h^3)) (2 - q)^3                  for 1 < q < 2
 * for q = |dx|/h
 * 
 * NOTE: This function returns a scaled version of W(q)
 * @param ar2 = q^2 = (|dx|/h)^2
 * @return (pi h^3) W
 */
    static Real kernelM4(Real ar2) {
        Real ak;
        ak = 2.0 - sqrt(ar2);
        if (ar2 < 1.0) ak = (1.0 - 0.75*ak*ar2);
        else ak = 0.25*ak*ak*ak;
        return ak;
    }
/**
 * @brief dkernelM4 returns a scaled gradient of the M4 SPH kernel.
 * 
 * This returns a scaled version of the gradient of the Monaghan 1992 kernel
 * The kernel gradient gradW is defined as:
 *  gradW(q) = (1/(pi h^5)) (-3 + 9q/4) dx            for 0 < q < 1
 *  gradW(q) = -(1/(pi h^5)) (3/4) [(2-q)^2 /q] dx    for 1 < q < 2
 * For q = |dx|/h and dx is the particle separation (a vector)
 * NOTE: This function returns a scaled (and scalar) version of gradW
 * 
 * @param ar2 = q^2 = (|dx|/h)^2
 * @return (pi h^5/|dx|^2) (dx.dot.gradW)
 */
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

typedef struct PressSmoothUpdateStruct {
    double dvdotdr;
    double visc;
    double aFac;
    Vector3D<double> dx;
} PressSmoothUpdate;

typedef struct PressSmoothParticleStruct {
    double rNorm;
    double PoverRho2;
    double PoverRho2f;
} PressSmoothParticle;

static void doSPHCalc(SpatialNode<CentroidData>& leaf, int pi, Real fBall, Particle& b, Real fDivv_Corrector) {
    auto& a = leaf.particles()[pi];
    static constexpr const Real visc = 0.;
    static constexpr const Real aFac = 1.; // both of these are cosmology
    static constexpr const Real vFac = 1.;
    static constexpr const Real H = 0.0; // hubble constant //expansion of the universe, dont need it
    static constexpr const Real gammam1 = 5.0/3.0 - 1.;
    // scale factor of the universe treated as 1
    // poverrho2 = gammam1 / fDensity^2;
    // poverrho2 is pressure over density^2.
    // poverrho2 = gammam1 * p.uPred() / density;
    // poverrho2f is the geometric mean of the two densities


    Real ph = 0.5 * fBall; // fBall is the smoothing length and also the search radius
    Real ih2 = 4. / (fBall * fBall); // invH2 in changa
    Real fNorm1 = 0.5 * M_1_PI * ih2 * ih2 / ph; // 1 over sum of all weights
    auto dx = b.position - a.position; // points from us to our neighbor
    Real fDist2 = dx.lengthSquared();
    Real r2 = fDist2*ih2;
    Real rs1 = dkernelM4(r2);
    rs1 *= fNorm1;
    rs1 *= fDivv_Corrector;
    PressSmoothUpdate params;
    PressSmoothParticle aParams;
    PressSmoothParticle bParams;
    aParams.rNorm = rs1 * a.mass;
    bParams.rNorm = rs1 * b.mass;
    params.dx = dx;
    auto dv = b.velocity_predicted - a.velocity_predicted;
    params.dvdotdr = vFac*dot(dv, params.dx) + fDist2*H;
    aParams.PoverRho2 = a.u_predicted*gammam1/b.density;
    bParams.PoverRho2 = b.u_predicted*gammam1/a.density;
    /***********************************
     * SPH Pressure Terms Calculation
     ***********************************/
    /* Calculate Artificial viscosity term prefactor terms 
     * 
     * Updates:
     *  params.visc
     */
    { // Begin SPH pressure terms calculation and scope the variables below
    if (params.dvdotdr>=0.0) {
        params.visc = 0.0;
    } else {
        /* mu multiply by a to be consistent with physical c */
        Real absmu = -params.dvdotdr*aFac/sqrt(fDist2);
        /* viscosity terms */
        // params.visc = (varAlpha(alpha, p, q)*(p->c() + q->c())
        //    + varBeta(beta, p, q)*1.5*absmu);
        // params.visc = switchCombine(p,q)*params.visc*absmu/(p->fDensity + q->fDensity);
        params.visc = 0.0;
    }
//    updateParticle(a, b, &params, &pParams, &qParams, 1);
    Real PdV = bParams.rNorm * 0.5 * params.visc * params.dvdotdr;
    PdV += bParams.rNorm*aParams.PoverRho2*params.dvdotdr;
    leaf.applyGasWork(pi, PdV);
    auto && acc = (aParams.PoverRho2 + bParams.PoverRho2) + params.visc;
    assert(isfinite(acc));
    leaf.applyAcceleration(pi, acc*bParams.rNorm*params.dx);
    
//    updateParticle(a, b, &params, &qParams, &pParams, -1);
    PdV = aParams.rNorm * 0.5 * params.visc * params.dvdotdr;
    PdV += aParams.rNorm*aParams.PoverRho2*params.dvdotdr;
    b.pressure_dVolume += PdV;
    b.acceleration -= acc*aParams.rNorm*params.dx;
  }
}

}


