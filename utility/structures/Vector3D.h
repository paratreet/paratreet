/** @file Vector3D.h
 This file defines a three-dimensional vector in Cartesian coordinates, 
 as well as various vector operations.
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created March 29, 2000
 @version 2.0
 */

#ifndef VECTOR3D_H
#define VECTOR3D_H

#include <iostream> //for formatted output
#include <cmath> //for sqrt()

#include "TypeSelection.h"

/** A class containing the three cartesian coordinates of a three-dimensional vector. */
template <typename T = double>
class Vector3D {
public:
	//expose the type of the coordinates
	typedef T componentType;
	
	/// The cartesian coordinates of this three-dimensional vector
	T x, y, z;

	/// Standard constructor, initialize all coordinates to one value
	Vector3D(T a = 0) : x(a), y(a), z(a) { }
	
	/// Standard constructor, initialize all components
	Vector3D(T a, T b, T c) : x(a), y(b), z(c) { }

	/// Constructor from an array (3 elements) of values (of possibly different type!)
	template <typename T2>
	Vector3D(T2* arr) : x(*arr), y(*(arr + 1)), z(*(arr + 2)) { }

	/// Copy constructor copies the components
	template <typename T2>
	Vector3D(const Vector3D<T2>& v) : x(static_cast<T>(v.x)), y(static_cast<T>(v.y)), z(static_cast<T>(v.z)) { }

	//nothing to destruct!
	// ~Vector3D() { }

	/// Casting to a different template class (just casts the components)
	/*
	template <class T2>
	operator Vector3D<T2> () const {
		return Vector3D<T2>(x, y, z);
	}
	*/
			
	/* The only innate attribute a vector has is its length, 
	 given here as a member function. */
	
	/// The length of this vector
	inline T length() const {
		return static_cast<T>(std::sqrt(static_cast<double>(x * x + y * y + z * z)));
	}

	/// The length squared of this vector
	inline T lengthSquared() const {
		return x * x + y * y + z * z;
	}

	/// Make this vector a unit vector (length one)
	inline Vector3D<T>& normalize() {
		return *this /= length();
	}
	
	/* To manipulate Vector3D objects as expected, we need to overload a bunch of operators. */

	/// Allow array subscripting to return the components
	inline T& operator[](int index) {
		switch(index) { //if index isn't valid, x gets returned
			case 0: return x;
			case 1: return y;
			case 2: return z;
		}
		return x;
	}
	
	/// Assignment operator copies the components
	template <typename T2>
	inline Vector3D<T>& operator=(const Vector3D<T2>& v) {
		x = v.x;
		y = v.y;
		z = v.z;
		return *this;
	}
	
	/** Write the components into an array of values.
	 If we can create vectors from an array of components, we need to do the reverse.
	 The pointer indicating the start of the array should have at least two more
	 elements after it that can be set to the components of this vector.
	 */
	template <class T2>
	inline void array_form(T2* arr) const {
		arr[0] = x;
		arr[1] = y;
		arr[2] = z;
	}
		
	/// Equality operator tests all the components for equality
	inline bool operator==(const Vector3D<T>& v) const { 
		return (x == v.x) && (y == v.y) && (z == v.z);
	}

	/// Inequality operator tests all the components for inequality
	inline bool operator!=(const Vector3D<T>& v) const { 
		return (x != v.x) || (y != v.y) || (z != v.z);
	}

	/// Vector addition
	inline Vector3D<T> operator+(const Vector3D<T>& v) const {
		return Vector3D<T>(x + v.x, y + v.y, z + v.z);
	}
	
	/// Vector subtraction
	inline Vector3D<T> operator-(const Vector3D<T>& v) const {
		return Vector3D<T>(x - v.x, y - v.y, z - v.z);
	}
	
	/// Addition set-equals
	inline Vector3D<T>& operator+=(const Vector3D<T>& v) {
		x += v.x;
		y += v.y;
		z += v.z;
		return *this;
	}
	
	/// Subtraction set-equals
	inline Vector3D<T>& operator-=(const Vector3D<T>& v) {
		x -= v.x;
		y -= v.y;
		z -= v.z;
		return *this;
	}

	/// Scalar multiplication on the right
	inline Vector3D<T> operator*(const T& s) const {
		return Vector3D<T>(x * s, y * s, z * s);
	}
	
	/// Scalar multiplication on the right set-equals
	inline Vector3D<T>& operator*=(const T& s) {
		x *= s;
		y *= s;
		z *= s;
		return *this;
	}
	
	/// Scalar division on the right (can't have left division - division by a vector makes no sense!)
	inline Vector3D<T> operator/(const T& s) const {
		return Vector3D<T>(x / s, y / s, z / s);
	}
	
	/// Scalar division on the right set-equals
	inline Vector3D<T>& operator/=(const T& s) {
		x /= s;
		y /= s;
		z /= s;
		return *this;
	}
	
	/// Switch sign of all components (parity flip)
	inline Vector3D<T> operator-() const {
		return Vector3D<T>(-x, -y, -z);
	}
	
	/// Vector multiplication multiplies the components
	inline Vector3D<T> operator*(const Vector3D<T>& v) const {
		return Vector3D<T>(x * v.x, y * v.y, z * v.z);
	}

	/// Vector multiplication set-equals
	inline Vector3D<T>& operator*=(const Vector3D<T>& v) {
		x *= v.x;
		y *= v.y;
		z *= v.z;
		return *this;
	}

	/// Vector division divides the components
	inline Vector3D<T> operator/(const Vector3D<T>& v) const {
		return Vector3D<T>(x / v.x, y / v.y, z / v.z);
	}

	/// Vector division set-equals
	inline Vector3D<T>& operator/=(const Vector3D<T>& v) {
		x /= v.x;
		y /= v.y;
		z /= v.z;
		return *this;
	}
	
};

/// Scalar multiplication on the left
template <typename T, typename T2>
inline Vector3D<typename wider<T, T2>::widerType> operator*(const T2& s, const Vector3D<T>& v) {
	return Vector3D<typename wider<T, T2>::widerType>(v.x * s, v.y * s, v.z * s);
}

/// The dot product of two vectors
template <typename T, typename T2>
inline typename wider<T, T2>::widerType dot(const Vector3D<T>& a, const Vector3D<T2>& b) {
	return (a.x * b.x + a.y * b.y + a.z * b.z);
}

/// The cosine of the angle between two vectors
template <typename T, typename T2>
inline typename wider<T, T2>::widerType costheta(const Vector3D<T>& a, const Vector3D<T2>& b) {
	typename wider<T, T2>::widerType lengths_squared = a.lengthSquared() * b.lengthSquared();
	if(lengths_squared == 0)
		return 0;
	else
		return dot(a, b) / std::sqrt(lengths_squared);
}

/// The cross product of two vectors (Order is important! a x b == - b x a) */
template <typename T, typename T2>
inline Vector3D<typename wider<T, T2>::widerType> cross(const Vector3D<T>& a, const Vector3D<T2>& b) {
	return Vector3D<typename wider<T, T2>::widerType>(a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x);
}

/// Output operator, used for formatted display
template <typename T>
inline std::ostream& operator<<(std::ostream& os, const Vector3D<T>& v) {
	os << '(' << v.x << ' ' << v.y << ' ' << v.z << ')';
	//os << v.x << delim << v.y << delim << v.z;
	return os;
}

template <typename T>
inline std::istream& operator>>(std::istream& is, Vector3D<T>& v) {
	T x, y, z;
	is >> x >> y >> z;
	if(is)
		v = Vector3D<T>(x, y, z);
	return is;
}

/** Given the spherical coordinates of a vector, return a cartesion version of the same vector. */
template <typename T>
inline Vector3D<T> fromSpherical(const T r, const T theta, const T phi) {
	return Vector3D<T>(r * sin(theta) * cos(phi), r * sin(theta) * sin(phi), r * cos(theta));
}

/** Given the cylindrical coordinates of a vector, return a cartesion version of the same vector. */
template <typename T>
inline Vector3D<T> fromCylindrical(const T r, const T theta, const T z) {
	return Vector3D<T>(r * cos(theta), r * sin(theta), z);
}


/** This class represents a 3D rotation matrix specified by Euler angles.
 It is used to rotate a Vector3D object.
 The Euler angles of rotation are first a rotation of psi about the z axis.
 Then a rotation of theta about the x axis.  Finally another
 rotation about the z axis of phi.
 */
template <typename T = double>
class RotationMatrix {
public:
	T ii, ij, ik, ji, jj, jk, ki, kj, kk; //components of the matrix

	/// Set up the elements of the matrix
	RotationMatrix(const T psi, const T theta, const T phi) {
		//calculate sines and cosines first
		T cpsi = cos(psi), ctheta = cos(theta), cphi = cos(phi);
		T spsi = sin(psi), stheta = sin(theta), sphi = sin(phi);
		//construct matrix elements
		ii = cphi * cpsi - ctheta * sphi * spsi;
		ij = ctheta * cpsi * sphi + cphi * spsi;
		ik = stheta * sphi;
		ji = - cpsi * sphi - ctheta * cphi * spsi;
		jj = ctheta * cphi * cpsi - sphi * spsi;
		jk = cphi * stheta;
		ki = stheta * spsi;
		kj = -cpsi * stheta;
		kk = ctheta;
	}
	
	~RotationMatrix() { }
	
	/// Rotate a vector using this representation
	Vector3D<T> rotate(const Vector3D<T>& v) const { 
		return Vector3D<T>(ii * v.x + ij * v.y + ik * v.z,
				ji * v.x + jj * v.y + jk * v.z,
				ki * v.x + kj * v.y + kk * v.z); 
	}
};

template <typename T>
class SphericalVector3D {
public:
	T r, theta, phi;
	
	SphericalVector3D(T _r = 0, T _theta = 0, T _phi = 0) : r(_r), theta(_theta), phi(_phi) { }
	~SphericalVector3D() { }

	T length() {
		return r;
	}
	
	//allow array subscripting to return the components
	T& operator[] (int index) const {
		switch(index) { //if index isn't valid, who knows what gets returned
			case 0: return r;
			case 1: return theta;
			case 2: return phi;
		}
	}

	//assignment operator copies the components
	SphericalVector3D<T>& operator= (const SphericalVector3D<T>& v) {
		r = v.r;
		theta = v.theta;
		phi = v.phi;
		return *this;
	}

};

/** A shorthand version, for convenience. */
typedef Vector3D<double> Vector;

#ifdef __CHARMC__
#include "pup.h"
#include "charm++.h"

template <typename T>
inline void operator|(PUP::er& p, Vector3D<T>& v) {
	p | v.x;
	p | v.y;
	p | v.z;
}

class CkOStream;
template <typename T>
inline CkOStream& operator<<(CkOStream& os, const Vector3D<T>& v) {
	os << '(' << v.x << ' ' << v.y << ' ' << v.z << ')';
	//os << v.x << delim << v.y << delim << v.z;
	return os;
}
#endif //__CHARMC__

#endif //VECTOR3D_H
