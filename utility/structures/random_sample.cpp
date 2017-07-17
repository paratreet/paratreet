//random_sample.cpp

// This program takes a random subsample of a tipsy file

#include <iostream>
#include <algorithm>
#include <numeric>

#include "TipsyReader.h"

using namespace std;
using namespace Tipsy;

int main(int argc, char** argv) {
	if(argc < 3) {
		cerr << "Usage: random_sample num_samp tipsyfile" << endl;
		return 1;
	}

	int N = atoi(argv[1]);
	if(N <= 0) {
		cerr << "Size of sample must be positive!" << endl;
		return 2;
	}
	
	TipsyReader r(argv[2]);
	if(!r.status()) {
		cerr << "Couldn't load the tipsy file properly!" << endl;
		return 3;
	}
	
	header oldHeader = r.getHeader();
	
	if(N > oldHeader.nbodies) {
		cerr << "The sample size must be less than the total number of particles!" << endl;
		return 4;
	}
	
	vector<unsigned int> indices(oldHeader.nbodies, 1);
	indices[0] = 0;
	partial_sum(indices.begin(), indices.end(), indices.begin());
	random_shuffle(indices.begin(), indices.end());
	
	indices.resize(N);
	sort(indices.begin(), indices.end());
	
	vector<unsigned int>::iterator gasIter, darkIter;
	gasIter = lower_bound(indices.begin(), indices.end(), oldHeader.nsph);
	darkIter = lower_bound(gasIter, indices.end(), oldHeader.nsph + oldHeader.ndark);
	header h = oldHeader;
	h.nbodies = N;
	h.nsph = gasIter - indices.begin();
	h.ndark = darkIter - gasIter;
	h.nstar = h.nbodies - h.nsph - h.ndark;
	
	cout.write(reinterpret_cast<const char *>(&h), sizeof(h));
	
	gas_particle gp;
	dark_particle dp;
	star_particle sp;
	for(vector<unsigned int>::iterator iter = indices.begin(); iter != indices.end(); ++iter) {
		if(!r.seekParticleNum(*iter)) {
			cerr << "Problem seeking through tipsy file!" << endl;
			return 5;
		}
		if(*iter < oldHeader.nsph) {
			r.getNextGasParticle(gp);
			cout.write(reinterpret_cast<const char *>(&gp), gas_particle::sizeBytes);
		} else if(*iter < oldHeader.nsph + oldHeader.ndark) {
			r.getNextDarkParticle(dp);
			cout.write(reinterpret_cast<const char *>(&dp), dark_particle::sizeBytes);
		} else {
			r.getNextStarParticle(sp);
			cout.write(reinterpret_cast<const char *>(&sp), star_particle::sizeBytes);
		}
	}
	
	cerr << "Done." << endl;
}
