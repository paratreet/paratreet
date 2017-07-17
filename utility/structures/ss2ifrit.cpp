/** @file ss2ifrit.cpp
 @author Graeme Lufkin (gwl@umd.edu)
 @date Created 18 November 2005
 @version 1.0
 */
 
//This program converts a SS file to the IFRIT binary format

#include <iostream>
#include <fstream>

#include "SS.h"
#include "OrientedBox.h"

using namespace std;
using namespace SS;

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
		cout << "Usage: " << argv[0] << " ssfile [ifrit filename]" << endl;
		return 1;
	}

	SSFile tf(argv[1]);
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
	os.write(reinterpret_cast<const char*>(&tf.h.n_data), 4);	
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	//write bounding box (as 4-byte floats!)
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
	
	ntemp = 4 * tf.h.n_data;
	float val = 0;
	//write x positions
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.n_data; ++i) {
		val = tf.particles[i].pos.x;
		os.write(reinterpret_cast<const char*>(&val), 4);
	}
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	//write y positions
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.n_data; ++i) {
		val = tf.particles[i].pos.y;
		os.write(reinterpret_cast<const char*>(&val), 4);
	}
	os.write(reinterpret_cast<const char*>(&ntemp), 4);

	//write z positions
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.n_data; ++i) {
		val = tf.particles[i].pos.z;
		os.write(reinterpret_cast<const char*>(&val), 4);
	}
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	//write particle mass as first attribute
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.n_data; ++i) {
		val = tf.particles[i].mass;
		os.write(reinterpret_cast<const char*>(&val), 4);
	}
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	//write particle radius as second attribute
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.n_data; ++i) {
		val = tf.particles[i].radius;
		os.write(reinterpret_cast<const char*>(&val), 4);
	}
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	//write magnitude of spin as third attribute
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	for(int i = 0; i < tf.h.n_data; ++i) {
		val = tf.particles[i].spin.length();
		os.write(reinterpret_cast<const char*>(&val), 4);
	}
	os.write(reinterpret_cast<const char*>(&ntemp), 4);
	
	os.close();

	cout << "Done." << endl;
}
