#ifndef EWALDDATA_H_
#define EWALDDATA_H_

#include "paratreet.decl.h"

typedef struct ewaldTable 
{
    Vector3D<double> h;
    double hCfac,hSfac;
} EWT;

class EwaldData : public CBase_EwaldData {
public:
    std::vector<EWT> ewt;
    MOMC momcRoot;  /* hold complete multipole moments */
    MultipoleMoments multipoles; /* multipole of root cell */
    EwaldData() { }
    void EwaldInit(const struct CentroidData, const CkCallback& cb);
};


#endif
