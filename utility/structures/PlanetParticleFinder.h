//PlanetParticleFinder.h
// Routines to iteratively find gas particles around a dark matter planet particle

#ifndef PLANETPARTICLEFINDER_H__89uj0436890432y82y9408
#define PLANETPARTICLEFINDER_H__89uj0436890432y82y9408

#include <vector>
#include <iostream>

#include "Vector3D.h"
#include "TipsyFile.h"
#include "Sphere.h"
#include "Space.h"

const double G = 1;
const double M_sun = 1;

//Count and mark particles in a sphere, updating the center of mass and Hill radius
int countParticles(Tipsy::TipsyFile& tf, double& totalMass, Vector3D<double>& cm, std::vector<int>& markedParticles) {
	int count = 0;
	double hillRadius = cm.length() * pow(totalMass / 3.0 / M_sun, 1.0 / 3.0);
	Sphere<double> s(cm, hillRadius);
	double hillDensity = (9 * M_sun / 4 / M_PI / cm.lengthSquared() / cm.length());
	
	totalMass = tf.darks[0].mass;
	cm = tf.darks[0].mass * tf.darks[0].pos;
	markedParticles.clear();
	for(int i = 0; i < tf.h.nsph; ++i) {
		//to be included with the planet, must be inside the Hill sphere and have sufficient density
		if(Space::contains(s, tf.gas[i].pos) && tf.gas[i].rho > hillDensity) {
			++count;
			markedParticles.push_back(i);
			totalMass += tf.gas[i].mass;
			cm += tf.gas[i].mass * tf.gas[i].pos;
		}
	}
	cm /= totalMass;
	//cerr << "Total mass: " << totalMass << " Center of mass: " << cm << " Number of particles: " << markedParticles.size() << endl;
	return count;
}

void findPlanetParticles(Tipsy::TipsyFile& tf, double& totalMass, Vector3D<double>& cm, std::vector<int>& markedParticles) {
	totalMass = tf.darks[0].mass;
	cm = tf.darks[0].pos;
	
	int oldNumPlanetParticles = -1, numPlanetParticles = 0;
	int i = 0;
	while(oldNumPlanetParticles != numPlanetParticles) {
		if(++i > 15) {
			std::cerr << "Planet particle finder did not converge! old: " << oldNumPlanetParticles << " new: " << numPlanetParticles << std::endl;
			break;
		}
		//cerr << "Iteration " << (i++) << " N: " << numPlanetParticles << " Total mass: " << totalMass << " CM: " << cm << " Hill Radius: " << (cm.length() * pow(totalMass / 3.0, 1.0 / 3.0)) << endl;
		oldNumPlanetParticles = numPlanetParticles;
		numPlanetParticles = countParticles(tf, totalMass, cm, markedParticles);
	}
}

#endif //PLANETPARTICLEFINDER_H__89uj0436890432y82y9408
