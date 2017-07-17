/** @file TipsyReader.cpp
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created February 12, 2003
 @version 1.0
 */

#include <assert.h>
#include <errno.h>
#include "config.h"
#include "xdr_template.h"

#include "TipsyReader.h"

/// XDR conversion for the header structure
inline bool_t xdr_template(XDR* xdrs, Tipsy::header* val) {
	return (xdr_template(xdrs, &(val->time))
			&& xdr_template(xdrs, &(val->nbodies))
			&& xdr_template(xdrs, &(val->ndim)) 
			&& xdr_template(xdrs, &(val->nsph))
			&& xdr_template(xdrs, &(val->ndark))
			&& xdr_template(xdrs, &(val->nstar)));
}

///XDR conversions for the particle types
inline bool_t xdr_template(XDR* xdrs, Tipsy::simple_particle* p) {
	return (xdr_template(xdrs, &(p->mass))
		&& xdr_template(xdrs, &(p->pos))
		&& xdr_template(xdrs, &(p->vel)));
}

template <typename TPos, typename TVel>
inline bool_t xdr_template(XDR* xdrs, Tipsy::gas_particle_t<TPos,TVel>* p) {
	return (xdr_template(xdrs, &(p->mass))
		&& xdr_template(xdrs, &(p->pos))
		&& xdr_template(xdrs, &(p->vel))
		&& xdr_template(xdrs, &(p->rho))
		&& xdr_template(xdrs, &(p->temp))
		&& xdr_template(xdrs, &(p->hsmooth))
		&& xdr_template(xdrs, &(p->metals))
		&& xdr_template(xdrs, &(p->phi)));
}

template <typename TPos, typename TVel>
inline bool_t xdr_template(XDR* xdrs, Tipsy::dark_particle_t<TPos,TVel>* p) {
	return (xdr_template(xdrs, &(p->mass))
		&& xdr_template(xdrs, &(p->pos))
		&& xdr_template(xdrs, &(p->vel))
		&& xdr_template(xdrs, &(p->eps))
		&& xdr_template(xdrs, &(p->phi)));
}

template <typename TPos, typename TVel>
inline bool_t xdr_template(XDR* xdrs, Tipsy::star_particle_t<TPos,TVel>* p) {
	return (xdr_template(xdrs, &(p->mass))
		&& xdr_template(xdrs, &(p->pos))
		&& xdr_template(xdrs, &(p->vel))
		&& xdr_template(xdrs, &(p->metals))
		&& xdr_template(xdrs, &(p->tform))
		&& xdr_template(xdrs, &(p->eps))
		&& xdr_template(xdrs, &(p->phi)));
}

namespace Tipsy {

bool TipsyReader::loadHeader() {
	ok = false;
	
	if(!(*tipsyStream)) {
		throw std::ios_base::failure("Bad file open");
		return false;
	    }
	
	//read the header in
	tipsyStream->read(reinterpret_cast<char *>(&h), sizeof(h));
	if(!*tipsyStream)
		return false;
	
	if(h.ndim != MAXDIM) { //perhaps it's XDR
		XDR xdrs;
		xdrmem_create(&xdrs, reinterpret_cast<char *>(&h), header::sizeBytes, XDR_DECODE);
		if(!xdr_template(&xdrs, &h) || h.ndim != MAXDIM) { //wasn't xdr format either			
			h.nbodies = h.nsph = h.ndark = h.nstar = 0;
			xdr_destroy(&xdrs);
			return false;
		}
		xdr_destroy(&xdrs);
		
		native = false;
		// xdr format has an integer pad in the header,
		// which we don't need, but must skip.
		// However, if the native format has the pad (the compiler
		// pads structures, then we already took in the pad above.
		if(sizeof(h) != 32) {
		    int pad = 0;
                    assert(sizeof(h) == 28); // Assume that native format
					     // is unpadded.
		    tipsyStream->read(reinterpret_cast<char *>(&pad), 4);
		    }
		
		if(!*tipsyStream)
			return false;
		
	} else
		native = true;

        // determine double/single from size of file.
        std::streampos current = tipsyStream->tellg();
        tipsyStream->seekg(0,tipsyStream->end);
        // file size without header
        uint64_t fsize = tipsyStream->tellg() - current;
        tipsyStream->seekg(current,tipsyStream->beg);
        if(fsize == (((uint64_t)h.nsph)*gas_particle_t<float,float>::sizeBytes +
                     ((uint64_t)h.ndark)*dark_particle_t<float,float>::sizeBytes +
                     ((uint64_t)h.nstar)*star_particle_t<float,float>::sizeBytes)) {
            bDoublePos = false;
            bDoubleVel = false;
        }
        else if(fsize == (((uint64_t)h.nsph)*gas_particle_t<double,float>::sizeBytes +
                          ((uint64_t)h.ndark)*dark_particle_t<double,float>::sizeBytes +
                          ((uint64_t)h.nstar)*star_particle_t<double,float>::sizeBytes)) {
            bDoublePos = true;
            bDoubleVel = false;
            }
        else if(fsize == (((uint64_t)h.nsph)*gas_particle_t<double,double>::sizeBytes +
                          ((uint64_t)h.ndark)*dark_particle_t<double,double>::sizeBytes +
                          ((uint64_t)h.nstar)*star_particle_t<double,double>::sizeBytes)) {
            bDoublePos = true;
            bDoubleVel = true;
            }
        else{
            throw std::ios_base::failure("Bad file size");
		return false;
        }
	
        set_sizes();
	numGasRead = numDarksRead = numStarsRead = 0;
	ok = true;
	return ok;
}

/** Read the next particle, and get the simple_particle representation of it.
 Returns true if successful.  Returns false if the read failed, or no more particles.
 */
bool TipsyReader::getNextSimpleParticle(simple_particle& p) {
	if(!ok)
		return false;
	
	if(numGasRead < h.nsph) {
		++numGasRead;
		gas_particle gp;
		tipsyStream->read(reinterpret_cast<char *>(&gp), gas_particle::sizeBytes);
		if(!(*tipsyStream))
			return false;
		p = gp;
	} else if(numDarksRead < h.ndark) {
		++numDarksRead;
		dark_particle dp;
		tipsyStream->read(reinterpret_cast<char *>(&dp), dark_particle::sizeBytes);
		if(!(*tipsyStream))
			return false;
		p = dp;
	} else if(numStarsRead < h.nstar) {
		++numStarsRead;
		star_particle sp;
		tipsyStream->read(reinterpret_cast<char *>(&sp), star_particle::sizeBytes);
		if(!(*tipsyStream))
			return false;
		p = sp;
	} else //already read all the particles!
		return false;
	
	if(!native) {
		XDR xdrs;
		xdrmem_create(&xdrs, reinterpret_cast<char *>(&p), simple_particle::sizeBytes, XDR_DECODE);
		if(!xdr_template(&xdrs, &p))
			return false;
		xdr_destroy(&xdrs);
	}
	
	return true;
}

/** Get the next gas particle.
 Returns false if the read failed, or already read all the gas particles in this file.
 */
template <typename TPos, typename TVel>
bool TipsyReader::getNextGasParticle_t(gas_particle_t<TPos, TVel>& p) {
	if(!ok || !(*tipsyStream))
		return false;
        assert(p.sizeBytes == gas_size);
	
	if(numGasRead < h.nsph) {
		++numGasRead;
                if(native) {
                    tipsyStream->read((char *)&p.mass, sizeof(Real));
                    tipsyStream->read((char *)&p.pos, 3*sizeof(TPos));
                    tipsyStream->read((char *)&p.vel, 3*sizeof(TVel));
                    tipsyStream->read((char *)&p.rho, sizeof(Real));
                    tipsyStream->read((char *)&p.temp, sizeof(Real));
                    tipsyStream->read((char *)&p.hsmooth, sizeof(Real));
                    tipsyStream->read((char *)&p.metals, sizeof(Real));
                    tipsyStream->read((char *)&p.phi, sizeof(Real));
                    }
		else {
			XDR xdrs;
                        char buf[p.sizeBytes];
                        tipsyStream->read(buf, p.sizeBytes);
			xdrmem_create(&xdrs, buf, p.sizeBytes, XDR_DECODE);
			if(!xdr_template(&xdrs, &p))
				return false;
			xdr_destroy(&xdrs);
                    }
                if(numGasRead < h.nsph && !(*tipsyStream))
                    return false;
        } else
		return false;
	
	return true;
}

template bool TipsyReader::getNextGasParticle_t(gas_particle_t<float,float>& p);
template bool TipsyReader::getNextGasParticle_t(gas_particle_t<double,float>& p);
template bool TipsyReader::getNextGasParticle_t(gas_particle_t<double,double>& p);

/** Get the next dark particle.
 Returns false if the read failed, or already read all the dark particles in this file.
 */
template <typename TPos, typename TVel>
bool TipsyReader::getNextDarkParticle_t(dark_particle_t<TPos, TVel>& p) {
	if(!ok || !(*tipsyStream))
		return false;
        assert(p.sizeBytes == dark_size);
	
	if(numGasRead != h.nsph) { //some gas still not read, skip them
		if(!seekParticleNum(h.nsph))
			return false;
		numGasRead = h.nsph;
	}
	
	if(numDarksRead < h.ndark) {
		++numDarksRead;
                if(native) {
                    tipsyStream->read((char *)&p.mass, sizeof(Real));
                    tipsyStream->read((char *)&p.pos, 3*sizeof(TPos));
                    tipsyStream->read((char *)&p.vel, 3*sizeof(TVel));
                    tipsyStream->read((char *)&p.eps, sizeof(Real));
                    tipsyStream->read((char *)&p.phi, sizeof(Real));
                    }
		else {
			XDR xdrs;
                        char buf[p.sizeBytes];
                        tipsyStream->read(buf, p.sizeBytes);
			xdrmem_create(&xdrs, buf, p.sizeBytes, XDR_DECODE);
			if(!xdr_template(&xdrs, &p))
				return false;
			xdr_destroy(&xdrs);
		}
	// Hack to fix end of stream problem on Macs --trq
		if(numDarksRead < h.ndark && !(*tipsyStream))
			return false;
	} else
		return false;
	
	return true;
}

template bool TipsyReader::getNextDarkParticle_t(dark_particle_t<float,float>& p);
template bool TipsyReader::getNextDarkParticle_t(dark_particle_t<double,float>& p);
template bool TipsyReader::getNextDarkParticle_t(dark_particle_t<double,double>& p);

/** Get the next star particle.
 Returns false if the read failed, or already read all the star particles in this file.
 */
template <typename TPos, typename TVel>
bool TipsyReader::getNextStarParticle_t(star_particle_t<TPos, TVel>& p) {
	if(!ok || !(*tipsyStream))
		return false;
        assert(p.sizeBytes == star_size);
	
	if(numGasRead != h.nsph || numDarksRead != h.ndark) { //some gas and dark still not read, skip them
		if(!seekParticleNum(h.nsph + h.ndark))
			return false;
		numGasRead = h.nsph;
		numDarksRead = h.ndark;
	}
	
	if(numStarsRead < h.nstar) {
		++numStarsRead;
                if(native) {
                    tipsyStream->read((char *) &p.mass, sizeof(Real));
                    tipsyStream->read((char *) &p.pos, 3*sizeof(TPos));
                    tipsyStream->read((char *) &p.vel, 3*sizeof(TVel));
                    tipsyStream->read((char *) &p.metals, sizeof(Real));
                    tipsyStream->read((char *) &p.tform, sizeof(Real));
                    tipsyStream->read((char *) &p.eps, sizeof(Real));
                    tipsyStream->read((char *) &p.phi, sizeof(Real));
                    }
		else {
			XDR xdrs;
                        char buf[p.sizeBytes];
                        tipsyStream->read(buf, p.sizeBytes);
			xdrmem_create(&xdrs, buf, p.sizeBytes, XDR_DECODE);
			if(!xdr_template(&xdrs, &p))
				return false;
			xdr_destroy(&xdrs);
                        }
	// Hack to fix end of stream problem on Macs --trq
		if(numStarsRead < h.nstar && !(*tipsyStream))
			return false;
	} else
		return false;
	
	return true;
}

template bool TipsyReader::getNextStarParticle_t(star_particle_t<float,float>& p);
template bool TipsyReader::getNextStarParticle_t(star_particle_t<double,float>& p);
template bool TipsyReader::getNextStarParticle_t(star_particle_t<double,double>& p);

/** Read all the particles into arrays.
 Returns false if the read fails.
 */
bool TipsyReader::readAllParticles(gas_particle* gas, dark_particle* darks, star_particle* stars) {
	if(!ok || !(*tipsyStream))
		return false;
	
	//go back to the beginning of the file
	if(!seekParticleNum(0))
		return false;
	
	gas = new gas_particle[h.nsph];
	for(int i = 0; i < h.nsph; ++i) {
		if(!getNextGasParticle(gas[i]))
			return false;
	}
	darks = new dark_particle[h.ndark];
	for(int i = 0; i < h.ndark; ++i) {
		if(!getNextDarkParticle(darks[i]))
			return false;
	}
	stars = new star_particle[h.nstar];
	for(int i = 0; i < h.nstar; ++i) {
		if(!getNextStarParticle(stars[i]))
			return false;
	}
	
	return true;
}

/** Read all the particles into vectors.
 Returns false if the read fails.
 */
bool TipsyReader::readAllParticles(std::vector<gas_particle>& gas, std::vector<dark_particle>& darks, std::vector<star_particle>& stars) {
	if(!ok || !(*tipsyStream))
		return false;
	
	//go back to the beginning of the file
	if(!seekParticleNum(0))
		return false;
	
	gas.resize(h.nsph);
	for(int i = 0; i < h.nsph; ++i) {
		if(!getNextGasParticle(gas[i]))
			return false;
	}
	darks.resize(h.ndark);
	for(int i = 0; i < h.ndark; ++i) {
		if(!getNextDarkParticle(darks[i]))
			return false;
	}
	stars.resize(h.nstar);
	for(int i = 0; i < h.nstar; ++i) {
		if(!getNextStarParticle(stars[i]))
			return false;
	}
	
	return true;
}

/** Seek to the requested particle in the file.
 Returns false if the file seek fails, or if you request too large a particle.
 */
bool TipsyReader::seekParticleNum(unsigned int num) {
	int padSize = 0;
	if(!native)
		padSize = 4;
	else
		padSize = sizeof(h) - header::sizeBytes;

	unsigned int preface = header::sizeBytes + padSize;
	std::streampos seek_position;
	if(num < h.nsph) {
		seek_position = preface + num * gas_size;
		tipsyStream->seekg(seek_position);
		if(!(*tipsyStream))
			return false;
		numGasRead = num;
		numDarksRead = 0;
		numStarsRead = 0;
	} else if(num < (h.nsph + h.ndark)) {
		seek_position = preface + h.nsph * gas_size
                    + (num - h.nsph) * dark_size;
		tipsyStream->seekg(seek_position);
		if(!(*tipsyStream))
			return false;
		numGasRead = h.nsph;
		numDarksRead = num - h.nsph;
		numStarsRead = 0;
	} else if(num < (h.nsph + h.ndark + h.nstar)) {
		seek_position = preface + h.nsph * gas_size + h.ndark * dark_size + (num - h.ndark - h.nsph) * star_size;
		tipsyStream->seekg(seek_position);
		if(!(*tipsyStream))
			return false;
		numGasRead = h.nsph;
		numDarksRead = h.ndark;
		numStarsRead = num - h.ndark - h.nsph;
	} else
		return false;
	
	return true;
}

bool TipsyReader::skipParticles(unsigned int num) {
	for(unsigned int skipped = 0; skipped < num; ++skipped) {
		if(numGasRead < h.nsph) {
			++numGasRead;
			gas_particle_t<double,double> gp;
			tipsyStream->read(reinterpret_cast<char *>(&gp),  gas_size);
			if(!(*tipsyStream))
				return false;
		} else if(numDarksRead < h.ndark) {
			++numDarksRead;
			dark_particle_t<double,double> dp;
			tipsyStream->read(reinterpret_cast<char *>(&dp),  dark_size);
			if(!(*tipsyStream))
				return false;
		} else if(numStarsRead < h.nstar) {
			++numStarsRead;
			star_particle_t<double,double> sp;
			tipsyStream->read(reinterpret_cast<char *>(&sp),  star_size);
			if(!(*tipsyStream))
				return false;
		} else
			return false;
	}
	return true;
}

bool TipsyWriter::writeHeader() {
	
	if(!tipsyFp)
	    return false;
	
	int pad = 0;  // 4 byte pad
	
	if(native) {
	    fwrite(&h, sizeof(h), 1, tipsyFp);
	    if(sizeof(h) != 32) {
		assert(sizeof(h) == 28);
		fwrite(&pad, sizeof(pad), 1, tipsyFp);
		}
	    }
	else {
	    xdr_template(&xdrs, &h);
	    xdr_template(&xdrs, &pad);
	    }
	return true;
}

/** Write the next gas particle.
 Returns false if the write failed.
 */
template <typename TPos, typename TVel>
bool TipsyWriter::putNextGasParticle_t(gas_particle_t<TPos, TVel>& p) {
	if(!tipsyFp)
		return false;
        assert(p.sizeBytes == gas_size);
	
	if(native) {
	    fwrite(&p.mass, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.pos, 3*sizeof(TPos), 1, tipsyFp);
	    fwrite(&p.vel, 3*sizeof(TVel), 1, tipsyFp);
	    fwrite(&p.rho, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.temp, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.hsmooth, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.metals, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.phi, sizeof(Real), 1, tipsyFp);
	    }
	else {
	    if(!xdr_template(&xdrs, &p))
		return false;
	    }
	
	return true;
}

template bool TipsyWriter::putNextGasParticle_t(gas_particle_t<float,float>& p);
template bool TipsyWriter::putNextGasParticle_t(gas_particle_t<double,float>& p);
template bool TipsyWriter::putNextGasParticle_t(gas_particle_t<double,double>& p);

template <typename TPos, typename TVel>
bool TipsyWriter::putNextDarkParticle_t(dark_particle_t<TPos,TVel>& p) {
	if(!tipsyFp)
		return false;
        assert(p.sizeBytes == dark_size);
	
	if(native) {
	    fwrite(&p.mass, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.pos, 3*sizeof(TPos), 1, tipsyFp);
	    fwrite(&p.vel, 3*sizeof(TVel), 1, tipsyFp);
	    fwrite(&p.eps, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.phi, sizeof(Real), 1, tipsyFp);
	    }
	else {
	    if(!xdr_template(&xdrs, &p))
		return false;
	    }
	
	return true;
}

template bool TipsyWriter::putNextDarkParticle_t(dark_particle_t<float,float>& p);
template bool TipsyWriter::putNextDarkParticle_t(dark_particle_t<double,float>& p);
template bool TipsyWriter::putNextDarkParticle_t(dark_particle_t<double,double>& p);

template <typename TPos, typename TVel>
bool TipsyWriter::putNextStarParticle_t(star_particle_t<TPos,TVel>& p) {
	if(!tipsyFp)
		return false;
        assert(p.sizeBytes == star_size);
	
	if(native) {
	    fwrite(&p.mass, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.pos, 3*sizeof(TPos), 1, tipsyFp);
	    fwrite(&p.vel, 3*sizeof(TVel), 1, tipsyFp);
	    fwrite(&p.metals, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.tform, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.eps, sizeof(Real), 1, tipsyFp);
	    fwrite(&p.phi, sizeof(Real), 1, tipsyFp);
	    }
	else {
	    if(!xdr_template(&xdrs, &p))
		return false;
	    }
	
	return true;
}

template bool TipsyWriter::putNextStarParticle_t(star_particle_t<float,float>& p);
template bool TipsyWriter::putNextStarParticle_t(star_particle_t<double,float>& p);
template bool TipsyWriter::putNextStarParticle_t(star_particle_t<double,double>& p);

/** Seek to the requested particle in the file.
 Returns false if the file seek fails, or if you request too large a particle.
 */
bool TipsyWriter::seekParticleNum(unsigned int num) {
	int padSize = 0;
	if(!native)
		padSize = 4;
	else
		padSize = sizeof(h) - header::sizeBytes;

	unsigned int preface = header::sizeBytes + padSize;
	int64_t seek_position;
	if(!native)
	    xdr_destroy(&xdrs);
	
	if(num < (unsigned int) h.nsph) {
		seek_position = preface + num * (int64_t) gas_size;
	} else if(num < (h.nsph + h.ndark)) {
		seek_position = preface + h.nsph * (int64_t) gas_size + (num - h.nsph) * (int64_t) dark_size;
	} else if(num < (h.nsph + h.ndark + h.nstar)) {
		seek_position = preface + h.nsph * (int64_t) gas_size + h.ndark * (int64_t) dark_size + (num - h.ndark - h.nsph) * (int64_t) star_size;
	} else
		return false;
	
	int status = 0;
	while(1) {
	    status = fseek(tipsyFp, seek_position, 0);
	    if(status != 0 && errno == EINTR) {
		printf("Warning: fseek retrying ...\n");
		continue;
		}
	    else
		break;
	    }
	if(status != 0) return false;
	if(!native)
	    xdrstdio_create(&xdrs, tipsyFp, XDR_ENCODE);
	
	return true;
}
} //close namespace Tipsy
