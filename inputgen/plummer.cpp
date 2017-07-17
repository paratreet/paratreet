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

void writeToDisk(ofstream &ofs, Particle *p, int nbody);
void readFromDisk(ifstream &in, Particle *p, int nbody);

void testdata(ofstream &out, ifstream &in, int nbody, int preambleSize){
  Real rsc, vsc, rsq, r, v, x, y;
  Vector3D<Real> cmr, cmv;
  Particle *p;
  int rejects = 0;
  int k;
  int halfnbody, i;
  Real offset;
  Particle *cp;
  Real tmp;

  int curSize;

  int bufSize = 10*(1<<20)/sizeof(Particle);

  //cout << "bufsize " << bufSize << endl;

  halfnbody = nbody / 2;
  if (nbody % 2 != 0) halfnbody++;


  Particle *bodytab = new Particle[bufSize];
  assert(bodytab != NULL);

  rsc = 9 * PI / 16;
  vsc = sqrt(1.0 / rsc);

  cmr = Vector3D<Real>(0.0);
  cmv = Vector3D<Real>(0.0);

  int remaining = halfnbody;
  while(remaining > 0){
    if(remaining > bufSize) curSize = bufSize;
    else curSize = remaining;

    remaining -= curSize;

    for(p = bodytab; p < bodytab+curSize; p++){
      p->mass = 1.0/nbody;
      r = 1 / sqrt(std::pow((double)xrand(0.0, MFRAC), (double)-2.0/3.0) - 1);
      /*   reject radii greater than 10 */
      while (r > 9.0) {
        rejects++;
        r = 1 / sqrt(std::pow((double)xrand(0.0, MFRAC), (double)-2.0/3.0) - 1);

      }
      pickshell(p->position, rsc * r);

      cmr += p->position;
      do {
        x = xrand(0.0, 1.0);
        y = xrand(0.0, 0.1);

      } while (y > x*x * std::pow((double)(1 - x*x), (double)3.5));

      v = sqrt(2.0) * x / std::pow((double)(1 + r*r), (double)0.25);
      pickshell(p->velocity, vsc * v);
      cmv += p->velocity;
    }
    writeToDisk(out,bodytab,curSize);
  }

  offset = 4.0;

  remaining = nbody-halfnbody;
  in.seekg(preambleSize,ios::beg);

  //cout << "TELLG " << in.tellg() << endl;

  while(remaining > 0){
    if(remaining > bufSize) curSize = bufSize;
    else curSize = remaining;

    remaining -= curSize;

    readFromDisk(in,bodytab,curSize);

    for(p = bodytab; p < bodytab+curSize; p++){
      p->position += offset;
      cmr += p->position;
      cmv += p->position;
    }

    writeToDisk(out,bodytab,curSize);
  }

  cmr /= nbody;
  cmv /= nbody;

  in.seekg(preambleSize,ios::beg);
  out.seekp(preambleSize,ios::beg);

  remaining = nbody;
  while(remaining > 0){
    if(remaining > bufSize) curSize = bufSize;
    else curSize = remaining;

    remaining -= curSize;

    readFromDisk(in,bodytab,curSize);

    for(p = bodytab; p < bodytab+curSize; p++){
      p->position -= cmr;
      p->velocity -= cmv;
    }

    writeToDisk(out,bodytab,curSize);
  }

  delete[] bodytab;
}

/*
 * PICKSHELL: pick a random point on a sphere of specified radius.
 */

void pickshell(Vector3D<Real> &vec, Real rad)
   //Real vec[];                     /* coordinate vector chosen */
   //Real rad;                       /* radius of chosen point */
{
   register int k;
   Real rsq, rsc;

   do {
     vec.x = xrand(-1.0,1.0);
     vec.y = xrand(-1.0,1.0);
     vec.z = xrand(-1.0,1.0);
     rsq = vec.lengthSquared();
     //cout << "rsq " << vec.x << "," << vec.y << "," << vec.z << "," << rsq << endl;
   } while (rsq > 1.0);

   rsc = rad / sqrt(rsq);
   vec = rsc*vec;
}


int intpow(int i, int j)
{
    int k;
    int temp = 1;

    for (k = 0; k < j; k++)
        temp = temp*i;
    return temp;
}

void pranset(int);

#define NDIMS 3

int main(int argc, char **argv){
  pranset(128363);
  if(argc != 4){
    fprintf(stderr,"usage: ./plummer <mode write:0; read:1> <nbody> <output plummer model file>\n");
    return 1;
  }

  int mode = atoi(argv[1]);

  // WRITE
  if(mode == 0){
    ofstream out(argv[3], ios::out|ios::binary);
    ifstream in(argv[3], ios::in|ios::binary);

    assert(!in.bad() && !in.fail() && in.is_open());
    assert(!out.bad() && !out.fail() && out.is_open());

    int nbody = atoi(argv[2]);
    int ndims = 3;
    Real tnow = 0.0;

    out.write((char *)&nbody, sizeof(int));
    out.write((char *)&ndims, sizeof(int));
    out.write((char *)&tnow, sizeof(Real));

    int preambleSize = 2*sizeof(int)+sizeof(Real);

    testdata(out,in,nbody,preambleSize);

    out.close();
    in.close();
  }
  // READ
  else{
    ifstream in(argv[3], ios::in|ios::binary);
    assert(!in.fail() && !in.bad() && in.is_open());

    int print_limit = atoi(argv[2]);

    int nbody;
    int ndims;
    Real tnow;

    in.read((char *)&nbody, sizeof(int));
    in.read((char *)&ndims, sizeof(int));
    in.read((char *)&tnow, sizeof(Real));

    printf("nbody %d ndims %d tnow %f\n", nbody, ndims, tnow);
    assert(ndims == NDIMS);

    int bufSize = 10*(1<<20)/sizeof(Particle);

    Particle *bodytab = new Particle[bufSize];

    int remaining = nbody;
    int cnt = 0;
    int curSize;

    OrientedBox<Real> bb;
    Vector3D<Real> com;
    Real totalMass = 0.0;
    Real unitMass;

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
        if(cnt < print_limit){
          printf("idx %d pos %f %f %f mass %g\n",
                  cnt,
                  p->position.x,
                  p->position.y,
                  p->position.z,
                  p->mass);
        }
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

  }

  return 0;
}

void writeToDisk(ofstream &out, Particle *p, int nbody){
  Real *tmp = new Real[nbody*REALS_PER_PARTICLE];
  Real soft = 0.001;

  for(int i = 0; i < nbody*REALS_PER_PARTICLE; i += REALS_PER_PARTICLE){
    tmp[i+0] = p->position.x;
    tmp[i+1] = p->position.y;
    tmp[i+2] = p->position.z;
    tmp[i+3] = p->velocity.x;
    tmp[i+4] = p->velocity.y;
    tmp[i+5] = p->velocity.z;
    tmp[i+6] = p->mass;
    tmp[i+7] = soft;

    //cout << "WRITE " << p->position.x << " " << p->position.y << " " << p->position.z << endl;

    p++;
  }

  out.write((char*)tmp, nbody*REALS_PER_PARTICLE*sizeof(Real));
  out.flush();
  delete[] tmp;
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


