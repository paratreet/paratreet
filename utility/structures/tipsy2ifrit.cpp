/** @file tipsy2ifrit.cpp
 @author Graeme Lufkin (gwl@umd.edu)
 @date Created 16 November 2005
 @version 1.0
 */
 
//This program converts a tipsy file to the IFRIT binary format
//IFRIT allows only three attributes per particle,
//so I have arbitrarily chosen the particle type and two type-specific attributes
//If this isn't useful, email me and I'll write a more flexible program

#include <iostream>
#include <fstream>

#include "TipsyFile.h"
#include "OrientedBox.h"

using namespace std;
using namespace Tipsy;

template <typename T>
OrientedBox<T> cubize(OrientedBox<T> const& b) {
	OrientedBox<T> result(b);
	Vector3D<T> size = b.size();
	T longest_side = max(max(size.x, size.y), size.z);
	result.grow(b.center() + longest_side * Vector3D<T>(0.5, 0.5, 0.5));
	result.grow(b.center() - longest_side * Vector3D<T>(0.5, 0.5, 0.5));
	return result;
}

int main(int argc, char** argv) {
	if(argc < 2) {
		cout << "Usage: " << argv[0] << " tipsyfile [ifrit filename]" << endl;
		return 1;
	}

	TipsyFile tf(argv[1]);
	if(!tf.loadedSuccessfully()) {
		cerr << "Couldn't load the file properly!" << endl;
		return 1;
	}
	
	string filename = tf.filename + ".bin";
	if(argc >= 3)
		filename = argv[2];
	
	ofstream os(filename.c_str());
	if(!os) {
		cerr << "Couldn't create ifrit file" << endl;
		return 2;
	}

	//write number of particles
	int ntemp = 4;
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	os.write(reinterpret_cast<const char*>(&tf.h.nbodies), 4);	
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	//write bounding box (as 4-byte float!)
	ntemp = 24;
	OrientedBox<float> box = cubize(tf.stats.bounding_box);
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	os.write(reinterpret_cast<const char*>(&box.lesser_corner.x), 4);	
	os.write(reinterpret_cast<const char*>(&box.lesser_corner.y), 4);	
	os.write(reinterpret_cast<const char*>(&box.lesser_corner.z), 4);	
	os.write(reinterpret_cast<const char*>(&box.greater_corner.x), 4);	
	os.write(reinterpret_cast<const char*>(&box.greater_corner.y), 4);	
	os.write(reinterpret_cast<const char*>(&box.greater_corner.z), 4);	
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	ntemp = 4 * tf.h.nbodies;
	//write x positions
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.nsph; ++i)
		os.write(reinterpret_cast<const char*>(&tf.gas[i].pos.x), 4);
	for(int i = 0; i < tf.h.ndark; ++i)
		os.write(reinterpret_cast<const char*>(&tf.darks[i].pos.x), 4);
	for(int i = 0; i < tf.h.nstar; ++i)
		os.write(reinterpret_cast<const char*>(&tf.stars[i].pos.x), 4);
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	//write y positions
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.nsph; ++i)
		os.write(reinterpret_cast<const char*>(&tf.gas[i].pos.y), 4);
	for(int i = 0; i < tf.h.ndark; ++i)
		os.write(reinterpret_cast<const char*>(&tf.darks[i].pos.y), 4);
	for(int i = 0; i < tf.h.nstar; ++i)
		os.write(reinterpret_cast<const char*>(&tf.stars[i].pos.y), 4);
	os.write(reinterpret_cast<const char*>(&ntemp), 4);

	//write z positions
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.nsph; ++i)
		os.write(reinterpret_cast<const char*>(&tf.gas[i].pos.z), 4);
	for(int i = 0; i < tf.h.ndark; ++i)
		os.write(reinterpret_cast<const char*>(&tf.darks[i].pos.z), 4);
	for(int i = 0; i < tf.h.nstar; ++i)
		os.write(reinterpret_cast<const char*>(&tf.stars[i].pos.z), 4);
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	//write particles types as first attribute
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	float type = 0;
	for(int i = 0; i < tf.h.nsph; ++i)
		os.write(reinterpret_cast<const char*>(&type), 4);
	++type;
	for(int i = 0; i < tf.h.ndark; ++i)
		os.write(reinterpret_cast<const char*>(&type), 4);
	++type;
	for(int i = 0; i < tf.h.nstar; ++i)
		os.write(reinterpret_cast<const char*>(&type), 4);
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	//write gravitational potential as second attribute
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.nsph; ++i)
		os.write(reinterpret_cast<const char*>(&tf.gas[i].phi), 4);
	for(int i = 0; i < tf.h.ndark; ++i)
		os.write(reinterpret_cast<const char*>(&tf.darks[i].phi), 4);
	for(int i = 0; i < tf.h.nstar; ++i)
		os.write(reinterpret_cast<const char*>(&tf.stars[i].phi), 4);
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	//write density, eps, tform as third attribute
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.nsph; ++i)
		os.write(reinterpret_cast<const char*>(&tf.gas[i].rho), 4);
	for(int i = 0; i < tf.h.ndark; ++i)
		os.write(reinterpret_cast<const char*>(&tf.darks[i].eps), 4);
	for(int i = 0; i < tf.h.nstar; ++i)
		os.write(reinterpret_cast<const char*>(&tf.stars[i].tform), 4);
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	os.close();

	cout << "Done." << endl;
}
