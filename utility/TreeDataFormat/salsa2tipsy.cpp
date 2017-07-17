// Convert a salsa file into Tipsy format.
// Matches gasoline NChilada output.

#include <iostream>
#include <cstdio>
#include <string.h>
#include <assert.h>
#include <unistd.h>

#include "tree_xdr.h"
#include "TipsyFile.h"

using namespace std;
using namespace TypeHandling;
using namespace Tipsy;

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
    strcat(filename, "/pos");

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

void *readFieldData(char *filename, FieldHeader &fh, unsigned int dim)
{
    FILE* infile = fopen(filename, "rb");
    if(!infile) {
	string smess("Couldn't open field file: ");
	smess += filename;
	throw XDRException(smess);
	}

    XDR xdrs;
    xdrstdio_create(&xdrs, infile, XDR_DECODE);

    if(!xdr_template(&xdrs, &fh)) {
	throw XDRException("Couldn't read header from file!");
	}
    if(fh.magic != FieldHeader::MagicNumber) {
	throw XDRException("This file does not appear to be a field file (magic number doesn't match).");
	}
    if(fh.dimensions != dim) {
	throw XDRException("Wrong dimension of positions.");
	}

    void* data = readField(fh, &xdrs);
	
    if(data == 0) {
	throw XDRException("Had problems reading in the field");
	}
    xdr_destroy(&xdrs);
    fclose(infile);
    return data;
    }

template <class PartVecT>
void getPos(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/pos");
    
    void *data = readFieldData(filename, fh, 3);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    for(unsigned int j = 0; j < fh.dimensions; ++j)
		p[i].pos[j] = static_cast<float *>(data)[fh.dimensions * i + j];
	    break;
	case float64:
	    for(unsigned int j = 0; j < fh.dimensions; ++j)
		p[i].pos[j] = static_cast<double *>(data)[fh.dimensions * i + j];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

template <class PartVecT>
void getVel(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/vel");
    
    void *data = readFieldData(filename, fh, 3);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    for(unsigned int j = 0; j < fh.dimensions; ++j)
		p[i].vel[j] = static_cast<float *>(data)[fh.dimensions * i + j];
	    break;
	case float64:
	    for(unsigned int j = 0; j < fh.dimensions; ++j)
		p[i].vel[j] = static_cast<double *>(data)[fh.dimensions * i + j];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

template <class PartVecT>
void getMass(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/mass");
    
    void *data = readFieldData(filename, fh, 1);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    p[i].mass = static_cast<float *>(data)[fh.dimensions * i];
	    break;
	case float64:
	    p[i].mass = static_cast<double *>(data)[fh.dimensions * i];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

template <class PartVecT>
void getSoft(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/soft");
    
    void *data = readFieldData(filename, fh, 1);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    p[i].eps = static_cast<float *>(data)[fh.dimensions * i];
	    break;
	case float64:
	    p[i].eps = static_cast<double *>(data)[fh.dimensions * i];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

template <class PartVecT>
void getPhi(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/pot");
    if(access(filename, F_OK) != 0) {  // no potentials; set to zero
        for(uint64_t i = 0; i < fh.numParticles; ++i)
            p[i].phi = 0.0;
        return;
    }
    
    
    void *data = readFieldData(filename, fh, 1);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    p[i].phi = static_cast<float *>(data)[fh.dimensions * i];
	    break;
	case float64:
	    p[i].phi = static_cast<double *>(data)[fh.dimensions * i];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

template <class PartVecT>
void getHSmooth(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/soft"); 	// N.B. the "hsmooth" gas field is
					// really gravitational softening
    
    void *data = readFieldData(filename, fh, 1);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    p[i].hsmooth = static_cast<float *>(data)[fh.dimensions * i];
	    break;
	case float64:
	    p[i].hsmooth = static_cast<double *>(data)[fh.dimensions * i];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

template <class PartVecT>
void getRho(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/GasDensity");
    
    void *data = readFieldData(filename, fh, 1);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    p[i].rho = static_cast<float *>(data)[fh.dimensions * i];
	    break;
	case float64:
	    p[i].rho = static_cast<double *>(data)[fh.dimensions * i];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

template <class PartVecT>
void getTemp(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/temperature");
    
    void *data = readFieldData(filename, fh, 1);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    p[i].temp = static_cast<float *>(data)[fh.dimensions * i];
	    break;
	case float64:
	    p[i].temp = static_cast<double *>(data)[fh.dimensions * i];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

// N.B. The following two routines assume that "metals" is the sum of Ox
// and Fe, which was the Gasoline convention prior to summer 2011.
// getMetalsOx has to be called first.

template <class PartVecT>
void getMetalsOx(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/OxMassFrac");
    
    void *data = readFieldData(filename, fh, 1);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    p[i].metals = static_cast<float *>(data)[fh.dimensions * i];
	    break;
	case float64:
	    p[i].metals = static_cast<double *>(data)[fh.dimensions * i];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

template <class PartVecT>
void getMetalsFe(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/FeMassFrac");
    
    void *data = readFieldData(filename, fh, 1);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    p[i].metals += static_cast<float *>(data)[fh.dimensions * i];
	    break;
	case float64:
	    p[i].metals += static_cast<double *>(data)[fh.dimensions * i];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

template <class PartVecT>
void getTForm(PartVecT &p, // reference to particle array
	    char *typedir // directory with file
	    )
{
    const int FILELEN = 256;
    char filename[FILELEN];
    FieldHeader fh;

    strncpy(filename, typedir, FILELEN);
    strcat(filename, "/timeform");
    
    void *data = readFieldData(filename, fh, 1);

    for(uint64_t i = 0; i < fh.numParticles; ++i) {
	switch(fh.code) {
	case float32:
	    p[i].tform = static_cast<float *>(data)[fh.dimensions * i];
	    break;
	case float64:
	    p[i].tform = static_cast<double *>(data)[fh.dimensions * i];
	    break;
	default:
	    throw XDRException("I don't recognize the type of this field!");
	    }
	    }
	
    deleteField(fh, data);
    }

int main(int argc, char** argv) {
	if(argc < 3) {
		cerr << "Usage: salsa2tipsy basedir outfile" << endl;
		return 1;
	    }
	FieldHeader fh;
	const int FILELEN = 256;
	char filename[FILELEN];
	
	strncpy(filename, argv[1], FILELEN);
	strcat(filename, "/dark");
	int64_t nDark = getCount(filename);

	strncpy(filename, argv[1], FILELEN);
	strcat(filename, "/gas");
	int64_t nSph = 0;
        if(access(filename, F_OK) == 0)
            nSph = getCount(filename);

	strncpy(filename, argv[1], FILELEN);
	strcat(filename, "/star");
        int64_t nStar= 0;
        if(access(filename, F_OK) == 0)
            nStar= getCount(filename);

	TipsyFile tf("tst", nSph, nDark, nStar);
	
	tf.h.time = fh_time;

	strncpy(filename, argv[1], FILELEN);
	strcat(filename, "/dark");
	getPos(tf.darks, filename);
	getMass(tf.darks, filename);
	getVel(tf.darks, filename);
	getSoft(tf.darks, filename);
	getPhi(tf.darks, filename);

        if(nSph > 0) {
            strncpy(filename, argv[1], FILELEN);
            strcat(filename, "/gas");
            getPos(tf.gas, filename);
            getMass(tf.gas, filename);
            getVel(tf.gas, filename);
            getPhi(tf.gas, filename);
            getHSmooth(tf.gas, filename);
            getRho(tf.gas, filename);
            getTemp(tf.gas, filename);
            getMetalsOx(tf.gas, filename);
            getMetalsFe(tf.gas, filename);
            }

        if(nStar > 0) {
            strncpy(filename, argv[1], FILELEN);
            strcat(filename, "/star");
            getPos(tf.stars, filename);
            getMass(tf.stars, filename);
            getVel(tf.stars, filename);
            getSoft(tf.stars, filename);
            getPhi(tf.stars, filename);
            getMetalsOx(tf.stars, filename);
            getMetalsFe(tf.stars, filename);
            getTForm(tf.stars, filename);
            }
	
	Tipsy::TipsyWriter w(argv[2], tf.h);
	w.writeHeader();
	w.seekParticleNum(0);
	for(int64_t i = 0; i < nSph; i++)
	    assert(w.putNextGasParticle(tf.gas[i]));
	for(int64_t i = 0; i < nDark; i++)
	    assert(w.putNextDarkParticle(tf.darks[i]));
	for(int64_t i = 0; i < nStar; i++)
	    assert(w.putNextStarParticle(tf.stars[i]));
	    
	cerr << "Done." << endl;	
    }
