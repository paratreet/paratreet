/* adapted from barnes benchmark in SPLASH2 */

/*
 * TESTDATA: generate Plummer model initial conditions for test runs,
 * scaled to units such that M = -4E = G = 1 (Henon, Hegge, etc).
 * See Aarseth, SJ, Henon, M, & Wielen, R (1974) Astr & Ap, 37, 183.
 */

#include "Vector3D.h"
#include "OrientedBox.h"
#include "Particle.h"
#include <assert.h>
#include <stdlib.h>
#include <iostream>
#include <fstream>


using namespace std;

#define MFRAC  0.999                /* mass cut off at MFRAC of total */

Real xrand(Real,Real);
void pickshell(Vector3D<Real> &vec, Real rad);

void readFromDisk(ifstream &in, Particle *p, int nbody);

#define NDIMS 3
#include "TipsyFile.h"

#define EPS 0.05


int main(int argc, char **argv){
  if(argc != 3){
    fprintf(stderr,"usage: ./tipsyPlummer <input plummer model file> <output Tipsy file>\n");
    return 1;
  }

  ifstream in(argv[1], ios::in|ios::binary);
  assert(!in.fail() && !in.bad() && in.is_open());

  int nbody;
  int ndims;
  Real tnow;

  in.read((char *)&nbody, sizeof(int));
  in.read((char *)&ndims, sizeof(int));
  in.read((char *)&tnow, sizeof(Real));

  string fname(argv[2]);
  Tipsy::header hdr;

  printf("nbody %d ndims %d tnow %f\n", nbody, ndims, tnow);
  assert(ndims == NDIMS);

  hdr.nbodies = nbody;
  hdr.ndim = ndims;
  hdr.ndark = nbody;
  hdr.time = tnow;

  Tipsy::TipsyWriter wr(fname,hdr);
  assert(wr.writeHeader());

  int bufSize = 10*(1<<20)/sizeof(Particle);

  Particle *bodytab = new Particle[bufSize];

  int remaining = nbody;
  int cnt = 0;
  int curSize;

  OrientedBox<Real> bb;
  Vector3D<Real> com;
  Real totalMass = 0.0;
  Real unitMass;

  Tipsy::dark_particle dp;

  while(remaining > 0){
    if(remaining > bufSize) curSize = bufSize;
    else curSize = remaining;
    readFromDisk(in,bodytab,curSize);

    remaining -= curSize;

    Particle *p = bodytab;
    for(int i = 0; i < curSize; i++){
      if(cnt == 0) unitMass = p->mass;
      bb.grow(p->position);
      totalMass += p->mass;
      com += p->position;

      dp.mass = p->mass;
      dp.pos = p->position;
      dp.vel = p->velocity;
      dp.eps = EPS;
      dp.phi = 0.0;

      wr.putNextDarkParticle(dp);

      cnt++;
      p++;
    }
  }

  printf("\n\nmass %f bb %f %f %f %f %f %f com %f %f %f\n",
      unitMass*cnt,
      bb.lesser_corner.x,
      bb.lesser_corner.y,
      bb.lesser_corner.z,
      bb.greater_corner.x,
      bb.greater_corner.y,
      bb.greater_corner.z,
      com.x,
      com.y,
      com.z
      );

  // TEST

  Tipsy::TipsyReader rd(fname);
  //assert(rd.status());
  hdr = rd.getHeader();

  return 0;
}

void readFromDisk(ifstream &in, Particle *p, int nbody){
  Real *tmp = new Real[nbody*REALS_PER_PARTICLE];
  Real soft = 0.001;

  in.read((char*)tmp, nbody*REALS_PER_PARTICLE*sizeof(Real));
  for(int i = 0; i < nbody*REALS_PER_PARTICLE; i += REALS_PER_PARTICLE){
    p->position.x = tmp[i+0];
    p->position.y = tmp[i+1];
    p->position.z = tmp[i+2];
    p->velocity.x = tmp[i+3];
    p->velocity.y = tmp[i+4];
    p->velocity.z = tmp[i+5];
    p->mass       = tmp[i+6];
    soft          = tmp[i+7];

    //cout << "READ " << p->position.x << " " << p->position.y << " " << p->position.z << endl;
    p++;
  }

  delete[] tmp;
}


