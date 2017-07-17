//see_header.cpp

// This program prints out the header information of a tipsy file.

#include <iostream>

#include "TipsyReader.h"

using namespace std;
using namespace Tipsy;

int main(int argc, char** argv) {
	if(argc < 2) {
		cout << "Usage: " << argv[0] << " tipsyfile" << endl;
		return 1;
	}

	TipsyReader r(argv[1]);
	if(!r.status()) {
		cerr << "Couldn't read the tipsy file.  Maybe it's not a tipsy file?" << endl;
		return 2;
	}
	
	cout << "Header information for \"" << argv[1] << "\"" << endl;
	if(r.isNative())
		cout << "Little-endian (Intel) byte-order" << endl;
	else
		cout << "Big-endian (SGI) byte-order" << endl;
	cout << r.getHeader() << endl;
}
