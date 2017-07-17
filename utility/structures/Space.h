/** @file Space.h
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created April 11, 2003
 @version 1.0
 @todo Fully document this class
 */

#ifndef SPACE_H
#define SPACE_H

#include <iostream>
#include <cmath>

#include "Vector3D.h"
#include "Sphere.h"
#include "OrientedBox.h"
#include "Box.h"
#include "TypeSelection.h"

template <typename T = double>
class PeriodicSpace {
public:
	
	T xPeriod;
	T yPeriod;
	T zPeriod;
	
	PeriodicSpace(const T& xp, const T& yp, const T& zp) : xPeriod(xp), yPeriod(xp), zPeriod(zp) { }
	PeriodicSpace(const Vector3D<T>& v) : xPeriod(v.x), yPeriod(v.y), zPeriod(v.z) { }
	
	/// Offset between two points (p - q), accounting for  periodic boundary conditions
	template <typename T2>
	inline Vector3D<T2> offset(const Vector3D<T2>& p, const Vector3D<T2>& q) const {
		Vector3D<T2> result = p - q;
		if(xPeriod > 0) {
			result.x = std::fmod(result.x, xPeriod);
			if(result.x > xPeriod / 2)
				result.x -= xPeriod;
			else if(result.x < -xPeriod / 2)
				result.x += xPeriod;
		}
		if(yPeriod > 0) {
			result.y = std::fmod(result.y, yPeriod);
			if(result.y > yPeriod / 2)
				result.y -= yPeriod;
			else if(result.y < -yPeriod / 2)
				result.y += yPeriod;
		}
		if(zPeriod > 0) {
			result.z = std::fmod(result.z, zPeriod);
			if(result.z > zPeriod / 2)
				result.z -= zPeriod;
			else if(result.z < -zPeriod / 2)
				result.z += zPeriod;
		}
		return result;
	}
	
	/// Distance between two points, accounting for periodic boundary conditions
	template <typename T2>
	inline T2 distance(const Vector3D<T2>& p, const Vector3D<T2>& q) const {
		return offset(p, q).length();
	}
	
	/// Does a sphere contain a point, accounting for periodic boundary conditions
	template <typename T2>
	inline bool contains(const Sphere<T2>& s, const Vector3D<T2>& v) const {
		return distance(s.origin, v) <= s.radius;
	}

	/// Does an oriented box contain a point, accounting for periodic boundary conditions
	template <typename T2, typename T3>
	inline bool contains(const OrientedBox<T2>& b, const Vector3D<T3>& v) const {
		T2 d;
		if((v.x < b.lesser_corner.x || v.x > b.greater_corner.x) && xPeriod > 0) {
			d = std::fmod(v.x - (b.lesser_corner.x + b.greater_corner.x) / 2, xPeriod);
			if(d > xPeriod / 2)
				d -= xPeriod;
			else if(d < -xPeriod / 2)
				d += xPeriod;
			if(std::fabs(d) > (b.greater_corner.x - b.lesser_corner.x) / 2)
				return false;
		}
		if((v.y < b.lesser_corner.y || v.y > b.greater_corner.y) && yPeriod > 0) {
			d = std::fmod(v.y - (b.lesser_corner.y + b.greater_corner.y) / 2, yPeriod);
			if(d > yPeriod / 2)
				d -= yPeriod;
			else if(d < -yPeriod / 2)
				d += yPeriod;
			if(std::fabs(d) > (b.greater_corner.y - b.lesser_corner.y) / 2)
				return false;
		}
		if((v.z < b.lesser_corner.z || v.z > b.greater_corner.z) && zPeriod > 0) {
			d = std::fmod(v.z - (b.lesser_corner.z + b.greater_corner.z) / 2, zPeriod);
			if(d > zPeriod / 2)
				d -= zPeriod;
			else if(d < -zPeriod / 2)
				d += zPeriod;
			if(std::fabs(d) > (b.greater_corner.z - b.lesser_corner.z) / 2)
				return false;
		}
		return true;
	}

	/// Do two oriented boxes intersect, accounting for periodic boundary conditions
	template <typename T2, typename T3>
	bool intersect(const OrientedBox<T2>& box1, const OrientedBox<T3>& box2) const {
		T2 d;
		if((box1.lesser_corner.x > box2.greater_corner.x || box1.greater_corner.x < box2.lesser_corner.x) && xPeriod > 0) {
			d = std::fmod((box1.lesser_corner.x + box1.greater_corner.x - box2.lesser_corner.x - box2.greater_corner.x) / 2, xPeriod);
			if(d > xPeriod / 2)
				d -= xPeriod;
			else if(d < -xPeriod / 2)
				d += xPeriod;
			if(std::fabs(d) > (box1.greater_corner.x - box1.lesser_corner.x + box2.greater_corner.x - box2.lesser_corner.x) / 2)
				return false;
		}
		if((box1.lesser_corner.y > box2.greater_corner.y || box1.greater_corner.y < box2.lesser_corner.y) && yPeriod > 0) {
			d = std::fmod((box1.lesser_corner.y + box1.greater_corner.y - box2.lesser_corner.y - box2.greater_corner.y) / 2, yPeriod);
			if(d > yPeriod / 2)
				d -= yPeriod;
			else if(d < -yPeriod / 2)
				d += yPeriod;
			if(std::fabs(d) > (box1.greater_corner.y - box1.lesser_corner.y + box2.greater_corner.y - box2.lesser_corner.y) / 2)
				return false;
		}
		if((box1.lesser_corner.z > box2.greater_corner.z || box1.greater_corner.z < box2.lesser_corner.z) && zPeriod > 0) {
			d = std::fmod((box1.lesser_corner.z + box1.greater_corner.z - box2.lesser_corner.z - box2.greater_corner.z) / 2, zPeriod);
			if(d > zPeriod / 2)
				d -= zPeriod;
			else if(d < -zPeriod / 2)
				d += zPeriod;
			if(std::fabs(d) > (box1.greater_corner.z - box1.lesser_corner.z + box2.greater_corner.z - box2.lesser_corner.z) / 2)
				return false;
		}
		return true;		
	}
};

class Space {
public:
	
	template <typename T>
	static inline Vector3D<T> offset(const Vector3D<T>& p, const Vector3D<T>& q) {
		return p - q;
	}
	
	template <typename T>	
	static inline T distance(const Vector3D<T>& p, const Vector3D<T>& q) {
		return (p - q).length();
	}

	template <typename T, typename T2>
	static inline bool contains(const Sphere<T>& s, const Vector3D<T2>& v) {
		return (s.origin - v).length() <= s.radius;
	}

	template <typename T, typename T2>
	static inline bool contains(const SphericalShell<T>& s, const Vector3D<T2>& v) {
		typename wider<T, T2>::widerType len = (s.origin - v).length();
		return (s.radius - s.delta < len) && (len < s.radius + s.delta);
	}

	/// Does an oriented box contain a point
	template <typename T, typename T2>
	static inline bool contains(const OrientedBox<T>& b, const Vector3D<T2>& v) {
		return v.x >= b.lesser_corner.x && v.x <= b.greater_corner.x
				&& v.y >= b.lesser_corner.y && v.y <= b.greater_corner.y
				&& v.z >= b.lesser_corner.z && v.z <= b.greater_corner.z;
	}

	/// Does an oriented box contain a sphere
	template <typename T, typename T2>
	static inline bool contains(const OrientedBox<T>& b, const Sphere<T2>& s) {
		return s.origin.x - s.radius >= b.lesser_corner.x
		    && s.origin.x + s.radius <= b.greater_corner.x
		    && s.origin.y - s.radius >= b.lesser_corner.y
		    && s.origin.y + s.radius <= b.greater_corner.y
		    && s.origin.z - s.radius >= b.lesser_corner.z
		    && s.origin.z + s.radius <= b.greater_corner.z;
	}

	/// Does an unaligned box contain a point
	template <typename T, typename T2>
	static inline bool contains(const Box<T>& box, const Vector3D<T2>& v) {
		typename wider<T, T2>::widerType a, b, c;
		Vector3D<typename wider<T, T2>::widerType> v_minus_center = v - box.center;
		a = dot(v_minus_center, box.axis1);
		b = dot(v_minus_center, box.axis2);
		c = dot(v_minus_center, box.axis3);
		return -box.axis1_length <= a && a <= box.axis1_length
				&& -box.axis2_length <= b && b <= box.axis2_length
				&& -box.axis3_length <= c && c <= box.axis3_length;
	}

	/// Do two oriented boxes intersect
	template <typename T, typename T2>
	static bool intersect(const OrientedBox<T>& box1, const OrientedBox<T2>& box2) {
		return !(box1.lesser_corner.x > box2.greater_corner.x || box1.greater_corner.x < box2.lesser_corner.x
				|| box1.lesser_corner.y > box2.greater_corner.y || box1.greater_corner.y < box2.lesser_corner.y
				|| box1.lesser_corner.z > box2.greater_corner.z || box1.greater_corner.z < box2.lesser_corner.z);
	}
	
	/// Do two spheres intersect
	template <typename T, typename T2>
	static bool intersect(const Sphere<T>& s1, const Sphere<T2>& s2) {
		return distance(s1.origin, s2.origin) <= s1.radius + s2.radius;
	}
	
	/// Do an oriented box and a sphere intersect
	template <typename T, typename T2>
	static bool intersect(const OrientedBox<T>& b, const Sphere<T2>& s) {
		T dsq = 0;
		T rsq = s.radius * s.radius;
		T delta;
		if((delta = b.lesser_corner.x - s.origin.x) > 0)
			dsq += delta * delta;
		else if((delta = s.origin.x - b.greater_corner.x) > 0)
			dsq += delta * delta;
		if(rsq < dsq)
			return false;
		if((delta = b.lesser_corner.y - s.origin.y) > 0)
			dsq += delta * delta;
		else if((delta = s.origin.y - b.greater_corner.y) > 0)
			dsq += delta * delta;
		if(rsq < dsq)
			return false;
		if((delta = b.lesser_corner.z - s.origin.z) > 0)
			dsq += delta * delta;
		else if((delta = s.origin.z - b.greater_corner.z) > 0)
			dsq += delta * delta;
		return (dsq <= s.radius * s.radius);
	}
	
	/// Reverse order of arguments
	template <typename T, typename T2>
	static bool intersect(const Sphere<T>& s, const OrientedBox<T2>& box1) {
		return intersect(box1, s);
	}

	template <typename T, typename T2>
	static bool contained(const OrientedBox<T>& b, const Sphere<T2>& s) {
    Vector3D<T> vec;
		T delta1;
    T delta2;

    delta1 = b.lesser_corner.x - s.origin.x;
    delta2 = s.origin.x - b.greater_corner.x;
		if(delta1 > 0)
      vec.x = b.greater_corner.x;
    else if(delta2 > 0)
      vec.x = b.lesser_corner.x;
    else{
      delta1 = -delta1;
      delta2 = -delta2;
      if(delta1>=delta2)
        vec.x = b.lesser_corner.x;
      else
        vec.x = b.greater_corner.x;
    }
  
    delta1 = b.lesser_corner.y - s.origin.y;
    delta2 = s.origin.y - b.greater_corner.y;
		if(delta1 > 0)
      vec.y = b.greater_corner.y;
    else if(delta2 > 0)
      vec.y = b.lesser_corner.y;
    else{
      delta1 = -delta1;
      delta2 = -delta2;
      if(delta1>=delta2)
        vec.y = b.lesser_corner.y;
      else
        vec.y = b.greater_corner.y;
    }
  
    delta1 = b.lesser_corner.z - s.origin.z;
    delta2 = s.origin.z - b.greater_corner.z;
		if(delta1 > 0)
      vec.z = b.greater_corner.z;
    else if(delta2 > 0)
      vec.z = b.lesser_corner.z;
    else{
      delta1 = -delta1;
      delta2 = -delta2;
      if(delta1>=delta2)
        vec.z = b.lesser_corner.z;
      else
        vec.z = b.greater_corner.z;
    }
  
    if(contains(s,vec))
      return true;
    else
      return false;
  }

	/// Does the first box enclose the second
	template <typename T, typename T2>
	static inline bool enclose(const OrientedBox<T>& box1, const OrientedBox<T2>& box2) {
		return box1.lesser_corner.x <= box2.lesser_corner.x
				&& box1.lesser_corner.y <= box2.lesser_corner.y
				&& box1.lesser_corner.z <= box2.lesser_corner.z
				&& box1.greater_corner.x >= box2.greater_corner.x
				&& box1.greater_corner.y >= box2.greater_corner.y
				&& box1.greater_corner.z >= box2.greater_corner.z;
	}
	
};

template <typename T = double>
class Space3D {
public:
	
	bool periodic;

	T xPeriod;
	T yPeriod;
	T zPeriod;
	
	Space3D(const T& xp = 0, const T& yp = 0, const T& zp = 0) : xPeriod(xp), yPeriod(xp), zPeriod(zp) {
		if(xPeriod > 0 || yPeriod > 0 || zPeriod > 0)
			periodic = true;
	}
	
	Space3D(const Vector3D<T>& v) : xPeriod(v.x), yPeriod(v.y), zPeriod(v.z) {
		if(xPeriod > 0 || yPeriod > 0 || zPeriod > 0)
			periodic = true;
	}
	
	Space3D(const Space3D<T>& s) : periodic(s.periodic), xPeriod(s.xPeriod), yPeriod(s.yPeriod), zPeriod(s.zPeriod) { }
	

	/// Offset between two points (p - q), possibly accounting for  periodic boundary conditions
	template <typename T2>
	inline Vector3D<T2> offset(const Vector3D<T2>& p, const Vector3D<T2>& q) const {
		Vector3D<T2> result = p - q;
		if(periodic) {
			if(xPeriod > 0) {
				result.x = std::fmod(result.x, xPeriod);
				if(result.x > xPeriod / 2)
					result.x -= xPeriod;
				else if(result.x < -xPeriod / 2)
					result.x += xPeriod;
			}
			if(yPeriod > 0) {
				result.y = std::fmod(result.y, yPeriod);
				if(result.y > yPeriod / 2)
					result.y -= yPeriod;
				else if(result.y < -yPeriod / 2)
					result.y += yPeriod;
			}
			if(zPeriod > 0) {
				result.z = std::fmod(result.z, zPeriod);
				if(result.z > zPeriod / 2)
					result.z -= zPeriod;
				else if(result.z < -zPeriod / 2)
					result.z += zPeriod;
			}
		}
		return result;
	}
	
	
	/// Distance between two points, possibly accounting for periodic boundary conditions
	template <typename T2>
	inline T2 distance(const Vector3D<T2>& p, const Vector3D<T2>& q) const {
		return offset(p, q).length();
	}
	
	/// Does a sphere contain a point, possibly accounting for periodic boundary conditions
	template <typename T2>
	inline bool contains(const Sphere<T2>& s, const Vector3D<T2>& v) const {
		return distance(s.origin, v) <= s.radius;
	}

	/// Does a spherical shell contain a point, possibly accounting for periodic boundary conditions
	template <typename T2>
	inline bool contains(const SphericalShell<T2>& s, const Vector3D<T2>& v) const {
		return distance(s.origin, v) <= s.radius;
	}

	/// Does an oriented box contain a point, possibly accounting for periodic boundary conditions
	template <typename T2, typename T3>
	inline bool contains(const OrientedBox<T2>& b, const Vector3D<T3>& v) const {
		if(periodic) {
			T2 d;
			if((v.x < b.lesser_corner.x || v.x > b.greater_corner.x) && xPeriod > 0) {
				d = std::fmod(v.x - (b.lesser_corner.x + b.greater_corner.x) / 2, xPeriod);
				if(d > xPeriod / 2)
					d -= xPeriod;
				else if(d < -xPeriod / 2)
					d += xPeriod;
				if(std::fabs(d) > (b.greater_corner.x - b.lesser_corner.x) / 2)
					return false;
			}
			if((v.y < b.lesser_corner.y || v.y > b.greater_corner.y) && yPeriod > 0) {
				d = std::fmod(v.y - (b.lesser_corner.y + b.greater_corner.y) / 2, yPeriod);
				if(d > yPeriod / 2)
					d -= yPeriod;
				else if(d < -yPeriod / 2)
					d += yPeriod;
				if(std::fabs(d) > (b.greater_corner.y - b.lesser_corner.y) / 2)
					return false;
			}
			if((v.z < b.lesser_corner.z || v.z > b.greater_corner.z) && zPeriod > 0) {
				d = std::fmod(v.z - (b.lesser_corner.z + b.greater_corner.z) / 2, zPeriod);
				if(d > zPeriod / 2)
					d -= zPeriod;
				else if(d < -zPeriod / 2)
					d += zPeriod;
				if(std::fabs(d) > (b.greater_corner.z - b.lesser_corner.z) / 2)
					return false;
			}
			return true;
		} else 
			return v.x >= b.lesser_corner.x && v.x <= b.greater_corner.x
					&& v.y >= b.lesser_corner.y && v.y <= b.greater_corner.y
					&& v.z >= b.lesser_corner.z && v.z <= b.greater_corner.z;
	}
	
	/// Do two oriented boxes intersect, possibly accounting for periodic boundary conditions
	template <typename T2, typename T3>
	inline bool intersect(const OrientedBox<T2>& box1, const OrientedBox<T3>& box2) const {
		if(periodic) {
			T2 d;
			if((box1.lesser_corner.x > box2.greater_corner.x || box1.greater_corner.x < box2.lesser_corner.x) && xPeriod > 0) {
				d = std::fmod((box1.lesser_corner.x + box1.greater_corner.x - box2.lesser_corner.x - box2.greater_corner.x) / 2, xPeriod);
				if(d > xPeriod / 2)
					d -= xPeriod;
				else if(d < -xPeriod / 2)
					d += xPeriod;
				if(std::fabs(d) > (box1.greater_corner.x - box1.lesser_corner.x + box2.greater_corner.x - box2.lesser_corner.x) / 2)
					return false;
			}
			if((box1.lesser_corner.y > box2.greater_corner.y || box1.greater_corner.y < box2.lesser_corner.y) && yPeriod > 0) {
				d = std::fmod((box1.lesser_corner.y + box1.greater_corner.y - box2.lesser_corner.y - box2.greater_corner.y) / 2, yPeriod);
				if(d > yPeriod / 2)
					d -= yPeriod;
				else if(d < -yPeriod / 2)
					d += yPeriod;
				if(std::fabs(d) > (box1.greater_corner.y - box1.lesser_corner.y + box2.greater_corner.y - box2.lesser_corner.y) / 2)
					return false;
			}
			if((box1.lesser_corner.z > box2.greater_corner.z || box1.greater_corner.z < box2.lesser_corner.z) && zPeriod > 0) {
				d = std::fmod((box1.lesser_corner.z + box1.greater_corner.z - box2.lesser_corner.z - box2.greater_corner.z) / 2, zPeriod);
				if(d > zPeriod / 2)
					d -= zPeriod;
				else if(d < -zPeriod / 2)
					d += zPeriod;
				if(std::fabs(d) > (box1.greater_corner.z - box1.lesser_corner.z + box2.greater_corner.z - box2.lesser_corner.z) / 2)
					return false;
			}
			return true;
		} else
			return !(box1.lesser_corner.x > box2.greater_corner.x || box1.greater_corner.x < box2.lesser_corner.x
					|| box1.lesser_corner.y > box2.greater_corner.y || box1.greater_corner.y < box2.lesser_corner.y
					|| box1.lesser_corner.z > box2.greater_corner.z || box1.greater_corner.z < box2.lesser_corner.z);
	}
	
	/// Do an oriented box and a sphere intersect
	template <typename T2, typename T3>
	inline bool intersect(const OrientedBox<T2>& b, const Sphere<T3>& s) const {
		if(periodic) {
			return false;
		} else {
			T2 dsq = 0;
			T2 rsq = s.radius * s.radius;
			T2 delta;
			if((delta = b.lesser_corner.x - s.origin.x) > 0)
				dsq += delta * delta;
			else if((delta = s.origin.x - b.greater_corner.x) > 0)
				dsq += delta * delta;
			if(rsq < dsq)
				return false;
			if((delta = b.lesser_corner.y - s.origin.y) > 0)
				dsq += delta * delta;
			else if((delta = s.origin.y - b.greater_corner.y) > 0)
				dsq += delta * delta;
			if(rsq < dsq)
				return false;
			if((delta = b.lesser_corner.z - s.origin.z) > 0)
				dsq += delta * delta;
			else if((delta = s.origin.z - b.greater_corner.z) > 0)
				dsq += delta * delta;
			return (dsq <= s.radius * s.radius);
		}
	}
	
	/// Reverse order of arguments
	template <typename T2, typename T3>
	inline bool intersect(const Sphere<T2>& s, const OrientedBox<T3>& box1) const {
		return intersect(box1, s);
	}
	
	/// Does the first box enclose the second
	template <typename T2, typename T3>
	inline bool enclose(const OrientedBox<T2>& box1, const OrientedBox<T3>& box2) const {
		if(periodic) {
			return false;
		} else 
			return box1.lesser_corner.x <= box2.lesser_corner.x
					&& box1.lesser_corner.y <= box2.lesser_corner.y
					&& box1.lesser_corner.z <= box2.lesser_corner.z
					&& box1.greater_corner.x >= box2.greater_corner.x
					&& box1.greater_corner.y >= box2.greater_corner.y
					&& box1.greater_corner.z >= box2.greater_corner.z;
	}
};

#ifdef __CHARMC__
#include "pup.h"

template <typename T>
inline void operator|(PUP::er& p, Space3D<T>& s) {
	p | s.xPeriod;
	p | s.yPeriod;
	p | s.zPeriod;
}

#endif //__CHARMC__

#endif //SPACE_H
