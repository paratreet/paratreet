//simload.cpp

#include <iostream>
#include <string>
#include <list>
#include <vector>

#include "Simulation.h"
#include "SiXFormat.h"
#include "TipsyFormat.h"

using namespace SimulationHandling;
using namespace std;

extern "C" void CkRegisterMainModule(void) {

}

int main(int argc, char** argv) {

	if(argc < 2) {
		cerr << "Usage: simload simulation_directory" << endl;
		return 1;
	}
	
	Simulation* sim = new SiXFormatReader(argv[1]);
	if(sim->size() == 0) {
		cout << "Possible problem opening simulation file, trying plain tipsy format" << endl;
		sim->release();
		delete sim;
		sim = new TipsyFormatReader(argv[1]);
		if(sim->size() == 0) {
			cerr << "Couldn't open simulation file (tried new and plain tipsy formats)" << endl;
			return 2;
		}
	}
	cout << "Simulation " << sim->name << " contains " << sim->size() << " families" << endl;
	
	for(Simulation::iterator iter = sim->begin(); iter != sim->end(); ++iter) {
		cout << "Family " << iter->first << " has the following attributes:" << endl;
		ParticleFamily& family = iter->second;
		for(AttributeMap::iterator attrIter = family.attributes.begin(); attrIter != family.attributes.end(); ++attrIter) {
			cout << attrIter->first << " scalar min: " << getScalarMin(attrIter->second) << " max: " << getScalarMax(attrIter->second) << endl;
			cout << attrIter->second << endl;
		}
	}
	
	sim->loadAttribute("dark", "position", -1);
	sim->loadAttribute("dark", "mass", -1);
	
	ParticleFamily& darks = sim->getFamily("dark");
	Vector3D<float>* positions = darks.getAttribute("position", Type2Type<Vector3D<float> >());
	float* masses = darks.getAttribute<float>("mass");
	if(!masses)
		cerr << "Didn't get masses!" << endl;
	if(!positions)
		cerr << "Didn't get positions!" << endl;
	if(masses && positions) {
		string groupName = "My First Group";
		OrientedBox<float> boundingBox;
		boundingBox.lesser_corner = Vector3D<float>(-0.1, -0.1, -0.1);
		boundingBox.greater_corner = Vector3D<float>(0.2, 0.2, 0.1);
		darks.createGroup(groupName, "position", boundingBox.lesser_corner, boundingBox.greater_corner);
		cout << "Created group around " << boundingBox << endl;
		Vector3D<double> cm;
		double totalMass = 0;
		u_int64_t N = darks.count.totalNumParticles;
		cout << "Finding cm for " << N << " dark particles" << endl;
		ParticleGroup& activeGroup = darks.groups[groupName];
		if(activeGroup.size()) {
		cout << "There are " << activeGroup.size() << " particles in the group" << endl;
			for(ParticleGroup::iterator iter = activeGroup.begin(); iter != activeGroup.end(); ++iter) {
				cm += masses[*iter] * positions[*iter];
				totalMass += masses[*iter];
			}
			cm /= totalMass;
			cout << "Group dark mass: " << totalMass << endl;
			cout << "Dark group center of mass: " << cm << endl;
		} else 
			cerr << "Group didn't have any particles!" << endl;
	} else
		cerr << "Couldn't load attributes successfully!" << endl;
	
	cout << "After loading attributes ..." << endl;
	for(Simulation::iterator iter = sim->begin(); iter != sim->end(); ++iter) {
		cout << "Family " << iter->first << " has the following attributes:" << endl;
		ParticleFamily& family = iter->second;
		for(AttributeMap::iterator attrIter = family.attributes.begin(); attrIter != family.attributes.end(); ++attrIter) {
			cout << attrIter->first << " scalar min: " << getScalarMin(attrIter->second) << " max: " << getScalarMax(attrIter->second) << endl;
			cout << attrIter->second << endl;
		}
	}
	sim->release();
	
	delete sim;
	
	cerr << "Done." << endl;
}
