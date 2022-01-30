// Ewald summation code.
// First implemented by Thomas Quinn and Joachim Stadel in PKDGRAV.

#include "Main.h"
#include "CentroidData.h"
#include "EwaldData.h"

extern Vector3D<Real> fPeriod;
extern int nReplicas;

#ifdef HEXADECAPOLE
inline
static
void QEVAL(MOMC mom, double gam[], double dx, double dy,
		  double dz, double &ax, double &ay, double &az, double &fPot)
{
    double Qmirx,Qmiry,Qmirz,Qmir,Qta;
    Qta = 0.0;
    Qmirx = (1.0/6.0)*(mom.xzzz*dz*dz*dz + 3*mom.xyzz*dy*dz*dz + 3*mom.xyyz*dy*dy*dz + mom.xyyy*dy*dy*dy + 3*mom.xxzz*dx*dz*dz + 6*mom.xxyz*dx*dy*dz + 3*mom.xxyy*dx*dy*dy + 3*mom.xxxz*dx*dx*dz + 3*mom.xxxy*dx*dx*dy + mom.xxxx*dx*dx*dx);
    Qmiry = (1.0/6.0)*(mom.yzzz*dz*dz*dz + 3*mom.xyzz*dx*dz*dz + 3*mom.xxyz*dx*dx*dz + mom.xxxy*dx*dx*dx + 3*mom.yyzz*dy*dz*dz + 6*mom.xyyz*dx*dy*dz + 3*mom.xxyy*dx*dx*dy + 3*mom.yyyz*dy*dy*dz + 3*mom.xyyy*dx*dy*dy + mom.yyyy*dy*dy*dy);
    Qmirz = (1.0/6.0)*(mom.yyyz*dy*dy*dy + 3*mom.xyyz*dx*dy*dy + 3*mom.xxyz*dx*dx*dy + mom.xxxz*dx*dx*dx + 3*mom.yyzz*dy*dy*dz + 6*mom.xyzz*dx*dy*dz + 3*mom.xxzz*dx*dx*dz + 3*mom.yzzz*dy*dz*dz + 3*mom.xzzz*dx*dz*dz + mom.zzzz*dz*dz*dz);
    Qmir = (1.0/4.0)*(Qmirx*dx + Qmiry*dy + Qmirz*dz);
    fPot -= gam[4]*Qmir;
    Qta += gam[5]*Qmir;
    ax += gam[4]*Qmirx;
    ay += gam[4]*Qmiry;
    az += gam[4]*Qmirz;
    Qmirx = (1.0/2.0)*(mom.xzz*dz*dz + 2*mom.xyz*dy*dz + mom.xyy*dy*dy + 2*mom.xxz*dx*dz + 2*mom.xxy*dx*dy + mom.xxx*dx*dx);
    Qmiry = (1.0/2.0)*(mom.yzz*dz*dz + 2*mom.xyz*dx*dz + mom.xxy*dx*dx + 2*mom.yyz*dy*dz + 2*mom.xyy*dx*dy + mom.yyy*dy*dy);
    Qmirz = (1.0/2.0)*(mom.yyz*dy*dy + 2*mom.xyz*dx*dy + mom.xxz*dx*dx + 2*mom.yzz*dy*dz + 2*mom.xzz*dx*dz + mom.zzz*dz*dz);
    Qmir = (1.0/3.0)*(Qmirx*dx + Qmiry*dy + Qmirz*dz);
    fPot -= gam[3]*Qmir;
    Qta += gam[4]*Qmir;
    ax += gam[3]*Qmirx;
    ay += gam[3]*Qmiry;
    az += gam[3]*Qmirz;
    Qmirx = (1.0/1.0)*(mom.xz*dz + mom.xy*dy + mom.xx*dx);
    Qmiry = (1.0/1.0)*(mom.yz*dz + mom.xy*dx + mom.yy*dy);
    Qmirz = (1.0/1.0)*(mom.yz*dy + mom.xz*dx + mom.zz*dz);
    Qmir = (1.0/2.0)*(Qmirx*dx + Qmiry*dy + Qmirz*dz);
    fPot -= gam[2]*Qmir;
    Qta += gam[3]*Qmir;
    ax += gam[2]*Qmirx;
    ay += gam[2]*Qmiry;
    az += gam[2]*Qmirz;
    fPot -= gam[0]*mom.m;
    Qta += gam[1]*mom.m;
    ax -= dx*Qta;
    ay -= dy*Qta;
    az -= dz*Qta;
}
#else
inline
static
void QEVAL(MultipoleMoments mom, double gam[], double dx, double dy,
		  double dz, double &ax, double &ay, double &az, double &fPot)
{
    double Qmirx,Qmiry,Qmirz,Qmir,Qta;
    Qta = 0.0;
    Qmirx = (1.0/1.0)*(mom.xz*dz + mom.xy*dy + mom.xx*dx);
    Qmiry = (1.0/1.0)*(mom.yz*dz + mom.xy*dx + mom.yy*dy);
    Qmirz = (1.0/1.0)*(mom.yz*dy + mom.xz*dx + mom.zz*dz);
    Qmir = (1.0/2.0)*(Qmirx*dx + Qmiry*dy + Qmirz*dz);
    fPot -= gam[2]*Qmir;
    Qta += gam[3]*Qmir;
    ax += gam[2]*Qmirx;
    ay += gam[2]*Qmiry;
    az += gam[2]*Qmirz;
    fPot -= gam[0]*mom.totalMass;
    Qta += gam[1]*mom.totalMass;
    ax -= dx*Qta;
    ay -= dy*Qta;
    az -= dz*Qta;
}
#endif


// Set up table for Ewald h (Fourier space) loop

void EwaldData::EwaldInit(const struct CentroidData root, const CkCallback& cb)
{
	int i,hReps,hx,hy,hz,h2;
	double alpha,k4,L;
	double gam[6],mfacc,mfacs;
	double ax,ay,az;
        const double dEwhCut = 2.8; // Radius of expansion in Fourier space
        multipoles = root.multipoles;

#ifdef HEXADECAPOLE
	/* convert to complete moments */
        /* XXX Need to get moments of root node */
	momRescaleFmomr(&(multipoles.mom),1.0f,multipoles.getRadius());
	momFmomr2Momc(&(multipoles.mom), &momcRoot);
	/* XXX note that we could leave the scaling as is and change
	   the radius of the root. */
	momRescaleFmomr(&(multipoles.mom),multipoles.getRadius(),1.0f);
#endif
	/*
	 ** Now setup stuff for the h-loop.
	 */
	hReps = (int) ceil(dEwhCut);
	L = fPeriod.x;
	alpha = 2.0/L;
	k4 = M_PI*M_PI/(alpha*alpha*L*L);
	ewt.clear();
	for (hx=-hReps;hx<=hReps;++hx) {
		for (hy=-hReps;hy<=hReps;++hy) {
			for (hz=-hReps;hz<=hReps;++hz) {
				h2 = hx*hx + hy*hy + hz*hz;
				if (h2 == 0) continue;
				if (h2 > dEwhCut*dEwhCut) continue;
				gam[0] = exp(-k4*h2)/(M_PI*h2*L);
				gam[1] = 2*M_PI/L*gam[0];
				gam[2] = -2*M_PI/L*gam[1];
				gam[3] = 2*M_PI/L*gam[2];
				gam[4] = -2*M_PI/L*gam[3];
				gam[5] = 2*M_PI/L*gam[4];
				gam[1] = 0.0;
				gam[3] = 0.0;
				gam[5] = 0.0;
				ax = 0.0;
				ay = 0.0;
				az = 0.0;
				mfacc = 0.0;
#ifdef HEXADECAPOLE
				QEVAL(momcRoot, gam, hx, hy, hz,
				      ax, ay, az, mfacc);
#else
				QEVAL(root.moments, gam, hx, hy, hz,
				      ax, ay, az, mfacc);
#endif
				gam[0] = exp(-k4*h2)/(M_PI*h2*L);
				gam[1] = 2*M_PI/L*gam[0];
				gam[2] = -2*M_PI/L*gam[1];
				gam[3] = 2*M_PI/L*gam[2];
				gam[4] = -2*M_PI/L*gam[3];
				gam[5] = 2*M_PI/L*gam[4];
				gam[0] = 0.0;
				gam[2] = 0.0;
				gam[4] = 0.0;
				ax = 0.0;
				ay = 0.0;
				az = 0.0;
				mfacs = 0.0;
#ifdef HEXADECAPOLE
				QEVAL(momcRoot, gam,hx,hy,hz,
				      ax,ay,az,mfacs);
#else
				QEVAL(root.moments, gam,hx,hy,hz,
				      ax,ay,az,mfacs);
#endif
                                EWT hentry;
				hentry.h.x = 2*M_PI/L*hx;
				hentry.h.y = 2*M_PI/L*hy;
				hentry.h.z = 2*M_PI/L*hz;
				hentry.hCfac = mfacc;
				hentry.hSfac = mfacs;
				ewt.push_back(hentry);
                        }
                }
        }
    this->contribute(cb);
}

