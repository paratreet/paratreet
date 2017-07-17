/** @file NChilReader.cpp
 */

#include "NChilReader.h"

template <typename T>
bool NChilReader<T>::loadHeader() {
    ok = false;
    bConstField = false;
    if(!xdr_template(&xdrs, &fh)) {
        return false;
        }
    if(fh.magic != FieldHeader::MagicNumber)
        return false;
    xdr_template(&xdrs, &min);
    xdr_template(&xdrs, &max);
    if(min == max)
        bConstField = true;
    ok = checkType<T>(fh);
    return ok;
    }

template <typename T>
bool NChilReader<T>::getNextParticle(T & p) {
    if(bConstField) {
        p = min;
        return 1;
    }
    return xdr_template(&xdrs, &p);
    }

template <typename T>
bool NChilWriter<T>::writeHeader() {
	
	if(!ok)
	    return false;
	
        xdr_template(&xdrs, &fh);
        xdr_template(&xdrs, &min);
        xdr_template(&xdrs, &max);
	return true;
}

template <typename T>
bool NChilWriter<T>::putNextParticle(T & p) {
    return xdr_template(&xdrs, &p);
    }

template class NChilReader<float>;
template class NChilWriter<float>;
template class NChilReader<Vector3D<float> >;
template class NChilWriter<Vector3D<float> >;
