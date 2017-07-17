/** \file Shape.h
 \author Graeme Lufkin (gwl@u.washington.edu)
 \date Created November 2, 2001
 \version 1.0
 */

#ifndef SPHERE_H
#define SPHERE_H

#include <iostream>

#include "Shape.h"
#include "Vector3D.h"

/// A class representing a sphere in three dimensions
template <typename T = double>
class Sphere : public Shape<T> {
public:
	/// The origin of this sphere
	Vector3D<T> origin;
	/// The radius of this sphere
	T radius;

	Sphere(const Vector3D<T>& o = Vector3D<T>(), const T r = 1) : origin(o), radius(r) { }
	
	Sphere(const Sphere<T>& s) : origin(s.origin), radius(s.radius) { }
	
	Sphere<T>& operator=(const Sphere<T>& s) {
		origin = s.origin;
		radius = s.radius;
		return *this;
	}
	
	/// Growing a sphere keeps the origin fixed and increases the radius
	void grow(const Vector3D<T>& point) {
		T points_radius = (origin - point).length();
		if(points_radius > radius)
			radius = points_radius;
	}
	
	/// The volume of a sphere is \f$ \frac{4 \pi}{3} r^3 \f$
	virtual T volume() const {
		return 4.0 * 3.14159265358979323846 / 3.0 * radius * radius * radius;
	}
	
	/// Output operator, used for formatted display
	friend std::ostream& operator<<(std::ostream& os, const Sphere<T>& s) {
		os << '{' << s.origin << ", " << s.radius << '}';
		return os;
	}

};

template <typename T = double>
class SphericalShell : public Sphere<T> {
public:
	using Sphere<T>::origin;
	using Sphere<T>::radius;
	
	/// The half-thickness of the shell (shell extends from radius-delta to radius+delta) 
	T delta;

	SphericalShell(const Vector3D<T>& o = Vector3D<T>(), const T r = 1, const T d = 0) : Sphere<T>(o, r), delta(d) { }
	
	SphericalShell(const SphericalShell<T>& s) : Sphere<T>(s.origin, s.r), delta(s.delta) { }
		
	SphericalShell<T>& operator=(const SphericalShell<T>& s) {
		origin = s.origin;
		radius = s.radius;
		delta = s.delta;
		return *this;
	}
	
	virtual T volume() const {
		T r1 = radius + delta;
		T r2 = radius - delta;
		return 4.0 * 3.14159265358979323846 / 3.0 *(r1 * r1 * r1 - r2 * r2 * r2);
	}
		
	/// Output operator, used for formatted display
	friend std::ostream& operator<<(std::ostream& os, const SphericalShell<T>& s) {
		os << '{' << s.origin << ", " << s.radius << ", " << s.delta << '}';
		return os;
	}
};

typedef Sphere<double> dSphere;

#ifdef __CHARMC__
#include "pup.h"

template <typename T>
inline void operator|(PUP::er& p, Sphere<T>& s) {
	p | s.origin;
	p | s.radius;
}

template <typename T>
inline void operator|(PUP::er& p, SphericalShell<T>& s) {
	p | s.origin;
	p | s.radius;
	p | s.delta;
}

#endif //__CHARMC__

#endif //SPHERE_H
