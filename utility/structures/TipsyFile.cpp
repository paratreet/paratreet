/** @file TipsyFile.cpp
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created April 30, 2002
 @version 2.2
 */

#include <fstream>
#include <algorithm>
#include <iterator>

#include "TipsyFile.h"

namespace Tipsy {

using std::sort;
using std::unique;

TipsyStats::TipsyStats() {
	clear();
}

void TipsyStats::clear() {
	nsph = ndark = nstar = 0;
	total_mass = gas_mass = dark_mass = star_mass = 0;
	volume = density = 0;
	bounding_box = OrientedBox<double>();
	center = size = center_of_mass = gas_com = dark_com = star_com \
			= center_of_mass_velocity = gas_com_vel = dark_com_vel = star_com_vel \
			= angular_momentum = gas_ang_mom = dark_ang_mom = star_ang_mom \
			= Vector3D<double>();
	min_mass = gas_min_mass = dark_min_mass = star_min_mass = min_radius \
			= gas_min_radius = dark_min_radius = star_min_radius \
			= min_velocity = gas_min_velocity = dark_min_velocity \
			= star_min_velocity = dark_min_eps = star_min_eps = min_phi \
			= gas_min_phi = dark_min_phi = star_min_phi = gas_min_rho \
			= gas_min_temp = gas_min_hsmooth  = gas_min_metals = star_min_metals \
			= star_min_tform = HUGE_VAL;
	max_mass = gas_max_mass = dark_max_mass = star_max_mass = max_radius \
			= gas_max_radius = dark_max_radius = star_max_radius \
			= max_velocity = gas_max_velocity = dark_max_velocity \
			= star_max_velocity = dark_max_eps = star_max_eps = max_phi \
			= gas_max_phi = dark_max_phi = star_max_phi = gas_max_rho \
			= gas_max_temp = gas_max_hsmooth  = gas_max_metals = star_max_metals \
			= star_max_tform = -HUGE_VAL;
}

void TipsyStats::contribute(const gas_particle& p) {
	nsph++;
	gas_mass += p.mass;
	bounding_box.grow(p.pos);
	double poslen = p.pos.length();
	double vellen = p.vel.length();

	gas_com += p.mass * p.pos;
	gas_com_vel += p.mass * p.vel;
	gas_ang_mom += p.mass * cross(p.pos, p.vel);

	if(p.mass < gas_min_mass)
		gas_min_mass = p.mass;
	if(p.mass > gas_max_mass)
		gas_max_mass = p.mass;
	if(poslen < gas_min_radius)
		gas_min_radius = poslen;
	if(poslen > gas_max_radius)
		gas_max_radius = poslen;
	if(vellen < gas_min_velocity)
		gas_min_velocity = vellen;
	if(vellen > gas_max_velocity)
		gas_max_velocity = vellen;
	if(p.phi < gas_min_phi)
		gas_min_phi = p.phi;
	if(p.phi > gas_max_phi)
		gas_max_phi = p.phi;
	if(p.hsmooth < gas_min_hsmooth)
		gas_min_hsmooth = p.hsmooth;
	if(p.hsmooth > gas_max_hsmooth)
		gas_max_hsmooth = p.hsmooth;
	if(p.rho < gas_min_rho)
		gas_min_rho = p.rho;
	if(p.rho > gas_max_rho)
		gas_max_rho = p.rho;
	if(p.temp < gas_min_temp)
		gas_min_temp = p.temp;
	if(p.temp > gas_max_temp)
		gas_max_temp = p.temp;
	if(p.metals < gas_min_metals)
		gas_min_metals = p.metals;
	if(p.metals > gas_max_metals)
		gas_max_metals = p.metals;
}

void TipsyStats::contribute(const dark_particle& p) {
	ndark++;
	dark_mass += p.mass;
	bounding_box.grow(p.pos);
	double poslen = p.pos.length();
	double vellen = p.vel.length();

	dark_com += p.mass * p.pos;
	dark_com_vel += p.mass * p.vel;
	dark_ang_mom += p.mass * cross(p.pos, p.vel);

	if(p.mass < dark_min_mass)
		dark_min_mass = p.mass;
	if(p.mass > dark_max_mass)
		dark_max_mass = p.mass;
	if(poslen < dark_min_radius)
		dark_min_radius = poslen;
	if(poslen > dark_max_radius)
		dark_max_radius = poslen;
	if(vellen < dark_min_velocity)
		dark_min_velocity = vellen;
	if(vellen > dark_max_velocity)
		dark_max_velocity = vellen;
	if(p.eps < dark_min_eps)
		dark_min_eps = p.eps;
	if(p.eps > dark_max_eps)
		dark_max_eps = p.eps;
	if(p.phi < dark_min_phi)
		dark_min_phi = p.phi;
	if(p.phi > dark_max_phi)
		dark_max_phi = p.phi;
}

void TipsyStats::contribute(const star_particle& p) {
	nstar++;
	star_mass += p.mass;
	bounding_box.grow(p.pos);
	double poslen = p.pos.length();
	double vellen = p.vel.length();

	star_com += p.mass * p.pos;
	star_com_vel += p.mass * p.vel;
	star_ang_mom += p.mass * cross(p.pos, p.vel);

	if(p.mass < star_min_mass)
		star_min_mass = p.mass;
	if(p.mass > star_max_mass)
		star_max_mass = p.mass;
	if(poslen < star_min_radius)
		star_min_radius = poslen;
	if(poslen > star_max_radius)
		star_max_radius = poslen;
	if(vellen < star_min_velocity)
		star_min_velocity = vellen;
	if(vellen > star_max_velocity)
		star_max_velocity = vellen;
	if(p.metals < star_min_metals)
		star_min_metals = p.metals;
	if(p.metals > star_max_metals)
		star_max_metals = p.metals;
	if(p.tform < star_min_metals)
		star_min_metals = p.metals;
	if(p.metals > star_max_metals)
		star_max_metals = p.metals;
	if(p.eps < star_min_eps)
		star_min_eps = p.eps;
	if(p.eps > star_max_eps)
		star_max_eps = p.eps;
	if(p.phi < star_min_phi)
		star_min_phi = p.phi;
	if(p.phi > star_max_phi)
		star_max_phi = p.phi;
}

/// Calculate the aggregate statistics
void TipsyStats::finalize() {
	//get the total mass of the system
	total_mass = gas_mass + dark_mass + star_mass;
	
	//get the total center of mass position and velocity
	if(total_mass != 0) {
		center_of_mass = (gas_com + dark_com + star_com) / total_mass;
		center_of_mass_velocity = (gas_com_vel + dark_com_vel + star_com_vel) / total_mass;
	}
	
	//get total angular momentum about the origin
	angular_momentum = gas_ang_mom + dark_ang_mom + star_ang_mom;
	if(total_mass != 0) {
		//make specific (divide by total mass)
		angular_momentum /= total_mass;
		//subtract off the angular momentum of the center of mass
		angular_momentum -= cross(center_of_mass, center_of_mass_velocity);
	}

	//sort out the individual centers of mass and angular momenta
	if(gas_mass != 0) {
		gas_com /= gas_mass;
		gas_com_vel /= gas_mass;
		gas_ang_mom /= gas_mass;
		gas_ang_mom -= cross(gas_com, gas_com_vel);
	}
	if(dark_mass != 0) {
		dark_com /= dark_mass;
		dark_com_vel /= dark_mass;
		dark_ang_mom /= dark_mass;
		dark_ang_mom -= cross(dark_com, dark_com_vel);
	}
	if(star_mass != 0) {
		star_com /= star_mass;
		star_com_vel /= star_mass;
		star_ang_mom /= star_mass;
		star_ang_mom -= cross(star_com, star_com_vel);
	}
		
	//get position stats
        if(nsph + ndark + nstar > 0)
            center = bounding_box.center();
	size = bounding_box.greater_corner - bounding_box.lesser_corner;
	volume = bounding_box.volume();
	if(volume > 0)
		density = total_mass / volume;
	
	//figure out aggregate ranges
	min_mass = (gas_min_mass < dark_min_mass ? gas_min_mass : dark_min_mass);
	min_mass = (min_mass < star_min_mass ? min_mass : star_min_mass);
	max_mass = (gas_max_mass > dark_max_mass ? gas_max_mass : dark_max_mass);
	max_mass = (max_mass > star_max_mass ? max_mass : star_max_mass);
	min_radius = (gas_min_radius < dark_min_radius ? gas_min_radius : dark_min_radius);
	min_radius = (min_radius < star_min_radius ? min_radius : star_min_radius);
	max_radius = (gas_max_radius > dark_max_radius ? gas_max_radius : dark_max_radius);
	max_radius = (max_radius > star_max_radius ? max_radius : star_max_radius);
	min_velocity = (gas_min_velocity < dark_min_velocity ? gas_min_velocity : dark_min_velocity);
	min_velocity = (min_velocity < star_min_velocity ? min_velocity : star_min_velocity);
	max_velocity = (gas_max_velocity > dark_max_velocity ? gas_max_velocity : dark_max_velocity);
	max_velocity = (max_velocity > star_max_velocity ? max_velocity : star_max_velocity);
	min_phi = (gas_min_phi < dark_min_phi ? gas_min_phi : dark_min_phi);
	min_phi = (min_phi < star_min_phi ? min_phi : star_min_phi);
	max_phi = (gas_max_phi > dark_max_phi ? gas_max_phi : dark_max_phi);
	max_phi = (max_phi > star_max_phi ? max_phi : star_max_phi);	
}

std::ostream& operator<<(std::ostream& os, const TipsyStats& s) {
	os << "Bounding box: " << s.bounding_box;
	os << "\nBox center: " << s.center;
	os << "\nBox volume: " << s.volume;
	os << "\nBox size: " << s.size;
	os << "\nDensity: " << s.density;
	os << "\nTotal mass: " << s.total_mass;
	os << "\nTotal center of mass: " << s.center_of_mass;
	os << "\nTotal center of mass velocity: " << s.center_of_mass_velocity;
	os << "\nTotal angular momentum: " << s.angular_momentum;
	
	if(s.nsph) {
		os << "\nGas Stats: (" << s.nsph << " particle" << (s.nsph > 1 ? "s)" : ")");
		os << "\nGas mass: " << s.gas_mass;
		os << "\nGas center of mass: " << s.gas_com;
		os << "\nGas center of mass velocity: " << s.gas_com_vel;
		os << "\nGas angular momentum: " << s.gas_ang_mom;
		os << "\nRange of gas masses: (" << s.gas_min_mass << " <=> " << s.gas_max_mass << ")";
		os << "\nRange of gas radii: (" << s.gas_min_radius << " <=> " << s.gas_max_radius << ")";
		os << "\nRange of gas velocities: (" << s.gas_min_velocity << " <=> " << s.gas_max_velocity << ")";
		os << "\nRange of gas rho: (" << s.gas_min_rho << " <=> " << s.gas_max_rho << ")";
		os << "\nRange of gas temperature: (" << s.gas_min_temp << " <=> " << s.gas_max_temp << ")";
		os << "\nRange of gas hsmooth: (" << s.gas_min_hsmooth << " <=> " << s.gas_max_hsmooth << ")";
		os << "\nRange of gas metals: (" << s.gas_min_metals << " <=> " << s.gas_max_metals << ")";
		os << "\nRange of gas phi: (" << s.gas_min_phi << " <=> " << s.gas_max_phi << ")";
	} else
		os << "\nNo gas particles.";
	
	if(s.ndark) {
		os << "\nDark Stats: (" << s.ndark << " particle" << (s.ndark > 1 ? "s)" : ")");
		os << "\nDark mass: " << s.dark_mass;
		os << "\nDark center of mass: " << s.dark_com;
		os << "\nDark center of mass velocity: " << s.dark_com_vel;
		os << "\nDark angular momentum: " << s.dark_ang_mom;
		os << "\nRange of dark masses: (" << s.dark_min_mass << " <=> " << s.dark_max_mass << ")";
		os << "\nRange of dark radii: (" << s.dark_min_radius << " <=> " << s.dark_max_radius << ")";
		os << "\nRange of dark velocities: (" << s.dark_min_velocity << " <=> " << s.dark_max_velocity << ")";
		os << "\nRange of eps: (" << s.dark_min_eps << " <=> " << s.dark_max_eps << ")";
		os << "\nRange of dark phi: (" << s.dark_min_phi << " <=> " << s.dark_max_phi;
	} else
		os << "\nNo dark particles.";
	
	if(s.nstar) {
		os << "\nStar Stats: (" << s.nstar << " particle" << (s.nstar > 1 ? "s)" : ")");
		os << "\nStar mass: " << s.star_mass;
		os << "\nStar center of mass: " << s.star_com;
		os << "\nStar center of mass velocity: " << s.star_com_vel;
		os << "\nStar angular momentum: " << s.star_ang_mom;
		os << "\nRange of star masses: (" << s.star_min_mass << " <=> " << s.star_max_mass << ")";
		os << "\nRange of star radii: (" << s.star_min_radius << " <=> " << s.star_max_radius << ")";
		os << "\nRange of star velocities: (" << s.star_min_velocity << " <=> " << s.star_max_velocity << ")";
		os << "\nRange of star metals: (" << s.star_min_metals << " <=> " << s.star_max_metals << ")";
		os << "\nRange of star formation time: (" << s.star_min_tform << " <=> " << s.star_max_tform << ")";
		os << "\nRange of star eps: (" << s.star_min_eps << " <=> " << s.star_max_eps << ")";
		os << "\nRange of star phi: (" << s.star_min_phi << " <=> " << s.star_max_phi << ")";
	} else
		os << "\nNo star particles.";
	
	os << "\nAggregate Stats:";
	os << "\nRange of masses: (" << s.min_mass << " <=> " << s.max_mass << ")";
	os << "\nRange of radii: (" << s.min_radius << " <=> " << s.max_radius << ")";
	os << "\nRange of velocities: (" << s.min_velocity << " <=> " << s.max_velocity << ")";
	os << "\nRange of phi: (" << s.min_phi << " <=> " << s.max_phi << ")";
	
	return os;
}

TipsyFile::TipsyFile(const std::string& fn, int nGas, int nDark, int nStar) : native(true), success(false), filename(fn), h(nGas, nDark, nStar), gas(nGas), darks(nDark), stars(nStar) {
	success = true;
}

TipsyFile::TipsyFile(const std::string& fn) : native(true), success(false), myReader(fn), filename(fn) {
	loadfile();
}

TipsyFile::TipsyFile(std::istream& is) : native(true), success(false), myReader(is), filename("") {
	loadfile();
}

bool TipsyFile::reload(const std::string& fn) {
	filename = fn;	
	myReader.reload(filename);
	return loadfile();
}

bool TipsyFile::reload(std::istream& is) {
	filename = "";
	myReader.reload(is);
	return loadfile();
}

bool TipsyFile::saveAll() const {
	std::ofstream outfile(filename.c_str()); //open file
	if(!outfile)
		return false;
	bool result = saveAll(outfile);
	outfile.close();
	return result;
}

template <typename Iterator>
void write_vector(std::ostream& os, Iterator begin, Iterator end) {
	std::size_t itemSize = std::iterator_traits<Iterator>::value_type::sizeBytes;
	for(Iterator iter = begin; iter != end; ++iter) {
		os.write(reinterpret_cast<const char *>(&(*iter)), itemSize);
		if(!os)
			return;
	}
}

bool TipsyFile::saveAll(std::ostream& os) const {
	if(!os)
		return false;
	os.write(reinterpret_cast<const char *>(&h), sizeof(h)); //write out the header
	if(!os)
		return false;
	
	write_vector(os, gas.begin(), gas.end());
	if(!os)
		return false;
	write_vector(os, darks.begin(), darks.end());
	if(!os)
		return false;
	write_vector(os, stars.begin(), stars.end());
	if(!os)
		return false;
	else
		return true;
}

bool TipsyFile::save() const {
	std::ofstream outfile(filename.c_str());
	if(!outfile)
		return false;
	bool result = save(outfile);
	outfile.close();
	return result;
}

bool TipsyFile::save(std::ostream& os) const {
	//sort the marked indices
	sort(markedGas.begin(), markedGas.end());
	sort(markedDarks.begin(), markedDarks.end());
	sort(markedStars.begin(), markedStars.end());
	//remove any duplicate entries
	markedGas.erase(unique(markedGas.begin(), markedGas.end()), markedGas.end());
	markedDarks.erase(unique(markedDarks.begin(), markedDarks.end()), markedDarks.end());
	markedStars.erase(unique(markedStars.begin(), markedStars.end()), markedStars.end());
	header newh = h;
	newh.nsph = gas.size() - markedGas.size();
	newh.ndark = darks.size() - markedDarks.size();
	newh.nstar = stars.size() - markedStars.size();
	newh.nbodies = newh.nsph + newh.ndark + newh.nstar;

	if(!os)
		return false;
	os.write(reinterpret_cast<const char *>(&newh), sizeof(newh)); //write out the header
	if(!os)
		return false;
	
	int n;
	if(markedGas.size() == 0) {
		write_vector(os, gas.begin(), gas.end());
		if(!os)
			return false;
	} else {
		std::vector<gas_particle>::const_iterator startGas = gas.begin();
		for(std::vector<int>::iterator iter = markedGas.begin(); iter != markedGas.end(); ++iter) {
			n = gas.begin() + *iter - startGas;
			write_vector(os, startGas, startGas + n);
			if(!os)
				return false;
			startGas += n + 1;
		}
		write_vector(os, startGas, gas.end());
	}

	if(markedDarks.size() == 0) {
		write_vector(os, darks.begin(), darks.end());
		if(!os)
			return false;
	} else {
		std::vector<dark_particle>::const_iterator startDark = darks.begin();
		write_vector(os, darks.begin(), darks.end());
		for(std::vector<int>::iterator iter = markedDarks.begin(); iter != markedDarks.end(); ++iter) {
			n = darks.begin() + *iter - startDark;
			write_vector(os, startDark, startDark + n);
			if(!os)
				return false;
			startDark += n + 1;
		}
		write_vector(os, startDark, darks.end());
	}

	if(markedStars.size() == 0) {
		write_vector(os, stars.begin(), stars.end());
		if(!os)
			return false;
	} else {
		std::vector<star_particle>::const_iterator startStar = stars.begin();
		for(std::vector<int>::iterator iter = markedStars.begin(); iter != markedStars.end(); ++iter) {
			n = stars.begin() + *iter - startStar;
			write_vector(os, startStar, startStar + n);
			if(!os)
				return false;
			startStar += n + 1;
		}
		write_vector(os, startStar, stars.end());
	}
	
	return true;
}

bool TipsyFile::writeIOrdFile() {
	return writeIOrdFile(filename + std::string(".iOrd"));
}

bool TipsyFile::writeIOrdFile(const std::string& fn) {
	std::ofstream outfile(fn.c_str());
	if(!outfile)
		return false;
	bool result = writeIOrdFile(outfile);
	outfile.close();
	return result;
}

bool TipsyFile::writeIOrdFile(std::ostream& os) {
	if(!os)
		return false;
	
	//sort the marked indices
	sort(markedGas.begin(), markedGas.end());
	sort(markedDarks.begin(), markedDarks.end());
	sort(markedStars.begin(), markedStars.end());
	//remove any duplicate entries
	markedGas.erase(unique(markedGas.begin(), markedGas.end()), markedGas.end());
	markedDarks.erase(unique(markedDarks.begin(), markedDarks.end()), markedDarks.end());
	markedStars.erase(unique(markedStars.begin(), markedStars.end()), markedStars.end());

	remakeHeader();
	const char* id = "iOrd";
	//write out an identifier
	os.write(id, 4 * sizeof(char));
	if(!os)
		return false;
	//write out the next available gas index
	os.write(reinterpret_cast<const char *>(&h.nsph), sizeof(int));
	if(!os)
		return false;
	//write out the next available dark index
	os.write(reinterpret_cast<const char *>(&h.ndark), sizeof(int));
	if(!os)
		return false;
	//write out the next available star index
	os.write(reinterpret_cast<const char *>(&h.nstar), sizeof(int));
	if(!os)
		return false;
	
	std::vector<int>::iterator markedIter;
	markedIter = markedGas.begin();
	for(int i = 0; i < h.nsph; i++) {
		if(markedIter == markedGas.end()) {
			os.write(reinterpret_cast<const char *>(&i), sizeof(int));
			if(!os)
				return false;
		} else if(i == *markedIter)
			++markedIter;
		else {
			os.write(reinterpret_cast<const char *>(&i), sizeof(int));
			if(!os)
				return false;
		}
	}
	markedIter = markedDarks.begin();
	for(int i = 0; i < h.ndark; i++) {
		if(markedIter == markedDarks.end()) {
			os.write(reinterpret_cast<const char *>(&i), sizeof(int));
			if(!os)
				return false;
		} else if(i == *markedIter)
			++markedIter;
		else {
			os.write(reinterpret_cast<const char *>(&i), sizeof(int));
			if(!os)
				return false;
		}
	}
	markedIter = markedStars.begin();
	for(int i = 0; i < h.nstar; i++) {
		if(markedIter == markedStars.end()) {
			os.write(reinterpret_cast<const char *>(&i), sizeof(int));
			if(!os)
				return false;
		} else if(i == *markedIter)
			++markedIter;
		else {
			os.write(reinterpret_cast<const char *>(&i), sizeof(int));
			if(!os)
				return false;
		}
	}
	return true;
}

bool TipsyFile::loadfile() {
	success = false;
	
	if(!myReader.status())
		return false;
	h = myReader.getHeader();
	native = myReader.isNative();
	
	stats.clear();
	
	gas.resize(h.nsph);
	for(int i = 0; i < h.nsph; ++i) {
		if(!myReader.getNextGasParticle(gas[i]))
			return false;
		stats.contribute(gas[i]);
	}
	darks.resize(h.ndark);
	for(int i = 0; i < h.ndark; ++i) {
		if(!myReader.getNextDarkParticle(darks[i]))
			return false;
		stats.contribute(darks[i]);
	}
	stars.resize(h.nstar);
	for(int i = 0; i < h.nstar; ++i) {
		if(!myReader.getNextStarParticle(stars[i]))
			return false;
		stats.contribute(stars[i]);
	}
	
	stats.finalize();
	
	success = true;
	return true;
}

PartialTipsyFile::PartialTipsyFile(const std::string& fn,
				   const unsigned int begin,
				   const unsigned int end) : myReader(fn) {
	loadPartial(begin, end);
}

PartialTipsyFile::PartialTipsyFile(std::istream& is, const unsigned int begin,
				   const unsigned int end) : myReader(is) {
	loadPartial(begin, end);
}

bool PartialTipsyFile::reloadIndex(const std::string& fn,
				   const unsigned int begin,
				   const unsigned int end) {
	myReader.reload(fn);
	return loadPartial(begin, end);
}

bool PartialTipsyFile::reloadIndex(std::istream& is, const unsigned int begin,
				   const unsigned int end) {
	myReader.reload(is);
	return loadPartial(begin, end);
}

bool PartialTipsyFile::loadPartial(const unsigned int beginParticle,
				   unsigned int endParticle) {
	success = false;
	
	if(!myReader.status())
		return false;
	
	fullHeader = myReader.getHeader();
	native = myReader.isNative();
	stats.clear();
	
	//check that the range of particles asked for is reasonable
	if(beginParticle < 0 || beginParticle > fullHeader.nbodies) {
		return false;
	}
	
	//just read all the rest of the particles if endParticle is bogus
        if(endParticle < beginParticle || endParticle > fullHeader.nbodies)
		endParticle = fullHeader.nbodies;
	
	//we will need to seek at least this far in the file
	//int seekPosition = headerSizeBytes + (native ? 0 : sizeof(int));
	
	//fill in the header for our part
	h.nbodies = endParticle - beginParticle;
	h.ndim = fullHeader.ndim;
	h.time = fullHeader.time;
	//figure out the left and right boundaries of each type of particle
	h.nsph = std::max(std::min(fullHeader.nsph, endParticle) - (int64_t) beginParticle, (int64_t) 0);
	h.ndark = std::max(std::min(endParticle, fullHeader.nsph + fullHeader.ndark) - (int64_t) std::max(fullHeader.nsph, beginParticle), (int64_t) 0);
	h.nstar = std::max(endParticle - (int64_t) std::max(fullHeader.nsph + fullHeader.ndark, beginParticle), (int64_t) 0);
	
	if(!myReader.seekParticleNum(beginParticle))
		return false;
	
	gas.resize(h.nsph);
	for(int i = 0; i < h.nsph; ++i) {
		if(!myReader.getNextGasParticle(gas[i]))
			return false;
		stats.contribute(gas[i]);
	}
	darks.resize(h.ndark);
	for(int i = 0; i < h.ndark; ++i) {
		if(!myReader.getNextDarkParticle(darks[i]))
			return false;
		stats.contribute(darks[i]);
	}
	stars.resize(h.nstar);
	for(int i = 0; i < h.nstar; ++i) {
		if(!myReader.getNextStarParticle(stars[i]))
			return false;
		stats.contribute(stars[i]);
	}

	stats.finalize();
	
	success = true;
	return true;
}

PartialTipsyFile::PartialTipsyFile(const std::string& fn, const std::string& markfilename) : myReader(fn) {
	std::ifstream markfile(markfilename.c_str());
	loadMark(markfile);
}

PartialTipsyFile::PartialTipsyFile(std::istream& is, std::istream& markstream) : myReader(is) {
	loadMark(markstream);
}

bool PartialTipsyFile::reloadMark(const std::string& fn, const std::string& markfilename) {
	myReader.reload(fn);
	std::ifstream markstream(markfilename.c_str());
	return loadMark(markstream);
}

bool PartialTipsyFile::reloadMark(std::istream& is, std::istream& markstream) {
	myReader.reload(is);
	return loadMark(markstream);
}

bool PartialTipsyFile::loadMark(std::istream& markstream) {
	success = false;
	
	if(!myReader.status())
		return false;
	
	fullHeader = myReader.getHeader();
	native = myReader.isNative();
	stats.clear();
	
	h.ndim = fullHeader.ndim;
	h.time = fullHeader.time;
	
	markstream >> h.nbodies;
	if(h.nbodies != fullHeader.nbodies)
		return false;
	markstream >> h.nsph;
	if(h.nsph != fullHeader.nsph)
		return false;
	markstream >> h.nstar;
	if(h.nstar != fullHeader.nstar)
		return false;
	
	h.nsph = h.ndark = h.nstar = 0;
	std::vector<int> marks;
	int markIndex;
	while(markstream >> markIndex) {
		markIndex--; //mark inidices start at 1
		if(markIndex < 0 || markIndex >= h.nbodies)
			return false;
		marks.push_back(markIndex);
		if(markIndex < fullHeader.nsph)
			h.nsph++;
		else if(markIndex < fullHeader.nsph + fullHeader.ndark)
			h.ndark++;
		else
			h.nstar++;
	}
		
	h.nbodies = h.nsph + h.ndark + h.nstar;
	gas.clear();
	darks.clear();
	stars.clear();
	gas.reserve(h.nsph);
	darks.reserve(h.ndark);
	stars.reserve(h.nstar);
	
	gas_particle gp;
	dark_particle dp;
	star_particle sp;
	
	for(std::vector<int>::iterator iter = marks.begin(); iter != marks.end(); ++iter) {
		if(!myReader.seekParticleNum(*iter))
			return false;
		if(*iter < fullHeader.nsph) {
			if(!myReader.getNextGasParticle(gp))
				return false;
			stats.contribute(gp);
			gas.push_back(gp);
		} else if(*iter < fullHeader.nsph + fullHeader.ndark) {
			if(!myReader.getNextDarkParticle(dp))
				return false;
			stats.contribute(dp);
			darks.push_back(dp);
		} else {
			if(!myReader.getNextStarParticle(sp))
				return false;
			stats.contribute(sp);
			stars.push_back(sp);
		}
	}
	
	stats.finalize();
	
	success = true;
	return true;
}

std::vector<Real> readTipsyArray(std::istream& is) {
	int num;
	is >> num;
	std::vector<Real> bad;
	if(!is || (num <= 0))
		return bad;
	std::vector<Real> arrayvals(num);
	for(int i = 0; i < num; ++i) {
		is >> arrayvals[i];
		if(!is)
			return bad;
	}
	return arrayvals;
}

std::vector<Vector3D<Real> > readTipsyVector(std::istream& is) {
	int num;
	is >> num;
	std::vector<Vector3D<Real> > bad;
	if(!is || (num <= 0))
		return bad;
	std::vector<Vector3D<Real> > vectorvals(num);
	int i;
	for(i = 0; i < num; ++i) {
		is >> vectorvals[i].x;
		if(!is)
			return bad;
	}
	for(i = 0; i < num; ++i) {
		is >> vectorvals[i].y;
		if(!is)
			return bad;
	}
	for(i = 0; i < num; ++i) {
		is >> vectorvals[i].z;
		if(!is)
			return bad;
	}
	return vectorvals;
}

} //close namespace Tipsy
