/** @file SFC.h
 Structures, defines, functions relating to space-filling curves,
 as used to build trees for particle data.
 */

#ifndef SFC_H
#define SFC_H

#include <iostream>
#include <stdlib.h>

#include "Vector3D.h"
#include "OrientedBox.h"

#ifndef PEANO
#define PEANO
#endif

#ifdef PEANO
#include "hilbert.h"
extern int peanoKey;
#endif

namespace SFC {

#ifdef BIGKEYS
    /* 128 bit key for large dynamic range problems.  This gives 42
       (we will only use the first 126 bits)
       bits per coordinate, or a dynamic range of 4e12. */
typedef __uint128_t Key;
static const int KeyBits = 126;
#else   
/* Standard 64 bit key: 21 bits per coordinate and a dynamic range of
   2e6 */
typedef uint64_t Key;
static const int KeyBits = 63;
#endif

inline void printFloatBits(float f, std::ostream& os) {
	int i = *reinterpret_cast<int *>(&f);
	if(i & (1 << 31))
		os << "1 ";
	else
		os << "0 ";
	for(int j = 30; j >= 23; --j) {
		if(i & (1 << j))
			os << "1";
		else
			os << "0";
	}
	os << " ";
	for(int j = 22; j >= 0; --j) {
		if(i & (1 << j))
			os << "1";
		else
			os << "0";
	}
}

inline void printKeyBits(Key k, std::ostream& os) {
	for(int i = 63; i >= 0; --i) {
		if(k & (static_cast<Key>(1) << i))
			os << "1";
		else
			os << "0";
	}
}

inline void printIntBits(int k, std::ostream& os) {
	for(int i = 31; i >= 0; --i) {
		if(k & (1 << i))
			os << "1";
		else
			os << "0";
	}
}

/** The very first possible key a particle can take on. */
const Key firstPossibleKey = static_cast<Key>(0);
/** The very last possible key a particle can take on. */
#ifdef BIGKEYS
const Key lastPossibleKey = ~(static_cast<Key>(3) << 126);
#else
const Key lastPossibleKey = ~(static_cast<Key>(1) << 63);
#endif

#ifndef BIGKEYS
/* Below only works for 63 bit keys. */
#ifdef PEANO
static int quadrants[24][2][2][2] = {
  /* rotx=0, roty=0-3 */
  {{{0, 7}, {1, 6}}, {{3, 4}, {2, 5}}},
  {{{7, 4}, {6, 5}}, {{0, 3}, {1, 2}}},
  {{{4, 3}, {5, 2}}, {{7, 0}, {6, 1}}},
  {{{3, 0}, {2, 1}}, {{4, 7}, {5, 6}}},
  /* rotx=1, roty=0-3 */
  {{{1, 0}, {6, 7}}, {{2, 3}, {5, 4}}},
  {{{0, 3}, {7, 4}}, {{1, 2}, {6, 5}}},
  {{{3, 2}, {4, 5}}, {{0, 1}, {7, 6}}},
  {{{2, 1}, {5, 6}}, {{3, 0}, {4, 7}}},
  /* rotx=2, roty=0-3 */
  {{{6, 1}, {7, 0}}, {{5, 2}, {4, 3}}},
  {{{1, 2}, {0, 3}}, {{6, 5}, {7, 4}}},
  {{{2, 5}, {3, 4}}, {{1, 6}, {0, 7}}},
  {{{5, 6}, {4, 7}}, {{2, 1}, {3, 0}}},
  /* rotx=3, roty=0-3 */
  {{{7, 6}, {0, 1}}, {{4, 5}, {3, 2}}},
  {{{6, 5}, {1, 2}}, {{7, 4}, {0, 3}}},
  {{{5, 4}, {2, 3}}, {{6, 7}, {1, 0}}},
  {{{4, 7}, {3, 0}}, {{5, 6}, {2, 1}}},
  /* rotx=4, roty=0-3 */
  {{{6, 7}, {5, 4}}, {{1, 0}, {2, 3}}},
  {{{7, 0}, {4, 3}}, {{6, 1}, {5, 2}}},
  {{{0, 1}, {3, 2}}, {{7, 6}, {4, 5}}},
  {{{1, 6}, {2, 5}}, {{0, 7}, {3, 4}}},
  /* rotx=5, roty=0-3 */
  {{{2, 3}, {1, 0}}, {{5, 4}, {6, 7}}},
  {{{3, 4}, {0, 7}}, {{2, 5}, {1, 6}}},
  {{{4, 5}, {7, 6}}, {{3, 2}, {0, 1}}},
  {{{5, 2}, {6, 1}}, {{4, 3}, {7, 0}}}
};


static int rotxmap_table[24] = { 4, 5, 6, 7, 8, 9, 10, 11,
  12, 13, 14, 15, 0, 1, 2, 3, 17, 18, 19, 16, 23, 20, 21, 22
};

static int rotymap_table[24] = { 1, 2, 3, 0, 16, 17, 18, 19,
  11, 8, 9, 10, 22, 23, 20, 21, 14, 15, 12, 13, 4, 5, 6, 7
};

static int rotx_table[8] = { 3, 0, 0, 2, 2, 0, 0, 1 };
static int roty_table[8] = { 0, 1, 1, 2, 2, 3, 3, 0 };

static int sense_table[8] = { -1, -1, -1, +1, +1, -1, -1, -1 };

static int flag_quadrants_inverse = 1;
static char quadrants_inverse_x[24][8];
static char quadrants_inverse_y[24][8];
static char quadrants_inverse_z[24][8];


/*! This function computes a Peano-Hilbert key for an integer triplet (x,y,z),
 *  with x,y,z in the range between 0 and 2^bits-1.
 */
inline Key peano_hilbert_key(unsigned int x, unsigned int y, unsigned int z, int bits)
{
  int i, quad, bitx, bity, bitz;
  int mask, rotation, rotx, roty, sense;
  Key key;


  mask = 1 << (bits - 1);
  key = 0;
  rotation = 0;
  sense = 1;


  for(i = 0; i < bits; i++, mask >>= 1)
    {
      bitx = (x & mask) ? 1 : 0;
      bity = (y & mask) ? 1 : 0;
      bitz = (z & mask) ? 1 : 0;

      quad = quadrants[rotation][bitx][bity][bitz];

      key <<= 3;
      key += (sense == 1) ? (quad) : (7 - quad);

      rotx = rotx_table[quad];
      roty = roty_table[quad];
      sense *= sense_table[quad];

      while(rotx > 0)
	{
	  rotation = rotxmap_table[rotation];
	  rotx--;
	}

      while(roty > 0)
	{
	  rotation = rotymap_table[rotation];
	  roty--;
	}
    }

  return key;
}


/*! This function computes for a given Peano-Hilbert key, the inverse,
 *  i.e. the integer triplet (x,y,z) with a Peano-Hilbert key equal to the
 *  input key. (This functionality is actually not needed in the present
 *  code.)
 */
inline void peano_hilbert_key_inverse(Key key, int bits, unsigned int *x, unsigned int *y, unsigned int *z)
{
  int i, keypart, bitx, bity, bitz, mask, quad, rotation, shift;
  char sense, rotx, roty;

  if(flag_quadrants_inverse)
    {
      flag_quadrants_inverse = 0;
      for(rotation = 0; rotation < 24; rotation++)
        for(bitx = 0; bitx < 2; bitx++)
          for(bity = 0; bity < 2; bity++)
            for(bitz = 0; bitz < 2; bitz++)
              {
                quad = quadrants[rotation][bitx][bity][bitz];
                quadrants_inverse_x[rotation][quad] = bitx;
                quadrants_inverse_y[rotation][quad] = bity;
                quadrants_inverse_z[rotation][quad] = bitz;
              }
    }

  shift = 3 * (bits - 1);
  mask = 7 << shift;

  rotation = 0;
  sense = 1;

  *x = *y = *z = 0;

  for(i = 0; i < bits; i++, mask >>= 3, shift -= 3)
    {
      keypart = (key & mask) >> shift;

      quad = (sense == 1) ? (keypart) : (7 - keypart);

      *x = (*x << 1) + quadrants_inverse_x[rotation][quad];
      *y = (*y << 1) + quadrants_inverse_y[rotation][quad];
      *z = (*z << 1) + quadrants_inverse_z[rotation][quad];

      rotx = rotx_table[quad];
      roty = roty_table[quad];
      sense *= sense_table[quad];

      while(rotx > 0)
        {
          rotation = rotxmap_table[rotation];
          rotx--;
        }

      while(roty > 0)
        {
          rotation = rotymap_table[rotation];
          roty--;
        }
    }
}
#endif
#endif /* !BIGKEYS */

#ifdef BIGKEYS
/** Given the double floating point numbers for the location,
 construct the key. 
 The key uses 42 of the 52 bits for the doubles of the x, y, and z coordinates
 of the position vector.  This process will only make sense if the position
 coordinates are in the range [1,2).  The mantissa bits are taken, and interleaved
 in xyz order to form the key.  This makes the key a position on the z-ordering
 space-filling curve. 
 */
/*
 * This function expects positions to be between [0,1).  The first
 * lines turn these into integers from 0 to 2^42.
 */
inline Key makeKey(double exchangeKey[3]) {
  uint64_t ix = (uint64_t)(exchangeKey[0]*(1ULL<<42) - exchangeKey[0]);
  uint64_t iy = (uint64_t)(exchangeKey[1]*(1ULL<<42) - exchangeKey[1]);
  uint64_t iz = (uint64_t)(exchangeKey[2]*(1ULL<<42) - exchangeKey[2]);
  Key key;
#ifdef PEANO
  switch (peanoKey) {
  case 1:
      abort();
      break;
  case 2:
      /* Joachim's hilberts want inputs between 1 and 2 */
      key = hilbert2d_double(exchangeKey[0]+1.0, exchangeKey[1]+1.0);
      break;
  case 3:
      key = hilbert3d_double(exchangeKey[0]+1.0, exchangeKey[1]+1.0,
	  exchangeKey[2]+1.0);
      break;
  case 0:
#endif
	key = 0;
	for(uint64_t mask = (1ULL << 41); mask > 0; mask >>= 1) {
		key <<= 3;
		if(ix & mask)
			key += 4;
		if(iy & mask)
			key += 2;
		if(iz & mask)
			key += 1;
	}
#ifdef PEANO
  }
#endif
	return key;
}

inline Key generateKey(const Vector3D<double>& v, const OrientedBox<double>& boundingBox){
  Vector3D<double> d = (v - boundingBox.lesser_corner)
      / (boundingBox.greater_corner - boundingBox.lesser_corner);

  double exchangeKey[3];
  exchangeKey[0] = d.x;
  exchangeKey[1] = d.y;
  exchangeKey[2] = d.z;
  return makeKey(exchangeKey);
}

/** Given a key, create a vector of doubles representing a position.
 This is almost the inverse of the makeKey() function.  Since the key
 does not use all the bits, each double generated here will have its last
 ten mantissa bits set zero, regardless of the values when the key was
 originally generated.
 */
inline Vector3D<double> makeVector(Key k){
  uint64_t ix=0, iy=0, iz=0;
#ifdef PEANO
  if (peanoKey) {
      abort();
  } else {
#endif
	for(int64_t mask = (1 << 0); mask <= (1ULL << 41); mask <<= 1) {
		if(k & 4)
			ix |= mask;
		if(k & 2)
			iy |= mask;
		if(k & 1)
			iz |= mask;
		k >>= 3;	
	}
#ifdef PEANO
  }
#endif
  double x = ((double)ix) / ((1ULL<<42)-1);
  double y = ((double)iy) / ((1ULL<<42)-1);
  double z = ((double)iz) / ((1ULL<<42)-1);
	Vector3D<double> v(x, y, z);
	return v;
}

#else /* !BIGKEYS */
/** Given the floating point numbers for the location, construct the key. 
 The key uses 21 of the 23 bits for the floats of the x, y, and z coordinates
 of the position vector.  This process will only make sense if the position
 coordinates are in the range [1,2).  The mantissa bits are taken, and interleaved
 in xyz order to form the key.  This makes the key a position on the z-ordering
 space-filling curve. 
 */
/*
 * This function expects positions to be between [0,1).  The first
 * lines turn these into integers from 0 to 2^21.
 */
inline Key makeKey(float exchangeKey[3]) {
  unsigned int ix = (unsigned int)(exchangeKey[0]*(1<<21) - exchangeKey[0]);
  unsigned int iy = (unsigned int)(exchangeKey[1]*(1<<21) - exchangeKey[1]);
  unsigned int iz = (unsigned int)(exchangeKey[2]*(1<<21) - exchangeKey[2]);
  Key key;
#ifdef PEANO
  switch (peanoKey) {
  case 1:
      key = peano_hilbert_key(ix, iy, iz, 21);
      break;
  case 2:
      /* Joachim's hilberts want inputs between 1 and 2 */
      key = hilbert2d(exchangeKey[0]+1.0f, exchangeKey[1]+1.0f);
      break;
  case 3:
      key = hilbert3d(exchangeKey[0]+1.0f, exchangeKey[1]+1.0f,
	  exchangeKey[2]+1.0f);
      break;
  case 0:
#endif
	key = 0;
	for(unsigned int mask = (1 << 20); mask > 0; mask >>= 1) {
		key <<= 3;
		if(ix & mask)
			key += 4;
		if(iy & mask)
			key += 2;
		if(iz & mask)
			key += 1;
	}
#ifdef PEANO
  }
#endif
	return key;
}

inline Key generateKey(const Vector3D<float>& v, const OrientedBox<float>& boundingBox){
  Vector3D<float> d = (v - boundingBox.lesser_corner) / (boundingBox.greater_corner - boundingBox.lesser_corner); //+ Vector3D<float>(1, 1, 1);

  float exchangeKey[3];
  exchangeKey[0] = d.x;
  exchangeKey[1] = d.y;
  exchangeKey[2] = d.z;
  return makeKey(exchangeKey);
}

/** Given a key, create a vector of floats representing a position.
 This is almost the inverse of the makeKey() function.  Since the key
 does not use all the bits, each float generated here will have its last
 two mantissa bits set zero, regardless of the values when the key was
 originally generated.
 */
inline Vector3D<float> makeVector(Key k){
  unsigned int ix=0, iy=0, iz=0;
#ifdef PEANO
  if (peanoKey) {
    peano_hilbert_key_inverse(k, 22, &ix, &iy, &iz);
  } else {
#endif
	for(int mask = (1 << 0); mask <= (1 << 20); mask <<= 1) {
		if(k & 4)
			ix |= mask;
		if(k & 2)
			iy |= mask;
		if(k & 1)
			iz |= mask;
		k >>= 3;	
	}
#ifdef PEANO
  }
#endif
  float x = ((float)ix) / ((1<<21)-1);
  float y = ((float)iy) / ((1<<21)-1);
  float z = ((float)iz) / ((1<<21)-1);
	Vector3D<float> v(x, y, z);
	return v;
}

#endif /* BIGKEYS */

template <typename T>
inline OrientedBox<T> cutBoxLeft(const OrientedBox<T>& box, const int axis) {
	OrientedBox<T> newBox = box;
	switch(axis) {
		case 0: //cut x
			newBox.greater_corner.x = (box.greater_corner.x + box.lesser_corner.x) / 2.0;
			break;
		case 1: //cut y
			newBox.greater_corner.y = (box.greater_corner.y + box.lesser_corner.y) / 2.0;				
			break;
		case 2: //cut z
			newBox.greater_corner.z = (box.greater_corner.z + box.lesser_corner.z) / 2.0;
			break;
	}
	return newBox;
}

template <typename T>
inline OrientedBox<T> cutBoxRight(const OrientedBox<T>& box, const int axis) {
	OrientedBox<T> newBox = box;
	switch(axis) {
		case 0: //cut x
			newBox.lesser_corner.x = (box.greater_corner.x + box.lesser_corner.x) / 2.0;
			break;
		case 1: //cut y
			newBox.lesser_corner.y = (box.greater_corner.y + box.lesser_corner.y) / 2.0;				
			break;
		case 2: //cut z
			newBox.lesser_corner.z = (box.greater_corner.z + box.lesser_corner.z) / 2.0;
			break;
	}
	return newBox;
}

/** Increment the floating point number until the last two bits
 of the mantissa are zero.
 */
inline float bumpLastTwoBits(float f, float direction) {
	int howmany = *reinterpret_cast<int *>(&f) & 3;
	if(direction < 0) {
		switch(howmany) {
			case 1:
				howmany = 3;
				break;
			case 3:
				howmany = 1;
				break;
		}
	}
	switch(howmany) {
		case 0:
			f = nextafterf(f, direction);
		case 1:
			f = nextafterf(f, direction);
		case 2:
			f = nextafterf(f, direction);
		case 3:
			f = nextafterf(f, direction);
	}
	return f;
}

inline void bumpBox(OrientedBox<float>& b, float direction) {
	b.greater_corner.x = bumpLastTwoBits(b.greater_corner.x, direction);
	b.greater_corner.y = bumpLastTwoBits(b.greater_corner.y, direction);
	b.greater_corner.z = bumpLastTwoBits(b.greater_corner.z, direction);
	b.lesser_corner.x = bumpLastTwoBits(b.lesser_corner.x, -direction);
	b.lesser_corner.y = bumpLastTwoBits(b.lesser_corner.y, -direction);
	b.lesser_corner.z = bumpLastTwoBits(b.lesser_corner.z, -direction);	
}

template <typename T>
inline void cubize(OrientedBox<T>& b) {
	T max = b.greater_corner.x - b.lesser_corner.x;
	if((b.greater_corner.y - b.lesser_corner.y) > max)
		max = b.greater_corner.y - b.lesser_corner.y;
	if((b.greater_corner.z - b.lesser_corner.z) > max)
		max = b.greater_corner.z - b.lesser_corner.z;
	T middle = (b.greater_corner.x + b.lesser_corner.x) / 2.0;
	b.greater_corner.x = middle + max / 2.0;
	b.lesser_corner.x = middle - max / 2.0;
	middle = (b.greater_corner.y + b.lesser_corner.y) / 2.0;
	b.greater_corner.y = middle + max / 2.0;
	b.lesser_corner.y = middle - max / 2.0;
	middle = (b.greater_corner.z + b.lesser_corner.z) / 2.0;
	b.greater_corner.z = middle + max / 2.0;
	b.lesser_corner.z = middle - max / 2.0;
	
}

} //close namespace SFC

#endif //SFC_H
