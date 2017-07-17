//export_OrientedBox.cpp

#include <boost/python/class.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/operators.hpp>

#include "OrientedBox.h"
#include "Space.h"

using namespace boost::python;

typedef Vector3D<float> Vec;
typedef OrientedBox<float> B;
typedef Sphere<float> S;

void export_OrientedBox() {
	
	class_<B>("OrientedBox", init<const Vec&, const Vec&>())
		.def(init<>())
		.def(init<const B&>())
		.def_readwrite("lesser_corner", &B::lesser_corner)
		.def_readwrite("greater_corner", &B::greater_corner)
		.def("initialized", &B::initialized)
		.def("grow", &B::grow)
		.def("volume", &B::volume)
		.def("center", &B::center)
		.def("size", &B::size)
		.def("shift", &B::shift, return_value_policy<reference_existing_object>())
		.def(str(self))
		.def("tipsyCommand", &B::tipsyCommand)
		.def("contains", static_cast<bool (*)(const B&, const Vec&)>(&Space::contains))
		.def("intersect", static_cast<bool (*)(const B&, const B&)>(&Space::intersect))
		.def("intersect", static_cast<bool (*)(const B&, const S&)>(&Space::intersect))
		.def("enclose", static_cast<bool (*)(const B&, const B&)>(&Space::enclose))
		;
		
}
