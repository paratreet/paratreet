/** @file NChilReader.h
  A class that read an NChilada format field file and element at a
  time.
*/

#ifndef NCHILREADER_H
#define NCHILREADER_H

#include "tree_xdr.h"

/// @brief Read in a field an element at a time.
template <typename T>
class NChilReader {
    FieldHeader fh;
    bool ok;
    bool bConstField;   /// Field is just a constant: compress storage
    XDR xdrs;
    T min;
    T max;
 public:
    FILE * fieldStream;
    bool loadHeader();
private:
//copy constructor and equals operator are private to prevent their use
    NChilReader(const NChilReader& r);
    NChilReader& operator=(const NChilReader& r);
 public:
NChilReader() : ok(false) {}
    /// Load from a file
NChilReader(const std::string& filename) : ok(false) {
        fieldStream = fopen(filename.c_str(), "rb");
        if(fieldStream != NULL) {
            xdrstdio_create(&xdrs, fieldStream, XDR_DECODE);
            loadHeader();
            }
	}
    ~NChilReader() {
        if(ok) {
            xdr_destroy(&xdrs);
            fclose(fieldStream);
            }
	}
    bool status() const {
        return ok;
	}
    FieldHeader getHeader() {
        return fh;
	}
    T getMin() {
        return min;
	}
    T getMax() {
        return max;
	}
    bool getNextParticle(T & p);
    };

template <typename T>
class NChilWriter {
    FieldHeader fh;
    bool ok;
    XDR xdrs;
    T min;
    T max;
 public:
    FILE * fieldStream;
    bool writeHeader();
private:
//copy constructor and equals operator are private to prevent their use
    NChilWriter(const NChilWriter& r);
    NChilWriter& operator=(const NChilWriter& r);
public:
    /// Write to a file
 NChilWriter(const std::string& filename, FieldHeader parfh, T parmin,
     T parmax) : fh(parfh), ok(false), min(parmin), max(parmax) {
        fieldStream = fopen(filename.c_str(), "a"); // Create file
        if(fieldStream == NULL) throw std::ios_base::failure("Bad file open");
        fclose(fieldStream);
        fieldStream = fopen(filename.c_str(), "rb+");
        xdrstdio_create(&xdrs, fieldStream, XDR_ENCODE);
        ok = true;
	}
    ~NChilWriter() {
        if(ok) {
            xdr_destroy(&xdrs);
	    int result = fclose(fieldStream);
            if (result!=0) assert(0);
            }
	}
    bool status() const {
        return ok;
	}
    FieldHeader getHeader() {
        return fh;
	}
    bool putNextParticle(T & p);
    };

#endif //NCHILREADER_H
