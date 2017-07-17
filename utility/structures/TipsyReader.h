/** @file TipsyReader.h
 A class that reads a tipsy format file from a stream.
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created February 12, 2003
 @version 1.0
 */

#ifndef TIPSYREADER_H
#define TIPSYREADER_H

#include <fstream>
#include <string>
#include <vector>

#include "TipsyParticles.h"
#include "Vector3D.h"
#include "xdr_template.h"
#include "assert.h"

namespace Tipsy {

/** The header in a tipsy format file. */
class header {
public:	
	static const unsigned int sizeBytes = 28; // Only for an x86 machine
	
	/// The time of the output
    double time;
	/// The number of particles of all types in this file
    unsigned int nbodies;
	/// The number of dimensions, must be equal to MAXDIM
    int ndim;
	/// The number of SPH (gas) particles in this file
    unsigned int nsph;
	/// The number of dark matter particles in this file
    unsigned int ndark;
	/// The number of star particles in this file
    unsigned int nstar;
    //int pad; //unused on x86
	
	header(int nGas = 0, int nDark = 0, int nStar = 0) : time(0), nbodies(nGas + nDark + nStar), ndim(MAXDIM), nsph(nGas), ndark(nDark), nstar(nStar) { }
	
	/// Output operator, used for formatted display
	friend std::ostream& operator<<(std::ostream& os, const header& h) {
		return os << "Time: " << h.time
			<< "\nnBodies: " << h.nbodies
			<< "\nnDim: " << h.ndim
			<< "\nnSPH: " << h.nsph
			<< "\nnDark: " << h.ndark
			<< "\nnStar: " << h.nstar;
	}
};

/** A particle-at-a-time reader for Tipsy files.
 On little-endian machines, can read native and standard format files.
 On big-endian machine, can read standard format files. 
 */
class TipsyReader {
	bool native;
	bool ok;
	
	unsigned int numGasRead, numDarksRead, numStarsRead;
	
	header h;
	
	bool responsible;
        bool bDoublePos;
        bool bDoubleVel;
        
        std::streampos gas_size;
        std::streampos dark_size;
        std::streampos star_size;
        
        void set_sizes()
        {
            if(bDoubleVel && bDoublePos) {
                gas_size = gas_particle_t<double,double>::sizeBytes;
                dark_size = dark_particle_t<double,double>::sizeBytes;
                star_size = star_particle_t<double,double>::sizeBytes;
            } else if (bDoublePos) {
                gas_size = gas_particle_t<double,float>::sizeBytes;
                dark_size = dark_particle_t<double,float>::sizeBytes;
                star_size = star_particle_t<double,float>::sizeBytes;
            } else if(bDoubleVel) {
                gas_size = gas_particle_t<float,double>::sizeBytes;
                dark_size = dark_particle_t<float,double>::sizeBytes;
                star_size = star_particle_t<float,double>::sizeBytes;
            } else {
                gas_size = gas_particle_t<float,float>::sizeBytes;
                dark_size = dark_particle_t<float,float>::sizeBytes;
                star_size = star_particle_t<float,float>::sizeBytes;
            }
        }
                
 public:
	std::istream* tipsyStream;
	
	bool loadHeader();
	
 private:
	//copy constructor and equals operator are private to prevent their use (use takeOverStream instead)
	TipsyReader(const TipsyReader& r);
	TipsyReader& operator=(const TipsyReader& r);
	
public:
	
	TipsyReader() : native(true), ok(false), responsible(false),
            tipsyStream(0), bDoublePos(false), bDoubleVel(false) { set_sizes();
            }

	/// Load from a file
        TipsyReader(const std::string& filename, bool _bDP = false, bool _bDV = false) : ok(false), responsible(true), bDoublePos(_bDP), bDoubleVel(_bDV) {
		tipsyStream = new std::ifstream(filename.c_str(), std::ios::in | std::ios::binary);
		if(!(*tipsyStream)) throw std::ios_base::failure("Bad file open");
		loadHeader();
	}
	
	/** Load from a stream.
	 @note When using the stream interface, the given stream's buffer must
	 outlast your use of the reader object, since the original stream owns
	 the buffer.  For the standard stream cin this is guaranteed.
	 */
        TipsyReader(std::istream& is, bool _bDP = false, bool _bDV = false) : ok(false), responsible(true), bDoublePos(_bDP), bDoubleVel(_bDV) {
		tipsyStream = new std::istream(is.rdbuf());
		loadHeader();
	}
	
	/// Use this instead of a copy constructor
	void takeOverStream(TipsyReader& r) {
		native = r.native;
		ok = r.ok;
		numGasRead = r.numGasRead;
		numDarksRead = r.numDarksRead;
		numStarsRead = r.numStarsRead;
		h = r.h;
		if(responsible)
			delete tipsyStream;
		responsible = true;
		r.responsible = false;
		tipsyStream = r.tipsyStream;
                bDoublePos = r.bDoublePos;
                bDoubleVel = r.bDoubleVel;
                set_sizes();
	}
	
	/** Reload from a file.
	 */
	bool reload(const std::string& filename) {
		if(responsible)
			delete tipsyStream;
		tipsyStream = new std::ifstream(filename.c_str(), std::ios::in | std::ios::binary);
		responsible = true;
		return loadHeader();
	}
	
	/** Reload from a stream.
	 @note When using the stream interface, the given stream's buffer must
	 outlast your use of the reader object, since the original stream owns
	 the buffer.  For the standard stream cin this is guaranteed.
	 */
	bool reload(std::istream& is) {
		if(responsible)
			delete tipsyStream;
		tipsyStream = new std::istream(is.rdbuf());
		responsible = true;
		return loadHeader();		
	}
	
	~TipsyReader() {
		if(responsible)
			delete tipsyStream;
	}
	
	header getHeader() {
		return h;
	}
	
	bool getNextSimpleParticle(simple_particle& p);
	
        template <typename TPos, typename TVel>
            bool getNextGasParticle_t(gas_particle_t<TPos, TVel> & p);
	bool getNextGasParticle(gas_particle& p)
            { return getNextGasParticle_t<float,float>(p);}
        
        template <typename TPos, typename TVel>
            bool getNextDarkParticle_t(dark_particle_t<TPos, TVel> & p);
	bool getNextDarkParticle(dark_particle& p)
            { return getNextDarkParticle_t<float,float>(p);}
        
        template <typename TPos, typename TVel>
            bool getNextStarParticle_t(star_particle_t<TPos, TVel> & p);
	bool getNextStarParticle(star_particle& p)
            { return getNextStarParticle_t<float,float>(p);}
        
	bool readAllParticles(gas_particle* gas, dark_particle* darks, star_particle* stars);
	bool readAllParticles(std::vector<gas_particle>& gas, std::vector<dark_particle>& darks, std::vector<star_particle>& stars);
	
	/// Is this file in native byte-order
	bool isNative() const {
		return native;
	}
	
	bool status() const {
		return ok;
	}
	bool isDoublePos() const {
		return bDoublePos;
	}
	bool isDoubleVel() const {
		return bDoubleVel;
	}
	
	bool seekParticleNum(unsigned int num);
	
	bool skipParticles(unsigned int num);
	
	/// Returns the index of the next particle to be read.
	unsigned int tellParticleNum() const {
		return numGasRead + numDarksRead + numStarsRead;
	}
};

#ifdef __CHARMC__
#include <converse.h>
#endif

class TipsyWriter {
	bool native;
	bool ok;
	FILE *tipsyFp;
	XDR xdrs;
	header h;
        bool bDoublePos;
        bool bDoubleVel;

        std::streampos gas_size;
        std::streampos dark_size;
        std::streampos star_size;

        void set_sizes()
        {
            if(bDoubleVel && bDoublePos) {
                gas_size = gas_particle_t<double,double>::sizeBytes;
                dark_size = dark_particle_t<double,double>::sizeBytes;
                star_size = star_particle_t<double,double>::sizeBytes;
            } else if (bDoublePos) {
                gas_size = gas_particle_t<double,float>::sizeBytes;
                dark_size = dark_particle_t<double,float>::sizeBytes;
                star_size = star_particle_t<double,float>::sizeBytes;
            } else if(bDoubleVel) {
                gas_size = gas_particle_t<float,double>::sizeBytes;
                dark_size = dark_particle_t<float,double>::sizeBytes;
                star_size = star_particle_t<float,double>::sizeBytes;
            } else {
                gas_size = gas_particle_t<float,float>::sizeBytes;
                dark_size = dark_particle_t<float,float>::sizeBytes;
                star_size = star_particle_t<float,float>::sizeBytes;
            }
        }

	/* hide copy constructor and assignment */
	TipsyWriter(const TipsyWriter& r);
	TipsyWriter& operator=(const TipsyWriter& r);
	
 public:
	
	bool writeHeader();
	
	/// Write to a file
	TipsyWriter(const std::string& filename, header &parh,
                    bool pnative=false, bool _bDP = false, bool _bDV = false)
	    : native(pnative), ok(false), h(parh), bDoublePos(_bDP),
            bDoubleVel(_bDV) {
#ifdef __CHARMC__
	    tipsyFp = CmiFopen(filename.c_str(), "a");  // Create file
#else
	    tipsyFp = fopen(filename.c_str(), "a");  // Create file
#endif
	    if(tipsyFp == NULL) {
		ok = false;
		assert(0);
		return;
		}
#ifdef __CHARMC__
	    CmiFclose(tipsyFp);
	    tipsyFp = CmiFopen(filename.c_str(), "rb+");
#else
	    fclose(tipsyFp);
	    tipsyFp = fopen(filename.c_str(), "rb+");
#endif
	    if(tipsyFp == NULL) {
		ok = false;
		assert(0);
		return;
		}
	    if(!native)
		xdrstdio_create(&xdrs, tipsyFp, XDR_ENCODE);
	    ok = true;
            set_sizes();
	}
	
	~TipsyWriter() {
	    if(tipsyFp == NULL)
		return;		/* file never opened */
	    if(!native)
		xdr_destroy(&xdrs);
#ifdef __CHARMC__
	    int result = CmiFclose(tipsyFp);
#else
	    int result = fclose(tipsyFp);
#endif
		if (result!=0) assert(0);
	}
	
        template <typename TPos, typename TVel>
            bool putNextGasParticle_t(gas_particle_t<TPos, TVel>& p);
        bool putNextGasParticle(gas_particle& p)
            { return putNextGasParticle_t<float,float>(p);}

        template <typename TPos, typename TVel>
            bool putNextDarkParticle_t(dark_particle_t<TPos, TVel>& p);
        bool putNextDarkParticle(dark_particle& p)
            { return putNextDarkParticle_t<float,float>(p);}

        template <typename TPos, typename TVel>
            bool putNextStarParticle_t(star_particle_t<TPos, TVel>& p);
        bool putNextStarParticle(star_particle& p)
            { return putNextStarParticle_t<float,float>(p);}
	
	//	bool writeAllParticles(gas_particle* gas, dark_particle* darks, star_particle* stars);
	//	bool writeAllParticles(std::vector<gas_particle>& gas, std::vector<dark_particle>& darks, std::vector<star_particle>& stars);
	
	/// Is this file in native byte-order
	bool isNative() const {
		return native;
	}
	
	bool status() const {
		return ok;
	}
	
	bool seekParticleNum(unsigned int num);
};

} //close namespace Tipsy

#ifdef __CHARMC__
#include "pup.h"
#include "charm++.h"

inline void operator|(PUP::er& p, Tipsy::header& h) {
	p | h.time;
	p | h.nbodies;
	p | h.ndim;
	p | h.nsph;
	p | h.ndark;
	p | h.nstar;
}

class CkOStream;

inline CkOStream& operator<<(CkOStream& os, const Tipsy::header& h) {
    return os << "Time: " << h.time
	      << "\nnBodies: " << h.nbodies
	      << "\nnDim: " << h.ndim
	      << "\nnSPH: " << h.nsph
	      << "\nnDark: " << h.ndark
	      << "\nnStar: " << h.nstar;
}

#endif //__CHARMC__

#endif //TIPSYREADER_H
