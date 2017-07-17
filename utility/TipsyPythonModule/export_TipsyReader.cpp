//export_TipsyReader.cpp

#include <boost/python/class.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/operators.hpp>

#include "TipsyReader.h"
#include "TipsyReader.cpp"

using namespace boost::python;
using namespace Tipsy;

void export_TipsyReader() {
	
	class_<header>("header", init<>())
		.def(init<int, int, int>())
		//.def_readonly("sizeBytes", &header::sizeBytes)
		.def_readwrite("time", &header::time)
		.def_readwrite("nbodies", &header::nbodies)
		.def_readwrite("ndim", &header::ndim)
		.def_readwrite("nsph", &header::nsph)
		.def_readwrite("ndark", &header::ndark)
		.def_readwrite("nstar", &header::nstar)
		.def(str(self))
		;
		
	class_<TipsyReader, boost::noncopyable>("TipsyReader", init<>())
		.def(init<const std::string&>())
		.def("takeOverStream", &TipsyReader::takeOverStream)
		.def("reload", static_cast<bool (TipsyReader::*)(const std::string&) >(&TipsyReader::reload))
		.def("getHeader", &TipsyReader::getHeader)
		.def("getNextSimpleParticle", &TipsyReader::getNextSimpleParticle)
		.def("getNextGasParticle", &TipsyReader::getNextGasParticle)
		.def("getNextDarkParticle", &TipsyReader::getNextDarkParticle)
		.def("getNextStarParticle", &TipsyReader::getNextStarParticle)
		.def("isNative", &TipsyReader::isNative)
		.def("status", &TipsyReader::status)
		.def("seekParticleNum", &TipsyReader::seekParticleNum)
		.def("skipParticles", &TipsyReader::skipParticles)
		.def("tellParticleNum", &TipsyReader::tellParticleNum)
		;
}
