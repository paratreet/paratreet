/** @file SFC.cpp
 Definition of key-making function.
 Must be placed in separate compilation unit because the compiler
 requires an extra flag to prevent the floats from being put into
 registers, which would mess up the calculation, which depends on
 the precise IEEE representation of 32-bit floating point numbers.
 */

#include "config.h"
#include <stdint.h>
#include "SFC.h"

namespace SFC {

  //float exchangeKey[3];

/// Out of three floats, make a Morton order (z-order) space-filling curve key
// Cannot make this function inline, or g++ with -O2 or higher generates incorrect code
/** Given the floating point numbers for the location, construct the key. 
 The key uses 21 of the 23 bits for the floats of the x, y, and z coordinates
 of the position vector.  This process will only make sense if the position
 coordinates are in the range [1,2).  The mantissa bits are taken, and interleaved
 in xyz order to form the key.  This makes the key a position on the z-ordering
 space-filling curve. 
 */
/*Key makeKey() {
  //unsigned int ix = *reinterpret_cast<unsigned int *>(&exchangeKey[0]);
  //unsigned int iy = *reinterpret_cast<unsigned int *>(&exchangeKey[1]);
  //unsigned int iz = *reinterpret_cast<unsigned int *>(&exchangeKey[2]);
  unsigned int ix = exchangeKey[0]*(1<<21) - exchangeKey[0];
  unsigned int iy = exchangeKey[1]*(1<<21) - exchangeKey[1];
  unsigned int iz = exchangeKey[2]*(1<<21) - exchangeKey[2];
	Key key = 0;
	for(unsigned int mask = (1 << 20); mask > 0; mask >>= 1) {
		key <<= 3;
		if(ix & mask)
			key += 4;
		if(iy & mask)
			key += 2;
		if(iz & mask)
			key += 1;
	}
	return key;
}

Key generateKey(const Vector3D<float>& v, const OrientedBox<float>& boundingBox) {
  Vector3D<float> d = (v - boundingBox.lesser_corner) / (boundingBox.greater_corner - boundingBox.lesser_corner); //+ Vector3D<float>(1, 1, 1);
  exchangeKey[0] = d.x;
  exchangeKey[1] = d.y;
  exchangeKey[2] = d.z;
  return makeKey();
}*/

/** Given a key, create a vector of floats representing a position.
 This is almost the inverse of the makeKey() function.  Since the key
 does not use all the bits, each float generated here will have its last
 two mantissa bits set zero, regardless of the values when the key was
 originally generated.
 */
/*Vector3D<float> makeVector(Key k) {
	//Vector3D<float> v(1.0, 1.0, 1.0);
	//int* ix = reinterpret_cast<int *>(&v.x);
	//int* iy = reinterpret_cast<int *>(&v.y);
	//int* iz = reinterpret_cast<int *>(&v.z);
  unsigned int ix=0, iy=0, iz=0;
	for(int mask = (1 << 2); mask < (1 << 23); mask <<= 1) {
		if(k & 4)
			ix |= mask;
		if(k & 2)
			iy |= mask;
		if(k & 1)
			iz |= mask;
		k >>= 3;	
	}
  float x = ((float)ix) / (1<<21);
  float y = ((float)iy) / (1<<21);
  float z = ((float)iz) / (1<<21);
	Vector3D<float> v(x, y, z);
	return v;
}*/

} //close namespace SFC
