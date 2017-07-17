// Find min and maximum of a field.

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
void minmax_field(XDR* xdrs, FieldHeader fh) 
{
    int64_t i;
    // include min and max
    int nSkip = 2*fh.dimensions;
    int64_t nValues = fh.numParticles*fh.dimensions;
    T sValue;
    T sMin = std::numeric_limits<T>::max();
    T sMax = std::numeric_limits<T>::min();
    if(fh.code >= TypeHandling::float32)
        sMax = -std::numeric_limits<T>::max();
    for(i = 0; i < nSkip; i++)
        assert(xdr_template(xdrs, &sValue) == 1);
    for(i = 0; i < nValues; i++) {
        assert(xdr_template(xdrs, &sValue) == 1);
        if(sValue > sMax)
            sMax = sValue;
        if(sValue < sMin)
            sMin = sValue;
        }
    std::cout << sMin << " " << sMax << std::endl;
    }

using namespace TypeHandling;

int main(int argc, char** argv) {
    if(argc > 1) {
        std::cerr << "Usage: minmax_field < field_file" << std::endl;
        return 1;
        }
    XDR xdrs1;
    xdrstdio_create(&xdrs1, stdin, XDR_DECODE);
    FieldHeader fh1;
    if(!xdr_template(&xdrs1, &fh1)) {
        std::cerr << "Couldn't read header from file!" << std::endl;
        return 4;
	}
    if(fh1.magic != FieldHeader::MagicNumber) {
        std::cerr << "Field file is corrupt" << std::endl;
        return 5;
	}

    switch(fh1.code) {
#undef caseCode2Type
#define caseCode2Type(enumName,typeName) \
	    case enumName: \
		minmax_field<typeName>(&xdrs1, fh1);  \
	    break;
	    
	    casesCode2Types
		}

    xdr_destroy(&xdrs1);
}
