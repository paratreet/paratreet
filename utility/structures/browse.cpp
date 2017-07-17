//browse.cpp

//This program lets you see the fields of individual particles in a tipsy file

#include <iostream>

#include "TipsyFile.h"

using namespace std;
using namespace Tipsy;

int main(int argc, char** argv) {
	if(argc < 2) {
		cout << "Usage: " << argv[0] << " tipsyfile" << endl;
		return 1;
	}

	TipsyFile tf(argv[1]);
	if(!tf.loadedSuccessfully()) {
		cerr << "Couldn't load the file properly!" << endl;
		return 1;
	}
	
	cout.setf(ios::scientific | ios::uppercase);
	cout << tf << endl;

	int i, type;

	cout << "Which particle: ";
	while((cin >> i) && (i >= 0)) {
		cout << "Which particle type: ";
		if(!(cin >> type))
			break;
		switch(type) {
			case 0: //gas
				if(i < tf.h.nsph)
					cout << tf.gas[i] << endl;
				break;
			case 1: //dark
				if(i < tf.h.ndark)
					cout << tf.darks[i] << endl;
				break;
			case 2: //star
				if(i < tf.h.nstar)
					cout << tf.stars[i] << endl;
				break;
			default: //unknown
				cerr << "Particle type must be 0, 1, or 2" << endl;
		}
		cout << "Which particle: ";
	}

	cout << "Done." << endl;
}
