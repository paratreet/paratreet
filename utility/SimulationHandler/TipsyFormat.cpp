/** @file TipsyFormat.cpp
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created November, 2003
 @version 1.0
 */
 
#include "config.h"
#include "TipsyFormat.h"

namespace SimulationHandling {

using namespace std;
using namespace Tipsy;

TipsyFormatReader::TipsyFormatReader(const string& filename) {
	if(!loadFromTipsyFile(filename))
	    cerr << "loadFromTipsyFile failed\n" ;
}

bool TipsyFormatReader::loadFromTipsyFile(const string& filename) {
	//erase any previous contents
	release();
	clear();
	
	name = filename;
	
	try {
	    r.reload(filename);
	    }
	catch (std::ios_base::failure e) {
	    return false;
	    }
	if(!r.status())
		return false;
	
	h = r.getHeader();
	time = h.time;
	
	float* empty = 0;
	Vector3D<float>* emptyVector = 0;
	
	if(h.nsph > 0) {
		ParticleFamily family;
		family.familyName = "gas";
		family.count.numParticles = 0;
		family.count.startParticle = 0;
		family.count.totalNumParticles = h.nsph;
		
		family.addAttribute("mass", TypedArray(empty, 0));
		family.addAttribute("density", TypedArray(empty, 0));
		family.addAttribute("temperature", TypedArray(empty, 0));
		family.addAttribute("softening", TypedArray(empty, 0));
		family.addAttribute("metals", TypedArray(empty, 0));
		family.addAttribute("potential", TypedArray(empty, 0));
		family.addAttribute("position", TypedArray(emptyVector, 0));
		family.addAttribute("velocity", TypedArray(emptyVector, 0));
		
		insert(make_pair("gas", family));
	}
	if(h.ndark > 0) {
		ParticleFamily family;
		family.familyName = "dark";
		family.count.numParticles = 0;
		family.count.startParticle = 0;
		family.count.totalNumParticles = h.ndark;

		family.addAttribute("mass", TypedArray(empty, 0));
		family.addAttribute("softening", TypedArray(empty, 0));
		family.addAttribute("potential", TypedArray(empty, 0));
		family.addAttribute("position", TypedArray(emptyVector, 0));
		family.addAttribute("velocity", TypedArray(emptyVector, 0));

		insert(make_pair("dark", family));
	}
	if(h.nstar > 0) {
		ParticleFamily family;
		family.familyName = "star";
		family.count.numParticles = 0;
		family.count.startParticle = 0;
		family.count.totalNumParticles = h.nstar;

		family.addAttribute("mass", TypedArray(empty, 0));
		family.addAttribute("metals", TypedArray(empty, 0));
		family.addAttribute("formationtime", TypedArray(empty, 0));
		family.addAttribute("softening", TypedArray(empty, 0));
		family.addAttribute("potential", TypedArray(empty, 0));
		family.addAttribute("position", TypedArray(emptyVector, 0));
		family.addAttribute("velocity", TypedArray(emptyVector, 0));

		insert(make_pair("star", family));
	}
	
	return true;
}

bool TipsyFormatReader::loadAttribute(const string& familyName, const string& attributeName, int64_t numParticles, const u_int64_t startParticle) {
	iterator familyIter = find(familyName);
	if(familyIter == end())
		return false;
	ParticleFamily& family = familyIter->second;
	if(numParticles < 0)
		numParticles = family.count.totalNumParticles;
	if(numParticles + startParticle > family.count.totalNumParticles)
		return false;
	//is this the first attribute loaded?
	if(family.count.numParticles == 0) {
		family.count.numParticles = numParticles;
		family.count.startParticle = startParticle;
		//load all attributes for the request family bunch
		if(familyName == "gas") {
			if(!r.seekParticleNum(startParticle))
				return false;
			float* masses = new float[numParticles];
			Vector3D<float>* positions = new Vector3D<float>[numParticles];
			Vector3D<float>* velocities = new Vector3D<float>[numParticles];
			float* densities = new float[numParticles];
			float* temperatures = new float[numParticles];
			float* metals = new float[numParticles];
			float* softenings = new float[numParticles];
			float* potentials = new float[numParticles];
			int64_t* index = new int64_t[numParticles];
			gas_particle gp;
			for(int64_t i = 0; i < numParticles; ++i) {
				if(!r.getNextGasParticle(gp))
					return false;
				masses[i] = gp.mass;
				positions[i] = gp.pos;
				velocities[i] = gp.vel;
				densities[i] = gp.rho;
				temperatures[i] = gp.temp;
				metals[i] = gp.metals;
				softenings[i] = gp.hsmooth;
				potentials[i] = gp.phi;
				index[i] = startParticle + i;
			}
			family.addAttribute("mass", masses);
			family.addAttribute("position", positions);
			family.addAttribute("velocity", velocities);
			family.addAttribute("density", densities);
			family.addAttribute("temperature", temperatures);
			family.addAttribute("metals", metals);
			family.addAttribute("softening", softenings);
			family.addAttribute("potential", potentials);
			family.addAttribute("index", index);
		} else if(familyName == "dark") {
			if(!r.seekParticleNum(startParticle + h.nsph)) {
			    cerr << "dark SEEK FAILED" << endl;
			    return false;
			    }
			float* masses = new float[numParticles];
			Vector3D<float>* positions = new Vector3D<float>[numParticles];
			Vector3D<float>* velocities = new Vector3D<float>[numParticles];
			float* softenings = new float[numParticles];
			float* potentials = new float[numParticles];
			int64_t* index = new int64_t[numParticles];
			dark_particle dp;
			for(int64_t i = 0; i < numParticles; ++i) {
				if(!r.getNextDarkParticle(dp)) {
					cerr << "BAD READ OF TIPSY FILE" <<endl;
					return false;
					}
				masses[i] = dp.mass;
				positions[i] = dp.pos;
				velocities[i] = dp.vel;
				softenings[i] = dp.eps;
				potentials[i] = dp.phi;
				index[i] = h.nsph + startParticle + i;
			}
			family.addAttribute("mass", masses);
			family.addAttribute("position", positions);
			family.addAttribute("velocity", velocities);
			family.addAttribute("softening", softenings);
			family.addAttribute("potential", potentials);
			family.addAttribute("index", index);
		} else if(familyName == "star") {
			if(!r.seekParticleNum(startParticle + h.nsph + h.ndark))
				return false;
			float* masses = new float[numParticles];
			Vector3D<float>* positions = new Vector3D<float>[numParticles];
			Vector3D<float>* velocities = new Vector3D<float>[numParticles];
			float* metals = new float[numParticles];
			float* formations = new float[numParticles];
			float* softenings = new float[numParticles];
			float* potentials = new float[numParticles];
			int64_t* index = new int64_t[numParticles];
			star_particle sp;
			for(int64_t i = 0; i < numParticles; ++i) {
				if(!r.getNextStarParticle(sp))
					return false;
				masses[i] = sp.mass;
				positions[i] = sp.pos;
				velocities[i] = sp.vel;
				metals[i] = sp.metals;
				formations[i] = sp.tform;
				softenings[i] = sp.eps;
				potentials[i] = sp.phi;
				index[i] = h.nsph + h.ndark + startParticle + i;
			}
			family.addAttribute("mass", masses);
			family.addAttribute("position", positions);
			family.addAttribute("velocity", velocities);
			family.addAttribute("metals", metals);
			family.addAttribute("formationtime", formations);
			family.addAttribute("softening", softenings);
			family.addAttribute("potential", potentials);
			family.addAttribute("index", index);
		}
	    } else if(family.count.numParticles != (u_int64_t) numParticles
		      || family.count.startParticle != startParticle)
		return false;
	AttributeMap::iterator attrIter = family.attributes.find(attributeName);
	if(attrIter == family.attributes.end())
		return false;
	//all the attributes for this bunch of particles have been loaded
	return true;
}

} //close namespace SimulationHandling
