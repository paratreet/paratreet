//see_treexdr.cpp

// This program prints out the header information of a tree file.

#include <iostream>

#include "tree_xdr.h"

using namespace std;

int main(int argc, char** argv) {
	if(argc < 2) {
		cout << "Usage: " << argv[0] << " tree_file" << endl;
		return 1;
	}

	FILE *infile = fopen(argv[1], "rb");
	FieldHeader fh;

	if(infile) {
		XDR xdrs;
		xdrstdio_create(&xdrs, infile, XDR_DECODE);
		xdr_template(&xdrs, &fh);

		xdr_destroy(&xdrs);
	}
	fclose(infile);
	
	cout << "Header information for \"" << argv[1] << "\"" << endl;
	
	cout << fh << endl;

}
