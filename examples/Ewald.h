extern CProxy_EwaldData ewaldProxy;

PARATREET_REGISTER_PER_LEAF_FN(LeafEwaldFn, CentroidData, (
  [](SpatialNode<CentroidData>& leaf, Partition<CentroidData>* partition)
{
#ifdef HEXADECAPOLE
        MOMC mom = ewaldProxy.ckLocalBranch()->momcRoot;
	MultipoleMoments &momQuad = ewaldProxy.ckLocalBranch()->multipoles;
	double xx,xxx,xxy,xxz,yy,yyy,yyz,xyy,zz,zzz,xzz,yzz,xy,xyz,xz,yz;
	double Q4mirx,Q4miry,Q4mirz,Q4mir,Q4x,Q4y,Q4z;
	double Q4xx,Q4xy,Q4xz,Q4yy,Q4yz,Q4zz,Q4,Q3x,Q3y,Q3z;
	double Q3mirx,Q3miry,Q3mirz,Q3mir;
	const double onethird = 1.0/3.0;
#else
	MultipoleMoments mom = ewaldProxy.ckLocalBranch()->multipoles;
#endif
        auto ewt = ewaldProxy.ckLocalBranch()->ewt;
	double Q2;
	double L,fEwCut2,fInner2,alpha,alpha2,alphan,k1,ka;
	double fPot,ax,ay,az;
	double x,y,z,r2,dir,dir2,a;
	double Q2mirx,Q2miry,Q2mirz,Q2mir,Qta;
	double g0,g1,g2,g3,g4,g5;
	double hdotx,s,c;
	int i,j,ix,iy,iz,nEwReps;
        const double fEwCut = 2.6;  /* radius of real space expansion */

	/*
	 ** Set up traces of the complete multipole moments.
	 */
#ifdef HEXADECAPOLE
        Q4xx = 0.5*(mom.xxxx + mom.xxyy + mom.xxzz);
        Q4xy = 0.5*(mom.xxxy + mom.xyyy + mom.xyzz);
        Q4xz = 0.5*(mom.xxxz + mom.xyyz + mom.xzzz);
        Q4yy = 0.5*(mom.xxyy + mom.yyyy + mom.yyzz);
        Q4yz = 0.5*(mom.xxyz + mom.yyyz + mom.yzzz);
        Q4zz = 0.5*(mom.xxzz + mom.yyzz + mom.zzzz);
        Q4 = 0.25*(Q4xx + Q4yy + Q4zz);
        Q3x = 0.5*(mom.xxx + mom.xyy + mom.xzz);
        Q3y = 0.5*(mom.xxy + mom.yyy + mom.yzz);
        Q3z = 0.5*(mom.xxz + mom.yyz + mom.zzz);
#endif
	Q2 = 0.5*(mom.xx + mom.yy + mom.zz);	

	nEwReps = (int) ceil(fEwCut);
	L = fPeriod.x;
	fEwCut2 = fEwCut*fEwCut*L*L;
	fInner2 = 1.2e-3*L*L;
	nEwReps = nEwReps > nReplicas ? nEwReps : nReplicas;
	alpha = 2.0/L;
	alpha2 = alpha*alpha;
	k1 = M_PI/(alpha2*L*L*L);
	ka = 2.0*alpha/sqrt(M_PI);
	for(j = 0; j < leaf.n_particles; j++) {
            // multistepping if (p[j].rung < activeRung) continue;
            auto& particle = leaf.particles()[j];
#ifdef HEXADECAPOLE
            fPot = momQuad.totalMass*k1;
#else
            fPot = mom.totalMass*k1;
#endif
            Vector3D<Real> acc(0.0);
#ifdef HEXADECAPOLE
            auto dx = particle.position - momQuad.cm;
#else
            auto dx = particle.position - mom.cm;
#endif
            for (ix=-nEwReps;ix<=nEwReps;++ix) {
                bool bInHolex = (ix >= -nReplicas && ix <= nReplicas);
                x = dx.x + ix*L;
                for(iy=-nEwReps;iy<=nEwReps;++iy) {
                    bool bInHolexy = (bInHolex && iy >= -nReplicas && iy <= nReplicas);
                    y = dx.y + iy*L;
                    for(iz=-nEwReps;iz<=nEwReps;++iz) {
                        bool bInHole = (bInHolexy && iz >= -nReplicas && iz <= nReplicas);
                        /*
                         ** Scoring for Ewald inner stuff = (+,*)
                         **	Visible ops 		= (104,161)
                         **	sqrt, 1/sqrt est. 	= (6,11)
                         **     division            = (6,11)  same as sqrt.
                         **	exp est.	    = (6,11)  same as sqrt.
                         **	erf/erfc est.	    = (12,22) twice a sqrt.
                         **	Total		    = (128,205) = 333
                         **     Old scoring                     = 447
                         */
                        z = dx.z + iz*L;
                        r2 = x*x + y*y + z*z;
                        if (r2 > fEwCut2 && !bInHole) continue;
                        if (r2 < fInner2) {
                            /*
                             * For small r, series expand about
                             * the origin to avoid errors caused
                             * by cancellation of large terms.
                             */
                            alphan = ka;
                            r2 *= alpha2;
                            g0 = alphan*((1.0/3.0)*r2 - 1.0);
                            alphan *= 2*alpha2;
                            g1 = alphan*((1.0/5.0)*r2 - (1.0/3.0));
                            alphan *= 2*alpha2;
                            g2 = alphan*((1.0/7.0)*r2 - (1.0/5.0));
                            alphan *= 2*alpha2;
                            g3 = alphan*((1.0/9.0)*r2 - (1.0/7.0));
                            alphan *= 2*alpha2;
                            g4 = alphan*((1.0/11.0)*r2 - (1.0/9.0));
                            alphan *= 2*alpha2;
                            g5 = alphan*((1.0/13.0)*r2 - (1.0/11.0));
                        }
                        else {
                            dir = 1/sqrt(r2);
                            dir2 = dir*dir;
                            a = exp(-r2*alpha2);
                            a *= ka*dir2;
                            if (bInHole) g0 = -erf(alpha/dir);
                            else g0 = erfc(alpha/dir);
                            g0 *= dir;
                            g1 = g0*dir2 + a;
                            alphan = 2*alpha2;
                            g2 = 3*g1*dir2 + alphan*a;
                            alphan *= 2*alpha2;
                            g3 = 5*g2*dir2 + alphan*a;
                            alphan *= 2*alpha2;
                            g4 = 7*g3*dir2 + alphan*a;
                            alphan *= 2*alpha2;
                            g5 = 9*g4*dir2 + alphan*a;
                        }
#ifdef HEXADECAPOLE
                        xx = 0.5*x*x;
                        xxx = onethird*xx*x;
                        xxy = xx*y;
                        xxz = xx*z;
                        yy = 0.5*y*y;
                        yyy = onethird*yy*y;
                        xyy = yy*x;
                        yyz = yy*z;
                        zz = 0.5*z*z;
                        zzz = onethird*zz*z;
                        xzz = zz*x;
                        yzz = zz*y;
                        xy = x*y;
                        xyz = xy*z;
                        xz = x*z;
                        yz = y*z;
                        Q2mirx = mom.xx*x + mom.xy*y + mom.xz*z;
                        Q2miry = mom.xy*x + mom.yy*y + mom.yz*z;
                        Q2mirz = mom.xz*x + mom.yz*y + mom.zz*z;
                        Q3mirx = mom.xxx*xx + mom.xxy*xy + mom.xxz*xz + mom.xyy*yy + mom.xyz*yz + mom.xzz*zz;
                        Q3miry = mom.xxy*xx + mom.xyy*xy + mom.xyz*xz + mom.yyy*yy + mom.yyz*yz + mom.yzz*zz;
                        Q3mirz = mom.xxz*xx + mom.xyz*xy + mom.xzz*xz + mom.yyz*yy + mom.yzz*yz + mom.zzz*zz;
                        Q4mirx = mom.xxxx*xxx + mom.xxxy*xxy + mom.xxxz*xxz + mom.xxyy*xyy + mom.xxyz*xyz +
                            mom.xxzz*xzz + mom.xyyy*yyy + mom.xyyz*yyz + mom.xyzz*yzz + mom.xzzz*zzz;
                        Q4miry = mom.xxxy*xxx + mom.xxyy*xxy + mom.xxyz*xxz + mom.xyyy*xyy + mom.xyyz*xyz +
                            mom.xyzz*xzz + mom.yyyy*yyy + mom.yyyz*yyz + mom.yyzz*yzz + mom.yzzz*zzz;
                        Q4mirz = mom.xxxz*xxx + mom.xxyz*xxy + mom.xxzz*xxz + mom.xyyz*xyy + mom.xyzz*xyz +
                            mom.xzzz*xzz + mom.yyyz*yyy + mom.yyzz*yyz + mom.yzzz*yzz + mom.zzzz*zzz;
                        Q4x = Q4xx*x + Q4xy*y + Q4xz*z;
                        Q4y = Q4xy*x + Q4yy*y + Q4yz*z;
                        Q4z = Q4xz*x + Q4yz*y + Q4zz*z;
                        Q2mir = 0.5*(Q2mirx*x + Q2miry*y + Q2mirz*z) - (Q3x*x + Q3y*y + Q3z*z) + Q4;
                        Q3mir = onethird*(Q3mirx*x + Q3miry*y + Q3mirz*z) - 0.5*(Q4x*x + Q4y*y + Q4z*z);
                        Q4mir = 0.25*(Q4mirx*x + Q4miry*y + Q4mirz*z);
                        Qta = g1*mom.m - g2*Q2 + g3*Q2mir + g4*Q3mir + g5*Q4mir;
                        fPot -= g0*mom.m - g1*Q2 + g2*Q2mir + g3*Q3mir + g4*Q4mir;
                        acc.x += g2*(Q2mirx - Q3x) + g3*(Q3mirx - Q4x) + g4*Q4mirx - x*Qta;
                        acc.y += g2*(Q2miry - Q3y) + g3*(Q3miry - Q4y) + g4*Q4miry - y*Qta;
                        acc.z += g2*(Q2mirz - Q3z) + g3*(Q3mirz - Q4z) + g4*Q4mirz - z*Qta;
#else
                        Q2mirx = mom.xx*x + mom.xy*y + mom.xz*z;
                        Q2miry = mom.xy*x + mom.yy*y + mom.yz*z;
                        Q2mirz = mom.xz*x + mom.yz*y + mom.zz*z;
                        Q2mir = 0.5*(Q2mirx*x + Q2miry*y + Q2mirz*z);
                        Qta = g1*mom.totalMass - g2*Q2 + g3*Q2mir;
                        fPot -= g0*mom.totalMass - g1*Q2 + g2*Q2mir;
                        acc.x += g2*(Q2mirx) - x*Qta;
                        acc.y += g2*(Q2miry) - y*Qta;
                        acc.z += g2*(Q2mirz) - z*Qta;
#endif
                    }
                }
            }
            /*
            ** Scoring for the h-loop (+,*)
            ** 	Without trig = (10,14)
            **	    Trig est.	 = 2*(6,11)  same as 1/sqrt scoring.
            **		Total        = (22,36)
            **					 = 58
            */
            for (i=0;i<ewt.size();++i) {
                hdotx = dot(ewt[i].h, dx);
                c = cos(hdotx);
                s = sin(hdotx);
                fPot += ewt[i].hCfac*c + ewt[i].hSfac*s;
                acc += ewt[i].h*(ewt[i].hCfac*s - ewt[i].hSfac*c);
            }
            leaf.applyPotential(j, fPot);
            leaf.applyAcceleration(j, acc);
        }
	return;
 }));

