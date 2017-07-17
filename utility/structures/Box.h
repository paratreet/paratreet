/** \file Box.h
 \author Graeme Lufkin (gwl@u.washington.edu)
 \date Created December 3, 2001
 \version 2.0
 */

#ifndef BOX_H
#define BOX_H

#include "Vector3D.h"
#include "Shape.h"

template <typename T = double>
class Box : public Shape<T> {
public:
	Vector3D<T> center;
	Vector3D<T> axis1;
	Vector3D<T> axis2;
	Vector3D<T> axis3;
	T axis1_length, axis2_length, axis3_length;
	
	Box() : axis1_length(0), axis2_length(0), axis3_length(0) { }

	Box(const Vector3D<T>& origin, const Vector3D<T>& x, const Vector3D<T>& y, const T& z) : center(origin) {
		axis1_length = x.length();
		axis2_length = y.length();
		axis3_length = z;
		axis1 = x / axis1_length;
		axis2 = y / axis2_length;
		axis3 = cross(axis1, axis2);
	}
	
	Box(const Vector3D<T>* vertices) {
		center = 0.5 * (vertices[1] + vertices[4] + vertices[3] - vertices[0]);
		axis1 = 0.5 * (vertices[1] - vertices[0]);
		axis1_length = axis1.length();
		axis1 /= axis1_length;
		axis2 = 0.5 * (vertices[4] - vertices[0]);
		axis2_length = axis2.length();
		axis2 /= axis2_length;
		axis3 = cross(axis1, axis2);
		axis3_length = 0.5 * (vertices[3] - vertices[0]).length();
	}
	
	~Box() { }
	
	T volume() const {
		return axis1_length * axis2_length * axis3_length;
	}
	
	Vector3D<T> vertex(const int i) {
		switch(i) {
			case 0:
				return center - axis1_length * axis1 - axis2_length * axis2 - axis3_length * axis3;
			case 1:
				return center + axis1_length * axis1 - axis2_length * axis2 - axis3_length * axis3;
			case 2:
				return center + axis1_length * axis1 - axis2_length * axis2 + axis3_length * axis3;
			case 3:
				return center - axis1_length * axis1 - axis2_length * axis2 + axis3_length * axis3;
			case 4:
				return center - axis1_length * axis1 + axis2_length * axis2 - axis3_length * axis3;
			case 5:
				return center + axis1_length * axis1 + axis2_length * axis2 - axis3_length * axis3;
			case 6:
				return center + axis1_length * axis1 + axis2_length * axis2 + axis3_length * axis3;
			case 7:
				return center - axis1_length * axis1 + axis2_length * axis2 + axis3_length * axis3;
			default:
				return center;
		}
	}
	
	/// Output operator, used for formatted display
	friend std::ostream& operator<<(std::ostream& os, const Box<T>& b) {
		return os << '{' << b.center<< ", {" << b.axis1 << ", " << b.axis2 << ", " << b.axis3 << "}, (" << b.axis1_length << " " << b.axis2_length << " " << b.axis3_length << ")}";
	}
};

#ifdef __CHARMC__
#include "pup.h"

template <typename T>
inline void operator|(PUP::er& p, Box<T>& b) {
	p | b.center;
	p | b.axis1;
	p | b.axis2;
	p | b.axis3;
	p | b.axis1_length;
	p | b.axis2_length;
	p | b.axis3_length;
}

#endif //__CHARMC__

#endif //BOX_H
