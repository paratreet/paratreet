/** @file tipsy_getsoft.cpp
 Get the softening out of a tipsy file and stick in into an attribute file.
 */

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <algorithm>
#include <cmath>

#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "TipsyFile.h"
#include "OrientedBox.h"
#include "tree_xdr.h"
#include "aggregate_xdr.h"

using namespace std;
using namespace Tipsy;
using namespace TypeHandling;

static double fh_time; // gross, but quick way to get time

// Returns total number of particles of a given type.
// Assumes the position attribute is available
int64_t getCount(char *typedir // directory containing the data for
		 // the type of particle of interest
		 )
{
    const int FILELEN = 256;
    char filename[FILELEN];

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/position");

    FILE* infile = fopen(filename, "rb");
    if(!infile) {
	cerr << "Couldn't open field file \"" << filename << "\"" << endl;
	return 0;  // Assume there is none of this particle type
	}

    XDR xdrs;
    FieldHeader fh;
    xdrstdio_create(&xdrs, infile, XDR_DECODE);

    if(!xdr_template(&xdrs, &fh)) {
	throw XDRException("Couldn't read header from file!");
	}
    if(fh.magic != FieldHeader::MagicNumber) {
	throw XDRException("This file does not appear to be a field file (magic number doesn't match).");
	}
    if(fh.dimensions != 3) {
	throw XDRException("Wrong dimension of positions.");
	}
    fh_time = fh.time;
    xdr_destroy(&xdrs);
    fclose(infile);
    return fh.numParticles;
    }

bool writeSoft(char* typedir, int64_t nPart, float soft)
{
    const int FILELEN = 256;
    char filename[FILELEN];

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/softening");

    FILE* outfile = fopen(filename, "wb");
    XDR xdrs;
	
    FieldHeader fh;
    fh.time = fh_time;
    fh.numParticles = nPart;
	
    fh.dimensions = 1;
    fh.code = float32;
    xdrstdio_create(&xdrs, outfile, XDR_ENCODE);

    if(!xdr_template(&xdrs, &fh)) {
	throw XDRException("Couldn't write header to file!");
	}
    if(!xdr_template(&xdrs, &soft) || !xdr_template(&xdrs, &soft)) {
	throw XDRException("Couldn't write min/max to file!");
	}
    xdr_destroy(&xdrs);
    fclose(outfile);	
	
    return true;
}

int main(int argc, char** argv) {
    if(argc < 3) {
	cerr << "Usage: tipsy_getsoft tipsyfile salsadir" << endl;
	exit(-1);
	}
    char *tipfname = argv[1];

    TipsyReader r(tipfname);
    if(!r.status()) {
	cerr << "Couldn't open tipsy file correctly!  Maybe it's not a tipsy file?" << endl;
	return 2;
	}
    assert(r.getHeader().nsph > 0);
    r.seekParticleNum(0);
    gas_particle gp;
    r.getNextGasParticle(gp);
    r.seekParticleNum(r.getHeader().nsph);
    assert(r.getHeader().ndark > 0);
    dark_particle dp;
    r.getNextDarkParticle(dp);
    
    const int FILELEN = 256;
    char filename[FILELEN];
    strncpy(filename, argv[1], FILELEN);
    strcat(filename, "/gas");
    int64_t nGas = getCount(filename);
    writeSoft(filename, nGas, gp.hsmooth);

    strncpy(filename, argv[1], FILELEN);
    strcat(filename, "/dark");
    int64_t nDark = getCount(filename);
    writeSoft(filename, nDark, dp.eps);

    strncpy(filename, argv[1], FILELEN);
    strcat(filename, "/star");
    int64_t nStar= getCount(filename);
    writeSoft(filename, nStar, gp.hsmooth);

}
