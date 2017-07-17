/** @file convert_ascii_field.cpp
 Given a tree file, a tipsy-order (uid) file, and an ascii-format
 tipsy array or vector file, convert to the tree-based field format.
 */

#include <iostream>
#include <fstream>
#include <cstdlib>

#include "tree_xdr.h"

using namespace std;

int main(int argc, char** argv) {
	
	if(argc < 3) {
		cerr << "Usage: convert_ascii_field arrayfile tispy-order-file outputfile" << endl;
		return 1;
	}
	
	int numParticles;
	ifstream infile(argv[1]);
	if(!infile) {
		cerr << "Couldn't open array file" << endl;
		return 2;
	}
	
	//read number of particles from array file
	infile >> numParticles;
	if(!infile) {
		cerr << "Couldn't read number of particles from array file" << endl;
		return 2;
	}
	
	//read in array values
	float* values = new float[numParticles];
	for(int i = 0; i < numParticles; ++i) {
		infile >> values[i];
		if(!infile) {
			cerr << "Problem reading value " << i << " from the array file" << endl;
			return 2;
		}
	}
	
	//get tipsy-order header
	FILE* uidfile = fopen(argv[2], "rb");
	if(!uidfile) {
		cerr <<"Couldn't open tispy-order file \"" << argv[2] << "\"" << endl;
		return 2;
	}
	
	XDR xdrs;
	xdrstdio_create(&xdrs, uidfile, XDR_DECODE);
	FieldHeader fh;
	if(!xdr_template(&xdrs, &fh)) {
		cerr << "Couldn't read tispy-order header" << endl;
		return 2;
	}
	if(fh.magic != FieldHeader::MagicNumber) {
		cerr << "tispy-order file appears corrupt (magic number isn't right)" << endl;
		return 2;
	}
	if(fh.dimensions != 1 || fh.code != uint32) {
		cerr << "tispy-order file contains wrong type" << endl;
		return 2;
	}
	
	unsigned int minOrder, maxOrder;
	if(!xdr_template(&xdrs, &minOrder) || !xdr_template(&xdrs, &maxOrder)) {
		cerr << "Had problems reading min/max values" << endl;
		return 2;
	}
	
	float minValue = HUGE_VAL, maxValue = -HUGE_VAL;
	
	fh.dimensions = 1;
	fh.code = float32;
	
	FILE* outfile = fopen(argv[3], "wb");
	if(!outfile) {
		cerr << "Couldn't open output file \"" << argv[3] << "\"" << endl;
		return 2;
	}
	
	//write header for output file
	XDR outxdrs;
	xdrstdio_create(&outxdrs, outfile, XDR_ENCODE);
	if(!xdr_template(&outxdrs, &fh)) {
		cerr << "Couldn't write output header" << endl;
		return 2;
	}
	if(!xdr_template(&outxdrs, &minValue) || !xdr_template(&outxdrs, &maxValue)) {
		cerr << "Had problems writing min/max values" << endl;
		return 2;
	}
	
	unsigned int order;
	for(unsigned int i = 0; i < fh.numParticles; ++i) {
		//read particle i's original tipsy file order
		if(!xdr_template(&xdrs, &order)) {
			cerr << "Problem reading tipsy-order file" << endl;
			return 2;
		}
		//make sure original order isn't too high
		if(order >= numParticles) {
			cerr << "tipsy order for particle " << i << " is out of array file's range (" << numParticles << " values)" << endl;
			return 2;
		}
		//write the order'th value to the i'th output
		if(!xdr_template(&outxdrs, values + order)) {
			cerr << "Problem writing value to output file" << endl;
			return 2;
		}
		if(values[order] < minValue)
			minValue = values[order];
		if(values[order] > maxValue)
			maxValue = values[order];
	}
	
	xdr_destroy(&xdrs);
	fclose(uidfile);

	//rewind and write min/max to output file
	if(!xdr_setpos(&outxdrs, FieldHeader::sizeBytes) || !xdr_template(&outxdrs, &minValue) || !xdr_template(&outxdrs, &maxValue)) {
		cerr << "Problem rewinding and writing min/max to output" << endl;
		return 2;
	}
	xdr_destroy(&outxdrs);
	fclose(outfile);
	
	delete[] values;
	
	cerr << "Done." << endl;
}
