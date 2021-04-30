#ifndef MOMENTS_INCLUDED
#define MOMENTS_INCLUDED

#include "common.h"

/*************************************************
 *
 *  The precision of the non-scaled moments (MOMR,
 *  MOMC, and LOCR) are to always be double (or better) to
 *  prevent known over/underflow issues.
 *  This is the meaning of momFloat.
 *
 *              Do not change this
 *
 *************************************************/
#ifdef QUAD
typedef long double momFloat;
#define sqrt(x)	sqrtl(x)
#else
typedef double momFloat;
#endif

/**
 ** @brief moment tensor components for reduced multipoles.
 */
struct MOMR {
  momFloat m;
  momFloat xx,yy,xy,xz,yz;
  momFloat xxx,xyy,xxy,yyy,xxz,yyz,xyz;
  momFloat xxxx,xyyy,xxxy,yyyy,xxxz,yyyz,xxyy,xxyz,xyyz;
};

/**
 ** @brief moment tensor components for complete multipoles.
 */
struct MOMC {
  momFloat m;
  momFloat xx,yy,xy,xz,yz;
  momFloat xxx,xyy,xxy,yyy,xxz,yyz,xyz;
  momFloat xxxx,xyyy,xxxy,yyyy,xxxz,yyyz,xxyy,xxyz,xyyz;
  momFloat zz;
  momFloat xzz,yzz,zzz;
  momFloat xxzz,xyzz,xzzz,yyzz,yzzz,zzzz;
};

/**
 ** @brief moment tensor components for reduced local expansion.
 ** note that we have the 5th-order terms here now!
 */
struct LOCR {
  momFloat m;
  momFloat x,y,z;
  momFloat xx,xy,yy,xz,yz;
  momFloat xxx,xxy,xyy,yyy,xxz,xyz,yyz;
  momFloat xxxx,xxxy,xxyy,xyyy,yyyy,xxxz,xxyz,xyyz,yyyz;
  momFloat xxxxx,xxxxy,xxxyy,xxyyy,xyyyy,yyyyy,xxxxz,xxxyz,xxyyz,xyyyz,yyyyz;
};

/*
** The next set of data structures are intended specifically for use with float
** precision. These moments are usually scaled to a characteristic size of the 
** cell or volume. The convention is to use the scaling factor u for the multipole
** moments and scaling factor v for the local expansion.
*/
struct FMOMR {
  Real m;
  Real xx, yy, xy, xz, yz;
  Real xxx, xyy, xxy, yyy, xxz, yyz, xyz;
  Real xxxx, xyyy, xxxy, yyyy, xxxz, yyyz, xxyy, xxyz, xyyz;
};

struct FLOCR {
  Real m;
  Real x, y, z;
  Real xx, yy, xy, xz, yz;
  Real xxx, xyy, xxy, yyy, xxz, yyz, xyz;
  Real xxxx, xyyy, xxxy, yyyy, xxxz, yyyz, xxyy, xxyz, xyyz;
  Real xxxxx, xyyyy, xxxxy, yyyyy, xxxxz, yyyyz, xxxyy, xxyyy, xxxyz, xyyyz, xxyyz;
};

void momClearMomr(MOMR *mr);
void momClearFmomr(FMOMR *l);
void momAddMomc(MOMC *,MOMC *);
void momAddMomr(MOMR *,MOMR *);
void momAddFmomr(FMOMR *mr,FMOMR *ma);
void momScaledAddFmomr(FMOMR *mr, Real ur, FMOMR *ma, Real ua);
void momRescaleFmomr(FMOMR *mr, Real unew, Real uold);
void momMulAddMomc(MOMC *,momFloat,MOMC *);
void momMulAddMomr(MOMR *,momFloat,MOMR *);
void momMulAddFmomr(FMOMR *mr, Real ur, Real m, FMOMR *ma,
                    Real ua);
void momSubMomc(MOMC *,MOMC *);
void momSubMomr(MOMR *,MOMR *);
void momScaledSubFmomr(FMOMR *mr, Real ur, FMOMR *ma, Real ua);
void momMakeMomc(MOMC *,momFloat,momFloat,momFloat,momFloat);
Real momMakeFmomr(FMOMR *mr, Real m, Real u, Real x,
                       Real y, Real z);
momFloat momMakeMomr(MOMR *,momFloat,momFloat,momFloat,momFloat);
void momOldMakeMomr(MOMR *,momFloat,momFloat,momFloat,momFloat);
void momShiftMomc(MOMC *,momFloat,momFloat,momFloat);
void momShiftMomr(MOMR *,momFloat,momFloat,momFloat);
void momShiftFmomr(FMOMR *m, Real u, Real x, Real y,
                   Real z);
double momShiftLocr(LOCR *,momFloat,momFloat,momFloat);
void momReduceMomc(MOMC *,MOMR *);
void momEvalMomr(MOMR *,momFloat,momFloat,momFloat,momFloat,
				 momFloat *,momFloat *,momFloat *,momFloat *);
void momEvalFmomrcm(const FMOMR *m, Real u, Real dir, Real x,
                    Real y, Real z, Real *fPot, Real *ax,
                    Real *ay, Real *az, Real *magai);
void momMomr2Momc(MOMR *,MOMC *);
void momFmomr2Momc(FMOMR *ma,MOMC *mc);
void momPrintMomc(MOMC *);
void momPrintMomr(MOMR *);

void momClearLocr(LOCR *);
double momLocrAddMomr5(LOCR *,MOMR *,momFloat,momFloat,momFloat,momFloat,double *,double *,double *);
double momFlocrAddFmomr5cm(FLOCR *l, Real v, FMOMR *m, Real u,
                           Real dir, Real x, Real y, Real z,
                           Real *tax, Real *tay, Real *taz);
void momEvalLocr(LOCR *,momFloat,momFloat,momFloat,
		 momFloat *,momFloat *,momFloat *,momFloat *);
double momLocrAddMomr(LOCR *,MOMR *,momFloat,momFloat,momFloat,momFloat);


#endif // MOMENTS_INCLUDED
