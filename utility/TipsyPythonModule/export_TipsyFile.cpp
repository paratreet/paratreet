//export_TipsyFile.cpp

#include <boost/python/class.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/operators.hpp>

#include "TipsyFile.h"
#include "TipsyFile.cpp"

using namespace boost::python;
using namespace Tipsy;

template <typename TF>
gas_particle& get_gas_particle(TF& tf, const int i) {
	return tf.gas[i];
}

template <typename TF>
dark_particle& get_dark_particle(TF& tf, const int i) {
	return tf.darks[i];
}

template <typename TF>
star_particle& get_star_particle(TF& tf, const int i) {
	return tf.stars[i];
}

void export_TipsyFile() {
	
	class_<TipsyStats>("TipsyStats", init<>())
		.def("clear", &TipsyStats::clear)
		.def("contribute", static_cast<void (TipsyStats::*)(const gas_particle&)>(&TipsyStats::contribute))
		.def("contribute", static_cast<void (TipsyStats::*)(const dark_particle&)>(&TipsyStats::contribute))
		.def("contribute", static_cast<void (TipsyStats::*)(const star_particle&)>(&TipsyStats::contribute))
		.def("finalize", &TipsyStats::finalize)
		.def(str(self))
		.def_readonly("total_mass", &TipsyStats::total_mass)
		.def_readonly("gas_mass", &TipsyStats::gas_mass)
		.def_readonly("dark_mass", &TipsyStats::dark_mass)
		.def_readonly("star_mass", &TipsyStats::star_mass)
		.def_readonly("volume", &TipsyStats::volume)
		.def_readonly("density", &TipsyStats::density)
		.def_readonly("bounding_box", &TipsyStats::bounding_box)
		.def_readonly("center", &TipsyStats::center)
		.def_readonly("size", &TipsyStats::size)
		.def_readonly("center_of_mass", &TipsyStats::center_of_mass)
		.def_readonly("gas_com", &TipsyStats::gas_com)
		.def_readonly("dark_com", &TipsyStats::dark_com)
		.def_readonly("star_com", &TipsyStats::star_com)
		.def_readonly("center_of_mass_velocity", &TipsyStats::center_of_mass_velocity)
		.def_readonly("gas_com_vel", &TipsyStats::gas_com_vel)
		.def_readonly("dark_com_vel", &TipsyStats::dark_com_vel)
		.def_readonly("star_com_vel", &TipsyStats::star_com_vel)
		.def_readonly("angular_momentum", &TipsyStats::angular_momentum)
		.def_readonly("gas_ang_mom", &TipsyStats::gas_ang_mom)
		.def_readonly("dark_ang_mom", &TipsyStats::dark_ang_mom)
		.def_readonly("star_ang_mom", &TipsyStats::star_ang_mom)
		.def_readonly("min_mass", &TipsyStats::min_mass)
		.def_readonly("max_mass", &TipsyStats::max_mass)
		.def_readonly("gas_min_mass", &TipsyStats::gas_min_mass)
		.def_readonly("gas_max_mass", &TipsyStats::gas_max_mass)
		.def_readonly("dark_min_mass", &TipsyStats::dark_min_mass)
		.def_readonly("dark_max_mass", &TipsyStats::dark_max_mass)
		.def_readonly("star_min_mass", &TipsyStats::star_min_mass)
		.def_readonly("star_max_mass", &TipsyStats::star_max_mass)
		.def_readonly("min_radius", &TipsyStats::min_radius)
		.def_readonly("max_radius", &TipsyStats::max_radius)
		.def_readonly("gas_min_radius", &TipsyStats::gas_min_radius)
		.def_readonly("gas_max_radius", &TipsyStats::gas_max_radius)
		.def_readonly("dark_min_radius", &TipsyStats::dark_min_radius)
		.def_readonly("dark_max_radius", &TipsyStats::dark_max_radius)
		.def_readonly("star_min_radius", &TipsyStats::star_min_radius)
		.def_readonly("star_max_radius", &TipsyStats::star_max_radius)
		.def_readonly("min_velocity", &TipsyStats::min_velocity)
		.def_readonly("max_velocity", &TipsyStats::max_velocity)
		.def_readonly("gas_min_velocity", &TipsyStats::gas_min_velocity)
		.def_readonly("gas_max_velocity", &TipsyStats::gas_max_velocity)
		.def_readonly("dark_min_velocity", &TipsyStats::dark_min_velocity)
		.def_readonly("dark_max_velocity", &TipsyStats::dark_max_velocity)
		.def_readonly("star_min_velocity", &TipsyStats::star_min_velocity)
		.def_readonly("star_max_velocity", &TipsyStats::star_max_velocity)
		.def_readonly("dark_min_eps", &TipsyStats::dark_min_eps)
		.def_readonly("dark_max_eps", &TipsyStats::dark_max_eps)
		.def_readonly("star_min_eps", &TipsyStats::star_min_eps)
		.def_readonly("star_max_eps", &TipsyStats::star_max_eps)
		.def_readonly("min_phi", &TipsyStats::min_phi)
		.def_readonly("max_phi", &TipsyStats::max_phi)
		.def_readonly("gas_min_phi", &TipsyStats::gas_min_phi)
		.def_readonly("gas_max_phi", &TipsyStats::gas_max_phi)
		.def_readonly("dark_min_phi", &TipsyStats::dark_min_phi)
		.def_readonly("dark_max_phi", &TipsyStats::dark_max_phi)
		.def_readonly("star_min_phi", &TipsyStats::star_min_phi)
		.def_readonly("star_max_phi", &TipsyStats::star_max_phi)
		.def_readonly("gas_min_rho", &TipsyStats::gas_min_rho)
		.def_readonly("gas_max_rho", &TipsyStats::gas_max_rho)
		.def_readonly("gas_min_temp", &TipsyStats::gas_min_temp)
		.def_readonly("gas_max_temp", &TipsyStats::gas_max_temp)
		.def_readonly("gas_min_hsmooth", &TipsyStats::gas_min_hsmooth)
		.def_readonly("gas_max_hsmooth", &TipsyStats::gas_max_hsmooth)
		.def_readonly("gas_min_metals", &TipsyStats::gas_min_metals)
		.def_readonly("gas_max_metals", &TipsyStats::gas_max_metals)
		.def_readonly("star_min_metals", &TipsyStats::star_min_metals)
		.def_readonly("star_max_metals", &TipsyStats::star_max_metals)
		.def_readonly("star_min_tform", &TipsyStats::star_min_tform)
		.def_readonly("star_max_tform", &TipsyStats::star_max_tform)
		;
	
	class_<TipsyFile>("TipsyFile", init<>())
		.def(init<const std::string&, int, int, int>())
		.def(init<const std::string&>())
		.def(init<const TipsyFile&>())
		.def_readwrite("filename", &TipsyFile::filename)
		.def_readwrite("h", &TipsyFile::h)
		.def_readwrite("stats", &TipsyFile::stats)
		.def("gas", static_cast<gas_particle& (*)(TipsyFile&, const int)>(get_gas_particle), return_value_policy<reference_existing_object>())
		.def("darks", static_cast<dark_particle& (*)(TipsyFile&, const int)>(get_dark_particle), return_value_policy<reference_existing_object>())
		.def("stars", static_cast<star_particle& (*)(TipsyFile&, const int)>(get_star_particle), return_value_policy<reference_existing_object>())
		.def("reload", static_cast<bool (TipsyFile::*)(const std::string&)>(&TipsyFile::reload))
		.def("save", static_cast<bool (TipsyFile::*)() const>(&TipsyFile::save))
		.def("writeIOrdFile", static_cast<bool (TipsyFile::*)(const std::string&)>(&TipsyFile::writeIOrdFile))
		.def("writeIOrdFile", static_cast<bool (TipsyFile::*)()>(&TipsyFile::writeIOrdFile))
		.def("saveAll", static_cast<bool (TipsyFile::*)() const>(&TipsyFile::saveAll))
		.def("addGasParticle", &TipsyFile::addGasParticle)
		.def("addDarkParticle", &TipsyFile::addDarkParticle)
		.def("addStarParticle", &TipsyFile::addStarParticle)
		.def("markGasForDeletion", &TipsyFile::markGasForDeletion)
		.def("markDarkForDeletion", &TipsyFile::markDarkForDeletion)
		.def("markStarForDeletion", &TipsyFile::markStarForDeletion)
		.def("isNative", &TipsyFile::isNative)
		.def("loadedSuccessfully", &TipsyFile::loadedSuccessfully)
		.def("remakeHeader", &TipsyFile::remakeHeader)
		.def("remakeStats", &TipsyFile::remakeStats)
		.def(str(self))
		;
		
	class_<PartialTipsyFile>("PartialTipsyFile", init<>())
		.def(init<const PartialTipsyFile&>())
		.def(init<const std::string&, const int, const int>())
		.def(init<const std::string&, const std::string&>())
		//.def(init<const std::string&, const Sphere<double>&>())
		//.def(init<const std::string&, const OrientedBox<double>&>())
		.def_readwrite("fullHeader", &PartialTipsyFile::fullHeader)
		.def_readwrite("h", &PartialTipsyFile::h)
		.def_readwrite("stats", &PartialTipsyFile::stats)
		.def("gas", static_cast<gas_particle& (*)(PartialTipsyFile&, const int)>(get_gas_particle), return_value_policy<reference_existing_object>())
		.def("darks", static_cast<dark_particle& (*)(PartialTipsyFile&, const int)>(get_dark_particle), return_value_policy<reference_existing_object>())
		.def("stars", static_cast<star_particle& (*)(PartialTipsyFile&, const int)>(get_star_particle), return_value_policy<reference_existing_object>())
		.def("reloadIndex", static_cast<bool (PartialTipsyFile::*)(const std::string&, int, int)>(&PartialTipsyFile::reloadIndex))
		.def("reloadMark", static_cast<bool (PartialTipsyFile::*)(const std::string&, const std::string&)>(&PartialTipsyFile::reloadMark))
		.def("isNative", &PartialTipsyFile::isNative)
		.def("loadedSuccessfully", &PartialTipsyFile::loadedSuccessfully)
		;	

}
