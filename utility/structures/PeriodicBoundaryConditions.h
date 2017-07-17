/** \file PeriodicBoundaryConditions.h
 \author Graeme Lufkin (gwl@u.washington.edu)
 \date Created August 20, 2002
 \version 1.0
 */

#ifndef PERIODICBOUNDARYCONDITIONS_H
#define PERIODICBOUNDARYCONDITIONS_H

#include "Vector3D.h"

template <typename T>
class PeriodicBoundaryConditions {
public:
	T xPeriod;
	T yPeriod;
	T zPeriod;
	
	PeriodicBoundaryConditions(T xp = 0, T yp = 0, T zp = 0) : xPeriod(xp), yPeriod(yp), zPeriod(zp) { }
	
	~PeriodicBoundaryConditions() { }
	
	/// Offset between two points (p - q) with periodic boundary conditions
	inline Vector3D<T> offset(const Vector3D<T>& p, const Vector3D<T>& q) const {
		Vector3D<T> result = p - q;
		if(xPeriod > 0) {
			if(result.x > xPeriod / 2)
				result.x -= xPeriod;
			else if(result.x < -xPeriod / 2)
				result.x += xPeriod;
		}
		if(yPeriod > 0) {
			if(result.y > yPeriod / 2)
				result.y -= yPeriod;
			else if(result.y < -yPeriod / 2)
				result.y += yPeriod;
		}
		if(zPeriod > 0) {
			if(result.z > zPeriod / 2)
				result.z -= zPeriod;
			else if(result.z < -zPeriod / 2)
				result.z += zPeriod;
		}
		return result;
	}
	
	/// Distance between two points with periodic boundary conditions
	inline T distance(const Vector3D<T>& p, const Vector3D<T>& q) const {
		return offset(p, q).length();
	}
	
};

#endif //PERIODICBOUNDARYCONDITIONS_H
