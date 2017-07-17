//export_Sphere.cpp

#include <boost/python/class.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/operators.hpp>

#include "Sphere.h"
#include "Space.h"

using namespace boost::python;

typedef Vector3D<float> Vec;
typedef OrientedBox<float> B;
typedef Sphere<float> S;

void export_Sphere() {
	
	class_<S>("Sphere", init<const Vec&, const float>())
		.def(init<>())
		.def(init<const S&>())
		.def_readwrite("origin", &S::origin)
		.def_readwrite("radius", &S::radius)
		.def("grow", &S::grow)
		.def("volume", &S::volume)
		.def(str(self))
		.def("contains", static_cast<bool (*)(const S&, const Vec&)>(&Space::contains))
		.def("intersect", static_cast<bool (*)(const S&, const S&)>(&Space::intersect))
		.def("intersect", static_cast<bool (*)(const S&, const B&)>(&Space::intersect))
		;
		
}
