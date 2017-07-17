//CodeUnits.h

#ifndef CODEUNITS_H__hsdf07j253v89724tvuhi23r2t3vuj23v
#define CODEUNITS_H__hsdf07j253v89724tvuhi23r2t3vuj23v

#include <cmath>
#include <ostream>

/** This class encapsulates the conversion factors from a set of simulation units to CGS. 
 To use the conversions, instantiate a \c CodeUnits object with the unit of length in
 centimeters and the unit of mass in grams.  By requiring that the gravitational
 constant G be equal to 1, this specifies the unit of time.  Several useful additional
 conversion factors are derived from the three fundamental conversions.  Temperatures
 are always measured in Kelvin.
 To convert a quantity from code units to CGS, multiply it by the conversion factor
 for that unit.  To go from CGS to code units, divide the quantity by the factor.
*/
class CodeUnits {
public:
	
	//Some physical constants in CGS units
	static const double BoltzmannConstantCGS; //1.38065E-16; //k_b in grams cm^2 per Kelvin per second^2
	static const double StefanBoltzmannConstantCGS; //5.6704E-5;  // \sigma in grams per Kelvin^4 per second^3
	static const double GravitationalConstantCGS; //6.673E-8;
	static const double HydrogenMassCGS; //1.67339E-24; //m_H
	static const double GasConstantCGS; //8.2506188037E7; //k_B / m_H
	static const double AU; //= 1.4959787066E13; //in cm
	static const double KiloParsec; //3.0857E21; //in cm
	static const double EarthMass; //5.9742E27; //in grams
	static const double SolarMass; //= 1.9891E33; //in grams
	static const double Year; //3.1536E7; //in seconds
	
	//force G == 1 in code units
	static const double GravitationalConstant;// = 1.0;

	//Conversions for the unit of distance to commonly used units
	double distanceUnitInCentimeters;
	double distanceUnitInAU;
	double distanceUnitInKpc;
	
	//Conversions for the unit of mass to commonly used units
	double massUnitInGrams;
	double massUnitInEarthMasses;
	double massUnitInSolarMasses;
	
	//Conversions for the unit of time to commonly used units
	double timeUnitInSeconds;
	double timeUnitInYears;
	
	//Conversions for various quantities, built out of the fundamental conversions
	double volumeUnitCGS;
	double densityUnitCGS;
	double forceUnitCGS;
	double pressureUnitCGS;
	double opacityUnitCGS;
	
	// Values of physical constants in code units
	double BoltzmannConstant;
	double StefanBoltzmannConstant;
	double HydrogenMass;
	double GasConstant;
	
	/// Define the system of units by specifying the unit of distance in centimeters and the unit of mass in grams (by requiring that G = 1, this specifies the unit of time).
	CodeUnits(const double distanceUnit = 0, const double massUnit = 0) : 
			distanceUnitInCentimeters(distanceUnit), 
			massUnitInGrams(massUnit), 
			timeUnitInSeconds(sqrt(distanceUnit * distanceUnit * distanceUnit / massUnit / GravitationalConstantCGS))
			{
		
		distanceUnitInAU = distanceUnitInCentimeters / AU;
		distanceUnitInKpc = distanceUnitInCentimeters / KiloParsec;
		massUnitInEarthMasses = massUnitInGrams / EarthMass;
		massUnitInSolarMasses = massUnitInGrams / SolarMass;
		timeUnitInYears = timeUnitInSeconds / Year;
		
		volumeUnitCGS = distanceUnitInCentimeters * distanceUnitInCentimeters * distanceUnitInCentimeters;
		densityUnitCGS = massUnitInGrams / volumeUnitCGS;
		forceUnitCGS = massUnitInGrams * distanceUnitInCentimeters / timeUnitInSeconds / timeUnitInSeconds;
		pressureUnitCGS = massUnitInGrams / distanceUnitInCentimeters / timeUnitInSeconds / timeUnitInSeconds;
		opacityUnitCGS = distanceUnitInCentimeters * distanceUnitInCentimeters / massUnitInGrams;
		
		BoltzmannConstant = BoltzmannConstantCGS / massUnitInGrams / distanceUnitInCentimeters / distanceUnitInCentimeters * timeUnitInSeconds * timeUnitInSeconds;
		StefanBoltzmannConstant = StefanBoltzmannConstantCGS / massUnitInGrams * timeUnitInSeconds * timeUnitInSeconds * timeUnitInSeconds;
		HydrogenMass = HydrogenMassCGS / massUnitInGrams;
		GasConstant = BoltzmannConstant / HydrogenMass;
	}
	
};

std::ostream& operator<<(std::ostream& os, const CodeUnits& units) {
	os << "Fundamental quantities that define the system of units, along with G = 1" << "\n";
	os << "Distance unit in centimeters: " << units.distanceUnitInCentimeters << "\n";
	os << "Distance unit in AU: " << units.distanceUnitInAU << "\n";
	os << "Distance unit in kiloparsecs: " << units.distanceUnitInKpc << "\n";
	os << "Mass unit in grams: " << units.massUnitInGrams << "\n";
	os << "Mass unit in Earth masses: " << units.massUnitInEarthMasses << "\n";
	os << "Mass unit in Solar masses: " << units.massUnitInSolarMasses << "\n";
	os << "Time unit in seconds: " << units.timeUnitInSeconds << "\n";
	os << "Time unit in years: " << units.timeUnitInYears << "\n";

	os << "Compound quantities in CGS" << "\n";
	os << "Volume unit: " << units.volumeUnitCGS << "\n";
	os << "Density unit: " << units.densityUnitCGS << "\n";
	os << "Force unit: " << units.forceUnitCGS << "\n";
	os << "Pressure unit: " << units.pressureUnitCGS << "\n";
	os << "Opacity unit: " << units.opacityUnitCGS << "\n";
	
	os << "Physical constants in system units" << "\n";
	os << "Boltzmann constant k_B: " << units.BoltzmannConstant << "\n";
	os << "Stefan-Boltzmann constant \\sigma: " << units.StefanBoltzmannConstant << "\n";
	os << "Hydrogen mass m_H: " << units.HydrogenMass << "\n";
	os << "Gas constant k_B / m_H: " << units.GasConstant << "\n";
	return os;
}

//Need definitions of static const members to be outside the class declaration so that Boost.Python module links in symbols correctly
const double CodeUnits::BoltzmannConstantCGS = 1.38065E-16; //k_b in grams cm^2 per Kelvin per second^2
const double CodeUnits::StefanBoltzmannConstantCGS = 5.6704E-5;  // \sigma in grams per Kelvin^4 per second^3
const double CodeUnits::GravitationalConstantCGS = 6.673E-8; //G in cm^3 per gram per second^2
const double CodeUnits::HydrogenMassCGS = 1.67339E-24; //m_H in grams
const double CodeUnits::GasConstantCGS = 8.2506188037E7; //k_B / m_H in cm^2 per Kelvin per second^2
const double CodeUnits::AU = 1.4959787066E13; //in cm
const double CodeUnits::KiloParsec = 3.0857E21; //in cm
const double CodeUnits::EarthMass = 5.9742E27; //in grams
const double CodeUnits::SolarMass = 1.9891E33; //in grams
const double CodeUnits::Year = 3.1536E7; //in seconds

const double CodeUnits::GravitationalConstant = 1.0;

#endif //CODEUNITS_H__hsdf07j253v89724tvuhi23r2t3vuj23v
