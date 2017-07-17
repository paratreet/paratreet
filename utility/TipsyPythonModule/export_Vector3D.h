//export_Vector3D.cpp

#include <boost/python/class.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/operators.hpp>

#include "Vector3D.h"

using namespace boost::python;

template <typename T>
std::string bare(const Vector3D<T>& v) {
	std::ostringstream oss;
	oss << v.x << " " << v.y << " " << v.z;
	return oss.str();
}

template <typename T>
void export_Vector3D(char const* className = "Vector3D") {
	
	class_<Vector3D<T> >(className, "A Cartesian vector in three dimensions", init<T, T, T>())
		.def(init<>())
		.def(init<const Vector3D<T>&>())
		.def_readwrite("x", &Vector3D<T>::x)
		.def_readwrite("y", &Vector3D<T>::y)
		.def_readwrite("z", &Vector3D<T>::z)
		.def("length", &Vector3D<T>::length)
		.def("lengthSquared", &Vector3D<T>::lengthSquared)
		.def("normalize", &Vector3D<T>::normalize, return_value_policy<reference_existing_object>())
		.def(self == self)
		.def(self != self)
		.def(self + self)
		.def(self - self)
		.def(self += self)
		.def(self -= self)
		.def(self * T())
		.def(self *= T())
		.def(self / T())
		.def(self /= T())
		.def(-self)
		.def(self * self)
		.def(self *= self)
		.def(self / self)
		.def(self /= self)
		.def(T() * self)
		.def(str(self))
		.def("bare", bare<T>, "A string containing the components of this vector")
		;
		
	def("dot", static_cast<T (*)(const Vector3D<T>&, const Vector3D<T>&)>(dot), "The dot product of two vectors");
	def("costheta",  static_cast<T (*)(const Vector3D<T>&, const Vector3D<T>&)>(costheta), "The cosine of the angle between two vectors");
	def("cross",  static_cast<Vector3D<T> (*)(const Vector3D<T>&, const Vector3D<T>&)>(cross), "The cross product of two vectors");
	
	def("fromSpherical",  static_cast<Vector3D<T> (*)(const T, const T, const T)>(fromSpherical), (arg("r"), arg("theta"), arg("phi")), "Construct a Cartesian vector from spherical components");
	def("fromCylindrical",  static_cast<Vector3D<T> (*)(const T, const T, const T)>(fromCylindrical), (arg("r"), arg("theta"), arg("z")), "Construct a Cartesian vector from cylindrical components");
	
	class_<RotationMatrix<T> >("RotationMatrix", init<const T, const T, const T>())
		.def("rotate", &RotationMatrix<T>::rotate)
		;
}
