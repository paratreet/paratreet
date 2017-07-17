/** @file histogram_field.cpp
 Given a field file, make a histogram of the values.
 */

#include <iostream>
#include <vector>
#include <cmath>

#include "tree_xdr.h"

using namespace std;
using namespace TypeHandling;

template <typename T>
void makeHistogram(const unsigned int numElements, XDR* xdrs, const int numBins, bool logarithmic) {
	vector<unsigned int> counts(numBins, 0);
	int bin;
	T min, max, element, binWidth;
	
	xdr_template(xdrs, &min);
	xdr_template(xdrs, &max);
	
	if(logarithmic) {
		if(min <= 0) {
			cerr << "Cannot have negative values with logarithmic bins" << endl;
			return;
		}
		min = static_cast<T>(log10(static_cast<double>(min)));
		max = static_cast<T>(log10(static_cast<double>(max)));
	}
	
	binWidth = (max - min) / numBins;
	
	for(unsigned int i = 0; i < numElements; ++i) {
		xdr_template(xdrs, &element);
		if(logarithmic)
			bin = static_cast<unsigned int>(floor(static_cast<double>(numBins * (log10(static_cast<double>(element)) - min) / (max - min))));
		else
			bin = static_cast<unsigned int>(floor(static_cast<double>(numBins * (element - min) / (max - min))));
		if(bin < 0)
			bin = 0;
		if(bin > numBins - 1)
			bin = numBins - 1;
		counts[bin]++;
	}
	
	element = min;
	for(int i = 0; i < numBins; ++i) {
		if(logarithmic)
			cout << pow(10.0, static_cast<double>(element)) << "\t" << counts[i] << "\n";
		else
			cout << element << "\t" << counts[i] << "\n";
		element += binWidth;
		if(logarithmic)
			cout << pow(10.0, static_cast<double>(element)) << "\t" << counts[i] << "\n";
		else
			cout << element << "\t" << counts[i] << "\n";
	}
}

void makeHistogram(const FieldHeader& fh, XDR* xdrs, const int numBins, bool logarithmic) {
	switch(fh.code) {
		case int8:
			return makeHistogram<char>(fh.numParticles, xdrs, numBins, logarithmic);
		case uint8:
			return makeHistogram<unsigned char>(fh.numParticles, xdrs, numBins, logarithmic);
		case int16:
			return makeHistogram<short>(fh.numParticles, xdrs, numBins, logarithmic);
		case uint16:
			return makeHistogram<unsigned short>(fh.numParticles, xdrs, numBins, logarithmic);
		case int32:
			return makeHistogram<int>(fh.numParticles, xdrs, numBins, logarithmic);
		case uint32:
			return makeHistogram<unsigned int>(fh.numParticles, xdrs, numBins, logarithmic);
		case int64:
			return makeHistogram<int64_t>(fh.numParticles, xdrs, numBins, logarithmic);
		case uint64:
			return makeHistogram<u_int64_t>(fh.numParticles, xdrs, numBins, logarithmic);
		case float32:
			return makeHistogram<float>(fh.numParticles, xdrs, numBins, logarithmic);
		case float64:
			return makeHistogram<double>(fh.numParticles, xdrs, numBins, logarithmic);
	}
}

int main(int argc, char** argv) {
	if(argc < 3) {
		cerr << "Usage: histogram_field numBins [-l] field_file" << endl;
		return 1;
	}
	
	int numBins = atoi(argv[1]);
	if(numBins <= 1) {
		cerr << "Number of bins must be greater than one!" << endl;
		return 2;
	}
	
	int fileArgument = 2;
	bool logarithmic = false;
	
	if(argc > 3) {
		fileArgument = 3;
		string s(argv[2]);
		if(s == "-l")
			logarithmic = true;
		else {
			cerr << "Invalid option!" << endl;
			return 2;
		}
	}
	
	FILE* infile = fopen(argv[fileArgument], "rb");
	if(!infile) {
		cerr << "Couldn't open file \"" << argv[1] << "\"" << endl;
		return 3;
	}
	
	XDR xdrs;
	xdrstdio_create(&xdrs, infile, XDR_DECODE);
	FieldHeader fh;
	if(!xdr_template(&xdrs, &fh)) {
		cerr << "Couldn't read header from file!" << endl;
		return 4;
	}
	if(fh.magic != FieldHeader::MagicNumber || fh.dimensions != 1) {
		cerr << "Field file is corrupt or not one-dimensional" << endl;
		return 5;
	}
	
	//read in min/max
	makeHistogram(fh, &xdrs, numBins, logarithmic);
	
	xdr_destroy(&xdrs);
	fclose(infile);	
	
	cerr << "Done." << endl;
}
