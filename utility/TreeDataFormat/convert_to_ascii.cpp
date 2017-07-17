//convert_to_ascii.cpp

#include <iostream>
#include <cstdio>

#include "tree_xdr.h"

using namespace std;
using namespace TypeHandling;

int main(int argc, char** argv) {
	if(argc < 2) {
		cerr << "Usage: convert_to_ascii filename [uidfile]" << endl;
		return 1;
	}
	
	FILE* infile;
	XDR xdrs;
	FieldHeader fh, uid_fh;
	unsigned int* uids = 0;
	
	if(argc > 2) {
		infile = fopen(argv[2], "rb");
		if(!infile) {
			cerr << "Couldn't open uid file \"" << argv[2] << "\"" << endl;
			return 2;
		}
		xdrstdio_create(&xdrs, infile, XDR_DECODE);
		if(!xdr_template(&xdrs, &uid_fh)) {
			cerr << "Couldn't read header from uid file!" << endl;
			return 3;
		}
		if(uid_fh.magic != FieldHeader::MagicNumber) {
			cerr << "The uid file appears corrupt (magic number doesn't match)." << endl;
			return 4;
		}
		if(uid_fh.dimensions != 1 || uid_fh.code != uint32) {
			cerr << "UID file doesn't contain the right type of data!" << endl;
			return 5;
		}
		uids = readField<unsigned int>(&xdrs, uid_fh.numParticles);
		if(uids == 0) {
			cerr << "Had problems reading in the uid file" << endl;
			return 6;
		}
		xdr_destroy(&xdrs);
		fclose(infile);
	}
	
	infile = fopen(argv[1], "rb");
	if(!infile) {
		cerr << "Couldn't open field file \"" << argv[1] << "\"" << endl;
		return 2;
	}
	xdrstdio_create(&xdrs, infile, XDR_DECODE);
	if(!xdr_template(&xdrs, &fh)) {
		cerr << "Couldn't read header from file!" << endl;
		return 3;
	}
	if(fh.magic != FieldHeader::MagicNumber) {
		cerr << "This file does not appear to be a field file (magic number doesn't match)." << endl;
		return 4;
	}
	if(argc > 2 && (uid_fh.numParticles != fh.numParticles)) {
		cerr << "UID file and field file don't match!" << endl;
		return 5;
	}
	
	void* data;
	if(argc > 2)
		data = readField_reorder(fh, &xdrs, uids);
	else
		data = readField(fh, &xdrs);
	
	if(data == 0) {
		cerr << "Had problems reading in the field" << endl;
		return 6;
	}
	
	xdr_destroy(&xdrs);
	fclose(infile);
	
	cout.precision(17);
	cout << fh.numParticles << '\n';
	for(unsigned int j = 0; j < fh.dimensions; ++j) {
		switch(fh.code) {
			case int8:
				for(unsigned int i = 0; i < fh.numParticles; ++i)
					cout << static_cast<char *>(data)[fh.dimensions * i + j] << '\n';
				break;
			case uint8:
				for(unsigned int i = 0; i < fh.numParticles; ++i)
					cout << static_cast<unsigned char *>(data)[fh.dimensions * i + j] << '\n';
				break;
			case int16:
				for(unsigned int i = 0; i < fh.numParticles; ++i)
					cout << static_cast<short *>(data)[fh.dimensions * i + j] << '\n';
				break;
			case uint16:
				for(unsigned int i = 0; i < fh.numParticles; ++i)
					cout << static_cast<unsigned short *>(data)[fh.dimensions * i + j] << '\n';
				break;
			case int32:
				for(unsigned int i = 0; i < fh.numParticles; ++i)
					cout << static_cast<int *>(data)[fh.dimensions * i + j] << '\n';
				break;
			case uint32:
				for(unsigned int i = 0; i < fh.numParticles; ++i)
					cout << static_cast<unsigned int *>(data)[fh.dimensions * i + j] << '\n';
				break;
			case int64:
				for(unsigned int i = 0; i < fh.numParticles; ++i)
					cout << static_cast<int64_t *>(data)[fh.dimensions * i + j] << '\n';
				break;
			case uint64:
				for(unsigned int i = 0; i < fh.numParticles; ++i)
					cout << static_cast<u_int64_t *>(data)[fh.dimensions * i + j] << '\n';
				break;
			case float32:
				for(unsigned int i = 0; i < fh.numParticles; ++i)
					cout << static_cast<float *>(data)[fh.dimensions * i + j] << '\n';
				break;
			case float64:
				for(unsigned int i = 0; i < fh.numParticles; ++i)
					cout << static_cast<double *>(data)[fh.dimensions * i + j] << '\n';
				break;
			default:
				cout << "I don't recognize the type of this field!";
				return 7;
		}
	}

	cerr << "Done." << endl;	
}
