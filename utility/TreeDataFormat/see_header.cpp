//see_header.cpp
// Look at the header information for a tree or field file

#include <iostream>
#include <cstdlib>

#include "tree_xdr.h"
#include "OrientedBox.h"

using namespace std;

int main(int argc, char** argv) {

	if(argc < 2) {
		cerr << "Usage: see_header (treefile | fieldfile)" << endl;
		return 1;
	}
	
	FILE* infile = fopen(argv[1], "rb");
	if(!infile) {
		cerr << "Couldn't open file \"" << argv[1] << "\"" << endl;
		return 2;
	}
	
	XDR xdrs;
	xdrstdio_create(&xdrs, infile, XDR_DECODE);
	int magic;
	if(!xdr_int(&xdrs, &magic)) {
		cerr << "Couldn't read magic number from file!" << endl;
		return 3;
	}
	
	if(!xdr_setpos(&xdrs, 0)) {
		cerr << "Couldn't rewind stream!" << endl;
		return 4;
	}
	
	TreeHeader th;
	FieldHeader fh;
	
	switch(magic) {
		case TreeHeader::MagicNumber:
			xdr_template(&xdrs, &th);
			cout << "File is a tree representation.\n" << th << endl;
			break;
		case FieldHeader::MagicNumber:
			xdr_template(&xdrs, &fh);
			cout << "File is a field representation.\n" << fh << endl;
			break;
		default:
			cerr << "File does not appear to be a tree or a field file (magic number didn't match)." << endl;
			return 5;
	}
	
	xdr_destroy(&xdrs);
	fclose(infile);
}
