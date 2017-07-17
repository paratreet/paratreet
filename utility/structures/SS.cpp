/** @file SS.cpp
 Solar System structures in C++ format.
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created February 13, 2004
 @version 1.0
 */

#include <limits>

#include "config.h"
#include "xdr_template.h"

#include "SS.h"

inline bool_t xdr_template(XDR* xdrs, SS::ss_header* val) {
	return (xdr_template(xdrs, &(val->time))
			&& xdr_template(xdrs, &(val->n_data))
			&& xdr_template(xdrs, &(val->pad)));
}

inline bool_t xdr_template(XDR* xdrs, SS::ss_particle* p) {
	return (xdr_template(xdrs, &(p->mass))
		&& xdr_template(xdrs, &(p->radius))
		&& xdr_template(xdrs, &(p->pos))
		&& xdr_template(xdrs, &(p->vel))
		&& xdr_template(xdrs, &(p->spin))
		&& xdr_template(xdrs, &(p->color))
		&& xdr_template(xdrs, &(p->org_idx)));
}

namespace SS {
	
bool SSReader::loadHeader() {
	ok = false;
	
	if(!(*ssStream))
		return false;
	
	//read the header in
	ssStream->read(reinterpret_cast<char *>(&h), ss_header::sizeBytes);
	if(!*ssStream)
		return false;
	
	XDR xdrs;
	xdrmem_create(&xdrs, reinterpret_cast<char *>(&h), ss_header::sizeBytes, XDR_DECODE);
	if(!xdr_template(&xdrs, &h))		
		return false;
	xdr_destroy(&xdrs);
	
	numParticlesRead = 0;
	ok = true;
	return ok;
}


/** Get the next particle.
 Returns false if the read failed, or already read all the particles in this file.
 */
bool SSReader::getNextParticle(ss_particle& p) {
	if(!ok || !(*ssStream))
		return false;
	
	if(numParticlesRead < h.n_data) {
		++numParticlesRead;
		ssStream->read(reinterpret_cast<char *>(&p), ss_particle::sizeBytes);
		if(!(*ssStream))
			return false;
		XDR xdrs;
		xdrmem_create(&xdrs, reinterpret_cast<char *>(&p), ss_particle::sizeBytes, XDR_DECODE);
		if(!xdr_template(&xdrs, &p))
			return false;
		xdr_destroy(&xdrs);
	} else
		return false;
	
	return true;
}
	
/** Read all the particles into vectors.
 Returns false if the read fails.
 
bool SSReader::readAllss_particles(std::vector<ss_particle>& particles) {
	if(!ok || !(*ssStream))
		return false;
	
	//go back to the beginning of the file
	if(!seekss_particleNum(0))
		return false;
	
	particles.resize(h.n_data);
	for(int i = 0; i < h.n_data; ++i) {
		if(!getNextss_particle(particles[i]))
			return false;
	}
	return true;
}
*/
/** Seek to the requested particle in the file.
 Returns false if the file seek fails, or if you request too large a particle.
 */
bool SSReader::seekParticleNum(unsigned int num) {
	if(num >= h.n_data)
		return false;
	
	int64_t seek_position = ss_header::sizeBytes + num * ss_particle::sizeBytes;
	ssStream->seekg(seek_position);
	if(!(*ssStream))
		return false;
	numParticlesRead = num;
	
	return true;
}

void SSStats::clear() {
	numParticles = 0;
	total_mass = 0;
	volume = density = 0;
	bounding_box = OrientedBox<double>();
	velocityBox = OrientedBox<double>();
	spinBox = OrientedBox<double>();
	center = size = center_of_mass = center_of_mass_velocity = angular_momentum = Vector3D<double>();
	min_mass = min_radius = HUGE_VAL;
	max_mass = max_radius = -HUGE_VAL;
	min_color = min_org_idx = std::numeric_limits<int>::max();
	max_color = max_org_idx = std::numeric_limits<int>::min();
}

void SSStats::contribute(const ss_particle& p) {
	numParticles++;
	total_mass += p.mass;
	bounding_box.grow(p.pos);
	velocityBox.grow(p.vel);
	spinBox.grow(p.spin);
	
	center_of_mass += p.mass * p.pos;
	center_of_mass_velocity += p.mass * p.vel;
	angular_momentum += p.mass * cross(p.pos, p.vel);

	if(p.mass < min_mass)
		min_mass = p.mass;
	if(p.mass > max_mass)
		max_mass = p.mass;
	if(p.radius < min_radius)
		min_radius = p.radius;
	if(p.radius > max_radius)
		max_radius = p.radius;
	if(p.color < min_color)
		min_color = p.color;
	if(p.color > max_color)
		max_color = p.color;
	if(p.org_idx < min_org_idx)
		min_org_idx = p.org_idx;
	if(p.org_idx > max_org_idx)
		max_org_idx = p.org_idx;
}

void SSStats::finalize() {
	//get the total center of mass position and velocity
	if(total_mass != 0) {
		center_of_mass /= total_mass;
		center_of_mass_velocity /= total_mass;
		angular_momentum /= total_mass;
		angular_momentum -= cross(center_of_mass, center_of_mass_velocity);
	}
			
	//get position stats
	center = bounding_box.center();
	size = bounding_box.greater_corner - bounding_box.lesser_corner;
	volume = bounding_box.volume();
	if(volume > 0)
		density = total_mass / volume;
}

std::ostream& operator<<(std::ostream& os, const SSStats& s) {
	os << "Bounding box: " << s.bounding_box;
	os << "\nBox center: " << s.center;
	os << "\nBox volume: " << s.volume;
	os << "\nBox size: " << s.size;
	os << "\nDensity: " << s.density;
	os << "\nTotal mass: " << s.total_mass;
	os << "\nTotal center of mass: " << s.center_of_mass;
	os << "\nTotal center of mass velocity: " << s.center_of_mass_velocity;
	os << "\nTotal angular momentum: " << s.angular_momentum;
	
	if(s.numParticles) {
		os << "\nStats for " << s.numParticles << " particle" << (s.numParticles > 1 ? "s" : "");
		os << "\nRange of masses: (" << s.min_mass << " <=> " << s.max_mass << ")";
		os << "\nRange of radii: (" << s.min_radius << " <=> " << s.max_radius << ")";
		os << "\nRange of colors: (" << s.min_color << " <=> " << s.max_color << ")";
		os << "\nRange of original indices: (" << s.min_org_idx << " <=> " << s.max_org_idx << ")";
	} else
		os << "\nNo particles.";
		
	return os;
}

SSFile::SSFile(std::string const& fn, int nParticles) : success(false), filename(fn), particles(nParticles) {
	h.n_data = nParticles;
	success = true;
}

SSFile::SSFile(std::string const& fn) : success(false), myReader(fn), filename(fn) {
	loadfile();
}

SSFile::SSFile(std::istream& is) : success(false), myReader(is), filename("") {
	loadfile();
}

bool SSFile::reload(std::string const& fn) {
	filename = fn;	
	myReader.reload(filename);
	return loadfile();
}

bool SSFile::reload(std::istream& is) {
	filename = "";
	myReader.reload(is);
	return loadfile();
}

bool SSFile::save() const {
	std::ofstream outfile(filename.c_str());
	if(!outfile)
		return false;
	bool result = save(outfile);
	outfile.close();
	return result;
}

// This function template copied from TipsyFile.cpp
template <typename Iterator>
void write_vector(std::ostream& os, Iterator begin, Iterator end) {
	std::size_t itemSize = std::iterator_traits<Iterator>::value_type::sizeBytes;
	for(Iterator iter = begin; iter != end; ++iter) {
		os.write(reinterpret_cast<const char *>(&(*iter)), itemSize);
		if(!os)
			return;
	}
}

bool SSFile::save(std::ostream& os) const {
	if(!os)
		return false;
	// This is more complicated than TipsyFile::save() because SS files are always big-endian, so we need to use XDR
	XDR xdrs;
	//create temporary header value
	ss_header alt_h(h);
	//create xdr writer into the memory of the temp
	xdrmem_create(&xdrs, reinterpret_cast<char *>(&alt_h), ss_header::sizeBytes, XDR_ENCODE);
	//write the XDR of the header into the temporary
	if(!xdr_template(&xdrs, &alt_h)) {
		xdr_destroy(&xdrs);
		return false;
	}
	xdr_destroy(&xdrs);
	//write the XDR'd memory to disk
	os.write(reinterpret_cast<char const*>(&alt_h), ss_header::sizeBytes);
	if(!os)
		return false;
	
	ss_particle p;
	xdrmem_create(&xdrs, reinterpret_cast<char *>(&p), ss_particle::sizeBytes, XDR_ENCODE);
	for(std::vector<ss_particle>::const_iterator iter = particles.begin(); iter != particles.end(); ++iter) {
		p = *iter;
		if(!xdr_template(&xdrs, &p)) {
			xdr_destroy(&xdrs);
			return false;
		}
		os.write(reinterpret_cast<char const*>(&p), ss_particle::sizeBytes);
		if(!os) {
			xdr_destroy(&xdrs);
			return false;
		}
		xdr_setpos(&xdrs, xdr_getpos(&xdrs) - ss_particle::sizeBytes);
	}
	
	return true;
}

bool SSFile::loadfile() {
	success = false;
	
	if(!myReader.status())
		return false;
	h = myReader.getHeader();
	
	stats.clear();
	
	particles.resize(h.n_data);
	for(std::vector<ss_particle>::iterator iter = particles.begin(); iter != particles.end(); ++iter) {
		if(!myReader.getNextParticle(*iter))
			return false;
		stats.contribute(*iter);
	}
	
	stats.finalize();
	
	success = true;
	return true;
}

} //close namespace SS
