//export_CodeUnits.cpp

#include <boost/python/class.hpp>
#include <boost/python/module.hpp>
#include <boost/python/def.hpp>
#include <boost/python/operators.hpp>

#include "CodeUnits.h"

using namespace boost::python;

void export_CodeUnits() {
	
	class_<CodeUnits>("CodeUnits", "A class that provides conversion factors between CGS and code units for dimensionful quantities", init<double, double>((arg("distanceUnitInCentimeters"), arg("massUnitInGrams")), "Create a CodeUnits object with the specified units"))
		.def_readonly("distanceUnitInCentimeters", &CodeUnits::distanceUnitInCentimeters)
		.def_readonly("distanceUnitInAU", &CodeUnits::distanceUnitInAU)
		.def_readonly("distanceUnitInKpc", &CodeUnits::distanceUnitInKpc)
		.def_readonly("massUnitInGrams", &CodeUnits::massUnitInGrams)
		.def_readonly("massUnitInEarthMasses", &CodeUnits::massUnitInEarthMasses)
		.def_readonly("massUnitInSolarMasses", &CodeUnits::massUnitInSolarMasses)
		.def_readonly("timeUnitInSeconds", &CodeUnits::timeUnitInSeconds)
		.def_readonly("timeUnitInYears", &CodeUnits::timeUnitInYears)
		.def_readonly("volumeUnitCGS", &CodeUnits::volumeUnitCGS)
		.def_readonly("densityUnitCGS", &CodeUnits::densityUnitCGS)
		.def_readonly("forceUnitCGS", &CodeUnits::forceUnitCGS)
		.def_readonly("pressureUnitCGS", &CodeUnits::pressureUnitCGS)
		.def_readonly("opacityUnitCGS", &CodeUnits::opacityUnitCGS)
		.def_readonly("BoltzmannConstant", &CodeUnits::BoltzmannConstant)
		.def_readonly("StefanBoltzmannConstant", &CodeUnits::StefanBoltzmannConstant)
		.def_readonly("HydrogenMass", &CodeUnits::HydrogenMass)
		.def_readonly("GasConstant", &CodeUnits::GasConstant)
		.def_readonly("BoltzmannConstantCGS", &CodeUnits::BoltzmannConstantCGS)
		.def_readonly("StefanBoltzmannConstantCGS", &CodeUnits::StefanBoltzmannConstantCGS)
		.def_readonly("GravitationalConstantCGS", &CodeUnits::GravitationalConstantCGS)
		.def_readonly("HydrogenMassCGS", &CodeUnits::HydrogenMassCGS)
		.def_readonly("GasConstantCGS", &CodeUnits::GasConstantCGS)
		.def_readonly("AU", &CodeUnits::AU)
		.def_readonly("KiloParsec", &CodeUnits::KiloParsec)
		.def_readonly("EarthMass", &CodeUnits::EarthMass)
		.def_readonly("SolarMass", &CodeUnits::SolarMass)
		.def_readonly("Year", &CodeUnits::Year)
		.def_readonly("GravitationalConstant", &CodeUnits::GravitationalConstant)
		.def(str(self))
		;
}
