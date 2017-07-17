/** @file browse_field.cpp
 Given a field file, browse the values by index.
 */

#include <iostream>
#include <fstream>
#include <vector>

#include "Vector3D.h"
#include "tree_xdr.h"

using namespace std;
using namespace TypeHandling;

template <typename T>
void printValueLoop_typed(TypedArray& arr) {
	cout << "Minimum: " << arr.getMinValue(Type2Type<T>()) << endl;
	cout << "Maximum: " << arr.getMaxValue(Type2Type<T>()) << endl;

	T* array = arr.getArray(Type2Type<T>());
	
	u_int64_t i;
	cout << "\nWhich index do you want to see? ";
	while((cin >> i) && (i >= 0)) {
		if(i >= arr.length)
			cout << "Index too high, must be between 0 and " << (arr.length - 1);
		else
			cout << "Value: " << array[i];
		cout << "\nWhich index do you want to see? ";
		cout.flush();
	}
}

template <typename T>
void printValueLoop_dimensions(TypedArray& arr) {
	if(arr.dimensions == 1)
		printValueLoop_typed<T>(arr);
	else if(arr.dimensions == 3)
		printValueLoop_typed<Vector3D<T> >(arr);
	//else
		//throw UnsupportedDimensionalityException;
}

int main(int argc, char** argv) {
	if(argc < 2) {
		cerr << "Usage: browse_field field_file" << endl;
		return 1;
	}
	
	FILE* infile = fopen(argv[1], "rb");
	if(!infile) {
		cerr << "Couldn't open file \"" << argv[1] << "\"" << endl;
		return 2;
	}
	
	XDR xdrs;
	xdrstdio_create(&xdrs, infile, XDR_DECODE);
	FieldHeader fh;
	if(!xdr_template(&xdrs, &fh)) {
		cerr << "Couldn't read header from file!" << endl;
		return 3;
	}
	if(fh.magic != FieldHeader::MagicNumber) {
		cerr << "This file does not appear to be a field file (magic number doesn't match)." << endl;
		return 4;
	}
	
	TypedArray arr;
	arr.dimensions = fh.dimensions;
	arr.code = fh.code;
	try {
	    if(!readAttributes(&xdrs, arr, fh.numParticles)) {
		cerr << "Had problems reading in the field" << endl;
		return 5;
		}
	    }
	catch(XDRReadError &e) {
	    cerr << "Problems in readAttributes: " << e << endl;
	    return 5;
	    }
	
	xdr_destroy(&xdrs);
	fclose(infile);
	
	cout << "File " << argv[1] << " contains an attribute.\nHeader:\n" << fh << endl;
	
	switch(arr.code) {
		case int8:
			printValueLoop_dimensions<char>(arr);
			break;
		case uint8:
			printValueLoop_dimensions<unsigned char>(arr);
			break;
		case int16:
			printValueLoop_dimensions<short>(arr);
			break;
		case uint16:
			printValueLoop_dimensions<unsigned short>(arr);
			break;
		case int32:
			printValueLoop_dimensions<int>(arr);
			break;
		case uint32:
			printValueLoop_dimensions<unsigned int>(arr);
			break;
		case int64:
			printValueLoop_dimensions<int64_t>(arr);
			break;
		case uint64:
			printValueLoop_dimensions<u_int64_t>(arr);
			break;
		case float32:
			printValueLoop_dimensions<float>(arr);
			break;
		case float64:
			printValueLoop_dimensions<double>(arr);
			break;
		default:
			cout << "Unknown data type!";
	}
	
	arr.release();
	
	cerr << "Done." << endl;
}
