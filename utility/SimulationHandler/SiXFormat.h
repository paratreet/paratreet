/** @file SiXFormat.h
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created September 18, 2003
 @version 1.0
 */
 
#ifndef SIXFORMAT_H
#define SIXFORMAT_H

#include <xercesc/sax2/DefaultHandler.hpp>

#include "Simulation.h"

namespace SimulationHandling {

XERCES_CPP_NAMESPACE_USE

class SiXFormatReader : public Simulation, public DefaultHandler {
	friend class SiXFormatWriter;
	
	ParticleFamily* currentFamily;
	std::string pathPrefix;
protected:
	/// A map to the names of the files the attributes reside in (fully qualified filenames), indexed by the string "familyName:attributeName".
	std::map<std::string, std::string> attributeFiles;
public:
		
	SiXFormatReader(const std::string& directoryname);

	bool loadFromXMLFile(std::string directoryname);
	
	virtual bool loadAttribute(const std::string& familyName, const std::string& attributeName, int64_t numParticles = 0, const u_int64_t startParticle = 0);
	
	virtual void startElement(const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname, const Attributes& attrs);
	virtual void endElement(const XMLCh *const uri, const XMLCh* const localname, const XMLCh *const qname);
};

class SiXFormatWriter : public SimulationWriter {
public:
	virtual bool save(Simulation* sim, const std::string& path, int thisIndex);
};

} //close namespace SimulationHandling

#endif //SIXFORMAT_H
