/*
 * Improved routines for Hilbert-Peano Space Filling Curve
 * Provided by Joachim Stadel, University of Zurich
 */

#include <stdio.h>
#include <stdint.h>
#include <assert.h>

#include "hilbert.h"

/*
** x and y must have range [1,2) !
*/
uint64_t hilbert2d(float x,float y) {
    uint64_t s = 0;
    uint32_t m,ux,uy,ut;

    ux = *(uint32_t *)&x;
    uy = *(uint32_t *)&y;
    
    m = 0x00400000;

    while (m) {
	s = s << 2;
	if (ux&m) {
	    if (uy&m) {
		s |= 2;
		}
	    else {
		ut = ux;
		ux = ~uy;
		uy = ~ut;
		s |= 3;
		}
	    }
	else {
	    if (uy&m) {
		s |= 1;
		}
	    else {
		ut = ux;
		ux = uy;
		uy = ut;
		}
	    }
	m = m >> 1;
	}
    return s;
    }

#ifdef BIGKEYS

/*
** x and y must have range [1,2) !
*/
__uint128_t hilbert2d_double(double x,double y) {
    __uint128_t s = 0;
    uint64_t m,ux,uy,ut;

    ux = *(uint64_t *)&x;
    uy = *(uint64_t *)&y;
    
    m = (1ULL << 52);

    while (m) {
	s = s << 2;
	if (ux&m) {
	    if (uy&m) {
		s |= 2;
		}
	    else {
		ut = ux;
		ux = ~uy;
		uy = ~ut;
		s |= 3;
		}
	    }
	else {
	    if (uy&m) {
		s |= 1;
		}
	    else {
		ut = ux;
		ux = uy;
		uy = ut;
		}
	    }
	m = m >> 1;
	}
    return s;
    }

#endif

void ihilbert2d(uint64_t s,float *px,float *py,float *pz) {
    /*
     * Function not implemented yet.
     */
    assert(0);
    }

/*
** x, y and z must have range [1,2) !
*/
uint64_t hilbert3d(float x,float y,float z) {
    uint64_t s = 0;
    uint32_t m,ux,uy,uz,ut;

    assert(x >= 1.0f && x < 2.0f);
    assert(y >= 1.0f && y < 2.0f);
    assert(z >= 1.0f && z < 2.0f);
    
    ux = (*(uint32_t *)&x)>>2;
    uy = (*(uint32_t *)&y)>>2;
    uz = (*(uint32_t *)&z)>>2;

    m = 0x00100000;

    while (m) {
	s = s << 3;

	if (ux&m) {
	    if (uy&m) {
		if (uz&m) {
		    ut = ux;
		    ux = uy;
		    uy = ~uz;
		    uz = ~ut;
		    s |= 5;
		    }
		else {
		    ut = uz;
		    uz = ux;
		    ux = uy;
		    uy = ut;
		    s |= 2;
		    }
		}
	    else {
		ux = ~ux;
		uy = ~uy;
		if (uz&m) {
		    s |= 4;
		    }
		else {
		    s |= 3;
		    }
		}
	    }
	else {
	    if (uy&m) {
		if (uz&m) {
		    ut = ux;
		    ux = uy;
		    uy = ~uz;
		    uz = ~ut;
		    s |= 6;
		    }
		else {
		    ut = uz;
		    uz = ux;
		    ux = uy;
		    uy = ut;
		    s |= 1;
		    }
		}
	    else {
		if (uz&m) {
		    ut = uy;
		    uy = ux;
		    ux = ~uz;
		    uz = ~ut;
		    s |= 7;
		    }
		else {
		    ut = uy;
		    uy = ux;
		    ux = uz;
		    uz = ut;
		    s |= 0;
		    }
		}
	    }
	m = m >> 1;
	}
    return s;
    }

#ifdef BIGKEYS
/*
** x, y and z must have range [1,2) !
*/
__uint128_t hilbert3d_double(double x,double y,double z) {
    __uint128_t s = 0;
    uint64_t m,ux,uy,uz,ut;

    assert(x >= 1.0 && x < 2.0);
    assert(y >= 1.0 && y < 2.0);
    assert(z >= 1.0 && z < 2.0);

    /* We can only use 42 bits.  Lose the last 10 bits. */
    ux = (*(uint64_t *)&x)>>10;
    uy = (*(uint64_t *)&y)>>10;
    uz = (*(uint64_t *)&z)>>10;

    m = (1ULL << 41);

    while (m) {
	s = s << 3;

	if (ux&m) {
	    if (uy&m) {
		if (uz&m) {
		    ut = ux;
		    ux = uy;
		    uy = ~uz;
		    uz = ~ut;
		    s |= 5;
		    }
		else {
		    ut = uz;
		    uz = ux;
		    ux = uy;
		    uy = ut;
		    s |= 2;
		    }
		}
	    else {
		ux = ~ux;
		uy = ~uy;
		if (uz&m) {
		    s |= 4;
		    }
		else {
		    s |= 3;
		    }
		}
	    }
	else {
	    if (uy&m) {
		if (uz&m) {
		    ut = ux;
		    ux = uy;
		    uy = ~uz;
		    uz = ~ut;
		    s |= 6;
		    }
		else {
		    ut = uz;
		    uz = ux;
		    ux = uy;
		    uy = ut;
		    s |= 1;
		    }
		}
	    else {
		if (uz&m) {
		    ut = uy;
		    uy = ux;
		    ux = ~uz;
		    uz = ~ut;
		    s |= 7;
		    }
		else {
		    ut = uy;
		    uy = ux;
		    ux = uz;
		    uz = ut;
		    s |= 0;
		    }
		}
	    }
	m = m >> 1;
	}
    return s;
    }

#endif

void ihilbert3d(uint64_t s,float *px,float *py,float *pz) {
    uint32_t m,ux=0,uy=0,uz=0,ut;

    m = 0x00000001;

    while (m != 0x00200000) {
	switch (s&7) {
	case 0:
	    ut = uz;
	    uz = ux;
	    ux = uy;
	    uy = ut;
	    ux &= ~m;
	    uy &= ~m;
	    uz &= ~m;
	    break;
	case 1:
	    ut = uy;
	    uy = ux;
	    ux = uz;
	    uz = ut;
	    ux &= ~m;
	    uy |= m;
	    uz &= ~m;
	    break;
	case 2:
	    ut = uy;
	    uy = ux;
	    ux = uz;
	    uz = ut;
	    ux |= m;
	    uy |= m;
	    uz &= ~m;
	    break;
	case 3:
	    ux = ~ux;
	    uy = ~uy;
	    ux |= m;
	    uy &= ~m;
	    uz &= ~m;
	    break;
	case 4:
	    ux = ~ux;
	    uy = ~uy;
	    ux |= m;
	    uy &= ~m;
	    uz |= m;
	    break;
	case 5:
	    ut = ~uz;
	    uz = ~uy;
	    uy = ux;
	    ux = ut;
	    ux |= m;
	    uy |= m;
	    uz |= m;
	    break;
	case 6:
	    ut = ~uz;
	    uz = ~uy;
	    uy = ux;
	    ux = ut;
	    ux &= ~m;
	    uy |= m;
	    uz |= m;
	    break;
	case 7:
	    ut = ~uz;
	    uz = ~ux;
	    ux = uy;
	    uy = ut;
	    ux &= ~m;
	    uy &= ~m;
	    uz |= m;
	    break;
	    }
	m = m << 1;
	s = s >> 3;
	}
    m = 0x001fffff;
    ux &= m;
    uy &= m;
    uz &= m;
    ux = ux << 2;
    uy = uy << 2;
    uz = uz << 2;
    m = 0x3f800000;
    ux |= m;
    uy |= m;
    uz |= m;
    *px = (*(float *)&ux);
    *py = (*(float *)&uy);
    *pz = (*(float *)&uz);
    }

#if 0

#include "tipsy.h"

struct mypart {
    uint64_t key;
    float x,y,z;
    };
    

int cmppart(const void *va,const void *vb) {
    struct mypart *pa = (struct mypart *)va; 
    struct mypart *pb = (struct mypart *)vb;

    if (pa->key > pb->key) return 1;
    else if (pa->key < pb->key) return -1;
    else return;
    }
	     

int main(void) {
    const int b3d = 1;
    TCTX out;
    int i,j,k,n,ii,nn;
    float x,y,z,d;
    struct mypart *p;
    struct dark_particle dp;

    n = (1 << 4);
    if (b3d) nn = n*n*n;
    else nn = n*n;

    d = 1.0/n;
    p = malloc(nn*sizeof(struct mypart));
    assert(p != NULL);

    ii = 0;
    for (i=0;i<n;++i) {
	x = (i+0.5)*d + 1.0; 
	for (j=0;j<n;++j) {
	    y = (j+0.5)*d + 1.0;
	    for (k=0;k<n;++k) {
		z = (k+0.5)*d + 1.0;
		p[ii].x = x;
		p[ii].y = y;
		p[ii].z = z;
		if (b3d) {
		    p[ii++].key = hilbert3d(x,y,z);
		    }
		else {
		    p[ii++].key = hilbert2d(x,y);
		    break;
		    }
		}
	    }
	}

    qsort(p,nn,sizeof(struct mypart),cmppart);

    TipsyInitialize(&out,0,NULL);

    for (ii=0;ii<nn;++ii) {
	dp.mass = 1.0;
	dp.pos[0] = p[ii].x;
	dp.pos[1] = p[ii].y;
	dp.pos[2] = p[ii].z;
	if (ii == nn-1) {
	    dp.vel[0] = 0.0;
	    dp.vel[1] = 0.0;
	    dp.vel[2] = 0.0;
	    }
	else {
	    dp.vel[0] = p[ii+1].x - p[ii].x;
	    dp.vel[1] = p[ii+1].y - p[ii].y;
	    dp.vel[2] = p[ii+1].z - p[ii].z;
	    }
	dp.phi = (double)p[ii].key/(double)p[nn-1].key;
	fprintf(stderr,"%lld\n",p[ii].key);
	TipsyAddDark(out,&dp);
	}
    TipsyWriteAll(out,0.0,NULL);
    }
#endif
