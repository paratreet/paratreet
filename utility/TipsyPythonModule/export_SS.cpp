//export_SS.cpp

#include <boost/python/class.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/operators.hpp>

#include "SS.h"
#include "SS.cpp"

using namespace boost::python;
using namespace SS;

ss_particle& get_ss_particle(SSFile& ssf, int i) {
	return ssf.particles[i];
}

void export_SS() {
	
	class_<ss_header>("ss_header", init<>())
		//.def_readonly("sizeBytes", &ss_header::sizeBytes)
		.def_readwrite("time", &ss_header::time)
		.def_readwrite("n_data", &ss_header::n_data)
		.def_readwrite("pad", &ss_header::pad)
		.def(str(self))
		;

	class_<ss_particle>("ss_particle", init<>())
		//.def_readonly("sizeBytes", &ss_particle::sizeBytes)
		.def_readwrite("mass", &ss_particle::mass)
		.def_readwrite("radius", &ss_particle::radius)
		.def_readwrite("pos", &ss_particle::pos)
		.def_readwrite("vel", &ss_particle::vel)
		.def_readwrite("spin", &ss_particle::spin)
		.def_readwrite("color", &ss_particle::color)
		.def_readwrite("org_idx", &ss_particle::org_idx)
		.def(self == self)
		.def(str(self))
		;
		
	class_<SSReader, boost::noncopyable>("SSReader", init<>())
		.def(init<const std::string&>())
		.def("takeOverStream", &SSReader::takeOverStream)
		.def("reload", static_cast<bool (SSReader::*)(const std::string&) >(&SSReader::reload))
		.def("getHeader", &SSReader::getHeader)
		.def("status", &SSReader::status)
		.def("getNextParticle", &SSReader::getNextParticle)
		.def("seekParticleNum", &SSReader::seekParticleNum)
		.def("tellParticleNum", &SSReader::tellParticleNum)
		;
		
	class_<SSStats>("SSStats", init<>())
		.def("clear", &SSStats::clear)
		.def("contribute", &SSStats::contribute)
		.def("finalize", &SSStats::finalize)
		.def(str(self))
		.def_readonly("total_mass", &SSStats::total_mass)
		.def_readonly("volume", &SSStats::volume)
		.def_readonly("density", &SSStats::density)
		.def_readonly("bounding_box", &SSStats::bounding_box)
		.def_readonly("velocityBox", &SSStats::velocityBox)
		.def_readonly("spinBox", &SSStats::spinBox)
		.def_readonly("center", &SSStats::center)
		.def_readonly("size", &SSStats::size)
		.def_readonly("center_of_mass", &SSStats::center_of_mass)
		.def_readonly("center_of_mass_velocity", &SSStats::center_of_mass_velocity)
		.def_readonly("angular_momentum", &SSStats::angular_momentum)
		.def_readonly("min_mass", &SSStats::min_mass)
		.def_readonly("max_mass", &SSStats::max_mass)
		.def_readonly("min_radius", &SSStats::min_radius)
		.def_readonly("max_radius", &SSStats::max_radius)
		.def_readonly("min_color", &SSStats::min_color)
		.def_readonly("max_color", &SSStats::max_color)
		.def_readonly("min_org_idx", &SSStats::min_org_idx)
		.def_readonly("max_org_idx", &SSStats::max_org_idx)
		;
		
	class_<SSFile>("SSFile", init<>())
		.def(init<std::string const&, int>())
		.def(init<std::string const&>())
		.def(init<SSFile const&>())
		.def_readwrite("filename", &SSFile::filename)
		.def_readwrite("h", &SSFile::h)
		.def_readwrite("stats", &SSFile::stats)
		.def("particles", get_ss_particle, return_value_policy<reference_existing_object>())
		.def("reload", static_cast<bool (SSFile::*)(const std::string&)>(&SSFile::reload))
		.def("save", static_cast<bool (SSFile::*)() const>(&SSFile::save))
		.def("loadedSuccessfully", &SSFile::loadedSuccessfully)
		.def("remakeHeader", &SSFile::remakeHeader)
		.def("remakeStats", &SSFile::remakeStats)
		.def(str(self))
		;
}
