/** @file TipsyFile.h
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created April 30, 2002
 @version 2.2
 */
 
#ifndef TIPSYFILE_H
#define TIPSYFILE_H

#include <iostream>
#include <vector>
#include <string>

#include "TipsyParticles.h"
#include "TipsyReader.h"
#include "Vector3D.h"
#include "OrientedBox.h"
#include "Sphere.h"

namespace Tipsy {

/** This class holds statistics about a tipsy file. */
class TipsyStats {
	int nsph, ndark, nstar;
public:
	
	TipsyStats();
	
	double total_mass;
	double gas_mass, dark_mass, star_mass;
	double volume;
	double density;
	OrientedBox<double> bounding_box;
	
	Vector3D<double> center;
	Vector3D<double> size;
	
	Vector3D<double> center_of_mass;
	Vector3D<double> gas_com;
	Vector3D<double> dark_com;
	Vector3D<double> star_com;
	Vector3D<double> center_of_mass_velocity;
	Vector3D<double> gas_com_vel;
	Vector3D<double> dark_com_vel;
	Vector3D<double> star_com_vel;
	Vector3D<double> angular_momentum;
	Vector3D<double> gas_ang_mom;
	Vector3D<double> dark_ang_mom;
	Vector3D<double> star_ang_mom;
	
	float min_mass, max_mass;
	float gas_min_mass, gas_max_mass, dark_min_mass, dark_max_mass, star_min_mass, star_max_mass;
	double min_radius, max_radius;
	double gas_min_radius, gas_max_radius, dark_min_radius, dark_max_radius, star_min_radius, star_max_radius;
	double min_velocity, max_velocity;
	double gas_min_velocity, gas_max_velocity, dark_min_velocity, dark_max_velocity, star_min_velocity, star_max_velocity;
	float dark_min_eps, dark_max_eps, star_min_eps, star_max_eps;
	float min_phi, max_phi;
	float gas_min_phi, gas_max_phi, dark_min_phi, dark_max_phi, star_min_phi, star_max_phi;
	float gas_min_rho, gas_max_rho;
	float gas_min_temp, gas_max_temp;
	float gas_min_hsmooth, gas_max_hsmooth;
	float gas_min_metals, gas_max_metals, star_min_metals, star_max_metals;
	float star_min_tform, star_max_tform;
	
	void clear();
	void contribute(const gas_particle& p);
	void contribute(const dark_particle& p);
	void contribute(const star_particle& p);
	void finalize();
	
	/// Output operator, used for formatted display
	friend std::ostream& operator<<(std::ostream& os, const TipsyStats& p);
};

/** This class represents a tipsy format file in memory. */
class TipsyFile {
private:

	/// Load the file from disk
	bool loadfile();
	
	//must be mutable so they can be sorted in save(), a const function
	mutable std::vector<int> markedGas;
	mutable std::vector<int> markedDarks;
	mutable std::vector<int> markedStars;
	
	bool native;

	bool success;
	
	TipsyReader myReader;
	
public:
	/// The filename of this tipsy file
	std::string filename;

	/// The header of this tipsy file
	header h;
	
	/// Statistics about this tipsy file
	TipsyStats stats;
	
	/// The array of gas particles
	std::vector<gas_particle> gas;
	/// The array of dark matter particles
	std::vector<dark_particle> darks;
	/// The array of star particles
	std::vector<star_particle> stars;
		
	TipsyFile() : native(true), success(false) { }

	/// Create a blank tipsy file with specified number of particles
	TipsyFile(const std::string& fn, int nGas, int nDark = 0, int nStar = 0);
	
	/// Load from a file
	TipsyFile(const std::string& fn);
	
	/// Load from a stream
	TipsyFile(std::istream& is);
	
	/// Copy constructor
	TipsyFile(const TipsyFile& tf) : markedGas(tf.markedGas), markedDarks(tf.markedDarks), markedStars(tf.markedStars), native(tf.native), success(tf.success), filename(tf.filename), h(tf.h), stats(tf.stats), gas(tf.gas), darks(tf.darks), stars(tf.stars) { }
	
	~TipsyFile() { }
	
	TipsyFile& operator=(const TipsyFile& tf) {
		// a TipsyFile object has already read in its particles, so it doesn't need to pass its TipsyReader around
		markedGas = tf.markedGas;
		markedDarks = tf.markedDarks;
		markedStars = tf.markedStars;
		native = tf.native;
		success = tf.success;
		filename = tf.filename;
		h = tf.h;
		stats = tf.stats;
		gas = tf.gas;
		darks = tf.darks;
		stars = tf.stars;
		return *this;
	}
	
	/// Load a new file into this object, destroying the original one
	bool reload(const std::string& fn);
	bool reload(std::istream& is);
	
	/// Save the current state to the given filename, not writing out particles marked for deletion
	bool save() const;
	/// Save the current state to a stream
	bool save(std::ostream& os) const;
	
	bool writeIOrdFile();
	bool writeIOrdFile(const std::string& fn);
	bool writeIOrdFile(std::ostream& os);
	
	/// Save the current state, including particles marked for deletion
	bool saveAll() const;
	bool saveAll(std::ostream& os) const;
	
	void addGasParticle(const gas_particle& p = gas_particle()) {
		gas.push_back(p);
		remakeHeader();
	}
	
	void addDarkParticle(const dark_particle& p = dark_particle()) {
		darks.push_back(p);
		remakeHeader();
	}
	
	void addStarParticle(const star_particle& p = star_particle()) {
		stars.push_back(p);
		remakeHeader();
	}
	
	bool markGasForDeletion(const int i) {
		if(i < 0 || i >= h.nsph)
			return false;
		markedGas.push_back(i);
		return true;
	}
	
	bool markDarkForDeletion(const int i) {
		if(i < 0 || i >= h.ndark)
			return false;
		markedDarks.push_back(i);
		return true;
	}

	bool markStarForDeletion(const int i) {
		if(i < 0 || i >= h.nstar)
			return false;
		markedStars.push_back(i);
		return true;
	}
	
	/// Is this file stored in native byte-order?
	bool isNative() const { return native; }

	/// Did the file load successfully?
	bool loadedSuccessfully() const { return success; }
	
	bool operator!() const { return !success; }
	
	void remakeHeader() {
		h.nsph = gas.size();
		h.ndark = darks.size();
		h.nstar = stars.size();
		h.nbodies = h.nsph + h.ndark + h.nstar;
	}
	
	void remakeStats() {
		stats.clear();
		for(int i = 0; i < h.nsph; ++i)
			stats.contribute(gas[i]);
		for(int i = 0; i < h.ndark; ++i)
			stats.contribute(darks[i]);
		for(int i = 0; i < h.nstar; ++i)
			stats.contribute(stars[i]);
		stats.finalize();
	}
	
	/// Output operator, used for formatted display
	friend std::ostream& operator<<(std::ostream& os, const TipsyFile& tf) {
		os << "tipsy file " << tf.filename;
		if(tf.isNative())
			os << "\nx86 file format (little-endian)";
		else
			os << "\nSGI file format (big-endian)";
		os << "\nSimulation time: " << tf.h.time;
		os << "\nNBodies: " << tf.h.nbodies << " (" << tf.h.nsph << " gas, " << tf.h.ndark << " dark, " << tf.h.nstar << " star)\n";
		os << tf.stats;
		return os;
	}
};

/** This class represents part of a tipsy file loaded into memory as if it were
 the whole thing.  It can be used in cases of low memory, or to split up one file
 among several processes.  It is essentially a read-only structure.  i.e., you
 cannot save, add particles, or remove particles from a PartialTipsyFile.  */
class PartialTipsyFile {
private:
		
	bool loadPartial(const unsigned int beginParticle,
			 const unsigned int endParticle);
	bool loadMark(std::istream& markstream);
	bool native;
	bool success;
	
	TipsyReader myReader;
	
public:

	/// The header for the full file
	header fullHeader;
	/// The header for the part of the file we hold
	header h;
	/// Statistics about this part of the tipsy file
	TipsyStats stats;
	
	/// The array of gas particles
	std::vector<gas_particle> gas;
	/// The array of dark matter particles
	std::vector<dark_particle> darks;
	/// The array of star particles
	std::vector<star_particle> stars;
	
	PartialTipsyFile() : native(true), success(false) { }
	PartialTipsyFile(const PartialTipsyFile& ptf) : native(ptf.native), success(ptf.success), fullHeader(ptf.fullHeader), h(ptf.h), stats(ptf.stats), gas(ptf.gas), darks(ptf.darks), stars(ptf.stars) { }
	PartialTipsyFile& operator=(const PartialTipsyFile& ptf) {
		native = ptf.native;
		success = ptf.success;
		fullHeader = ptf.fullHeader;
		h = ptf.h;
		stats = ptf.stats;
		gas = ptf.gas;
		darks = ptf.darks;
		stars = ptf.stars;
		return *this;
	}
	
	/// Read in part of a tipsy file, taking particles between given on-disk indices
	PartialTipsyFile(const std::string& fn, const unsigned int begin = 0,
			 const unsigned int end = 1);
	PartialTipsyFile(std::istream& is, const unsigned int begin = 0,
			 const unsigned int end = 1);

	//reloading a partial file from indices
	bool reloadIndex(const std::string& fn, const unsigned int begin = 0,
			 const unsigned int end = 1);
	bool reloadIndex(std::istream& is, const unsigned int begin = 0, const unsigned int end = 1);
		
	/// Load only the marked particles from a file
	PartialTipsyFile(const std::string& fn, const std::string& markfilename);
	PartialTipsyFile(std::istream& is, std::istream& markstream);
	
	//reloading a partial file from a mark file
	bool reloadMark(const std::string& fn, const std::string& markfilename);
	bool reloadMark(std::istream& is, std::istream& markstream);
	
	/// Load only the particles in the given region from a file
	PartialTipsyFile(const std::string& fn, const Sphere<Real>& s);
	PartialTipsyFile(const std::string& fn, const OrientedBox<Real>& b);
	
	
	/// Is this file stored in native byte-order?
	bool isNative() const { return native; }

	/// Did the file load successfully?
	bool loadedSuccessfully() const { return success; }
	
	bool operator!() const { return !success; }
        /// double precision info
        bool isDoublePos() const { return myReader.isDoublePos();}
        bool isDoubleVel() const { return myReader.isDoubleVel();}
            
};


std::vector<Real> readTipsyArray(std::istream& is);
std::vector<Vector3D<Real> > readTipsyVector(std::istream& is);

} //close namespace Tipsy

#endif //TIPSYFILE_H
