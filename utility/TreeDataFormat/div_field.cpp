// Subtract two field files.
// NB the output is on stdout which means that the min/max can't be
// set properly.

#include <iostream>
#include <limits>
#include "tree_xdr.h"
#include "TypeHandling.h"

/*
 * fancy double macro to do the switch amoung types.  It gives us a
 * constant to use for the template instantiation.
 * To use, first define a caseCode2Type macro whose first argument is
 * a variable of type TypeHandling::DataTypeCode, the second is the
 * actual type it maps to.
 */

#define casesCode2Types \
    caseCode2Type(int8, char)	\
    caseCode2Type(uint8, unsigned char)	\
    caseCode2Type(int16, short)	\
    caseCode2Type(uint16, unsigned short)	\
    caseCode2Type(int32, int)	\
    caseCode2Type(uint32, unsigned int)	\
    caseCode2Type(int64, int64_t)	\
    caseCode2Type(uint64, u_int64_t)	\
    caseCode2Type(float32, float)	\
    caseCode2Type(float64, double)	\
    default: assert(0);

template <typename T>
void subFields(XDR* xdrs1, XDR* xdrs2, XDR* xdrso, FieldHeader fh) 
{
    int64_t i;
    // include min and max
    int nHeader = fh.dimensions;
    int64_t nValues = fh.numParticles*fh.dimensions;
    T sValue1;
    T sValue2;
    T sValueo;
    for(i = 0; i < nHeader; i++) {
        assert(xdr_template(xdrs1, &sValue1) == 1);
        assert(xdr_template(xdrs2, &sValue2) == 1);
        sValueo = std::numeric_limits<T>::min();
        assert(xdr_template(xdrso, &sValueo) == 1);
        }
    for(i = 0; i < nHeader; i++) {
        assert(xdr_template(xdrs1, &sValue1) == 1);
        assert(xdr_template(xdrs2, &sValue2) == 1);
        sValueo = std::numeric_limits<T>::max();
        assert(xdr_template(xdrso, &sValueo) == 1);
        }
    for(i = 0; i < nValues; i++) {
        assert(xdr_template(xdrs1, &sValue1) == 1);
        assert(xdr_template(xdrs2, &sValue2) == 1);
        sValueo = sValue1 / sValue2;
        assert(xdr_template(xdrso, &sValueo) == 1);
        }
    }

using namespace TypeHandling;

int main(int argc, char** argv) {
    if(argc < 3) {
        std::cerr << "Usage: div_field field_file1 field_file2" << std::endl;
        return 1;
        }
    FILE* infile1 = fopen(argv[1], "rb");
    if(!infile1) {
        std::cerr << "Couldn't open file \"" << argv[1] << "\"" << std::endl;
        return 3;
    }
    FILE* infile2 = fopen(argv[2], "rb");
    if(!infile2) {
        std::cerr << "Couldn't open file \"" << argv[2] << "\"" << std::endl;
        return 3;
    }
    XDR xdrs1;
    xdrstdio_create(&xdrs1, infile1, XDR_DECODE);
    FieldHeader fh1;
    if(!xdr_template(&xdrs1, &fh1)) {
        std::cerr << "Couldn't read header from file!" << std::endl;
        return 4;
	}
    if(fh1.magic != FieldHeader::MagicNumber) {
        std::cerr << "Field file is corrupt" << std::endl;
        return 5;
	}
    XDR xdrs2;
    xdrstdio_create(&xdrs2, infile2, XDR_DECODE);
    FieldHeader fh2;
    if(!xdr_template(&xdrs2, &fh2)) {
        std::cerr << "Couldn't read header from file!" << std::endl;
        return 4;
	}
    if(fh2.magic != FieldHeader::MagicNumber) {
        std::cerr << "Field file is corrupt" << std::endl;
        return 5;
	}
    if(fh1.dimensions != fh2.dimensions) {
        std::cerr << "Field dimensions do not match" << std::endl;
        return 5;
	}
    if(fh1.code != fh2.code) {
        std::cerr << "Field types do not match" << std::endl;
        return 5;
	}

    XDR xdrsOut;
    xdrstdio_create(&xdrsOut, stdout, XDR_ENCODE);
    FieldHeader fho = fh1;
    if(!xdr_template(&xdrsOut, &fho)) {
        std::cerr << "Couldn't write output header" << std::endl;
        return 2;
	}
    switch(fho.code) {
#undef caseCode2Type
#define caseCode2Type(enumName,typeName) \
	    case enumName: \
		subFields<typeName>(&xdrs1, &xdrs2, &xdrsOut, fho);  \
	    break;
	    
	    casesCode2Types
		}

    xdr_destroy(&xdrs1);
    fclose(infile1);	
    xdr_destroy(&xdrs2);
    fclose(infile2);	
    xdr_destroy(&xdrsOut);
}
