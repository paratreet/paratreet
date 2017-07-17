//export_TipsyParticles.cpp

#include <boost/python/class.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/operators.hpp>

#include "TipsyParticles.h"

using namespace boost::python;
using namespace Tipsy;

void export_TipsyParticles() {
	
	class_<simple_particle>("simple_particle", init<>())
		//.def_readonly("sizeBytes", &simple_particle::sizeBytes)
		.def_readwrite("mass", &simple_particle::mass)
		.def_readwrite("pos", &simple_particle::pos)
		.def_readwrite("vel", &simple_particle::vel)
		.def(self == self)
		.def(str(self))
		;
		
	class_<gas_particle>("gas_particle", init<>())
		//.def_readonly("sizeBytes", &gas_particle::sizeBytes)
		.def_readwrite("mass", &gas_particle::mass)
		.def_readwrite("pos", &gas_particle::pos)
		.def_readwrite("vel", &gas_particle::vel)
		.def_readwrite("rho", &gas_particle::rho)
		.def_readwrite("temp", &gas_particle::temp)
		.def_readwrite("hsmooth", &gas_particle::hsmooth)
		.def_readwrite("eps", &gas_particle::hsmooth)
		.def_readwrite("metals", &gas_particle::metals)
		.def_readwrite("phi", &gas_particle::phi)
		.def(self == self)
		.def(str(self))
		;

	class_<dark_particle>("dark_particle", init<>())
		//.def_readonly("sizeBytes", &dark_particle::sizeBytes)
		.def_readwrite("mass", &dark_particle::mass)
		.def_readwrite("pos", &dark_particle::pos)
		.def_readwrite("vel", &dark_particle::vel)
		.def_readwrite("eps", &dark_particle::eps)
		.def_readwrite("phi", &dark_particle::phi)
		.def(self == self)
		.def(str(self))
		;

	class_<star_particle>("star_particle", init<>())
		//.def_readonly("sizeBytes", &star_particle::sizeBytes)
		.def_readwrite("mass", &star_particle::mass)
		.def_readwrite("pos", &star_particle::pos)
		.def_readwrite("vel", &star_particle::vel)
		.def_readwrite("metals", &star_particle::metals)
		.def_readwrite("tform", &star_particle::tform)
		.def_readwrite("eps", &star_particle::eps)
		.def_readwrite("phi", &star_particle::phi)
		.def(self == self)
		.def(str(self))
		;
		
	class_<group_particle>("group_particle", init<>())
		.def_readwrite("groupNumber", &group_particle::groupNumber)
		.def_readwrite("total_mass", &group_particle::total_mass)
		.def_readwrite("reference", &group_particle::reference)
		.def_readwrite("cm", &group_particle::cm)
		.def_readwrite("cm_velocity", &group_particle::cm_velocity)
		.def_readwrite("numMembers", &group_particle::numMembers)
		.def_readwrite("radius", &group_particle::radius)
		.def(self == self)
		.def(str(self))
		;
		
}
