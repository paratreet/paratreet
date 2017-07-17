/** @file SiXFormat.cpp
 Implementation of Simple XML file format for simulation data.
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created September 18, 2003
 @version 1.0
 */

#include "config.h"
#include <fstream>

#include <xercesc/util/PlatformUtils.hpp>
#include <xercesc/sax2/SAX2XMLReader.hpp>
#include <xercesc/sax2/XMLReaderFactory.hpp>
#include <xercesc/sax2/Attributes.hpp>
#include <sys/types.h>
#include <sys/stat.h>

#include "Group.h"


#include "SiXFormat.h"

namespace SimulationHandling {

using std::string;
using namespace TypeHandling;

SiXFormatReader::SiXFormatReader(const string& directoryname) {
	loadFromXMLFile(directoryname);
}

string makeString(const XMLCh* toConvert) {
	if(toConvert) {
		char* transcoded = XMLString::transcode(toConvert);
		string s(transcoded);
		XMLString::release(&transcoded);
		return s;
	} else
		return "";
}

bool SiXFormatReader::loadFromXMLFile(string directoryname) {
	//erase any previous contents
	release();
	clear();
	
	//remove any trailing slash
	if(directoryname.at(directoryname.size() - 1) == '/')
		directoryname.erase(directoryname.size() - 1, 1);
	//save the path to this directory
	pathPrefix = directoryname;
	//find any slashes
	string::size_type slashPos = directoryname.find_last_of("/");
	//save the directory's name as the simulation name
	if(slashPos == string::npos)
		name = directoryname;
	else
		name = directoryname.substr(slashPos + 1);
	
	// Test for file existence before going into XML
	FILE *fp = fopen((directoryname + "/description.xml").c_str(), "r");
	if(fp == NULL)
		return false;
	fclose(fp);
	//initialize XML system
    try {
		XMLPlatformUtils::Initialize();
    } catch(const XMLException& toCatch) {
		 return false;
    }
	
	//create parser
	SAX2XMLReader* parser = XMLReaderFactory::createXMLReader();
	
	//set features of parser (turn validation on)
	parser->setFeature(XMLUni::fgSAX2CoreValidation, true);
	parser->setFeature(XMLUni::fgXercesDynamic, false);
	
	//associate handler with parser
	try {
		parser->setContentHandler(this);
		parser->setErrorHandler(this);
		//parse it
		currentFamily = 0;
		//the xml file is inside the directory we were given
		parser->parse((directoryname + "/description.xml").c_str());
	} catch(const SAXException& toCatch) {
	    // throw(FileError(makeString(toCatch.getMessage())));
		std::cerr << "An XML error occurred: "
			  << makeString(toCatch.getMessage()) << std::endl;
		return false;
	}
	
	delete parser;
	XMLPlatformUtils::Terminate();
	
	return true;
}

const XMLCh nameString[] = {chLatin_n, chLatin_a, chLatin_m, chLatin_e, chNull};
const XMLCh linkString[] = {chLatin_l, chLatin_i, chLatin_n, chLatin_k, chNull};
const XMLCh attributeString[] = {chLatin_a, chLatin_t, chLatin_t, chLatin_r, chLatin_i, chLatin_b, chLatin_u, chLatin_t, chLatin_e, chNull};
const XMLCh familyString[] = {chLatin_f, chLatin_a, chLatin_m, chLatin_i, chLatin_l, chLatin_y, chNull};
const XMLCh treeString[] = {chLatin_t, chLatin_r, chLatin_e, chLatin_e, chNull};
void SiXFormatReader::startElement(const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname, const Attributes& attrs) {
	if(XMLString::equals(localname, familyString)) {
		const XMLCh* familyName = attrs.getValue(nameString);
		if(familyName)
			currentFamily = new ParticleFamily(makeString(familyName));
	} else if(currentFamily && XMLString::equals(localname, attributeString)) {
		const XMLCh* attributeName = attrs.getValue(nameString);
		if(attributeName) {
			//get link, open file, allocate and read in
			const XMLCh* link = attrs.getValue(linkString);
			if(link) {
				//get fully qualified path to the attribute file
				string filename = pathPrefix + "/" + makeString(link);
				FILE* infile = fopen(filename.c_str(), "rb");
				if(!infile) {  // try without prefix
				    filename = makeString(link);
				    infile = fopen(filename.c_str(), "rb");
				    }
				
				if(infile) {
					XDR xdrs;
					xdrstdio_create(&xdrs, infile, XDR_DECODE);
					if(XMLString::equals(attributeName, treeString)) {
						//do something with the tree
					} else {
						FieldHeader fh;
						if(xdr_template(&xdrs, &fh) && fh.magic == FieldHeader::MagicNumber) {
							//is this the first attribute for this family?
                                                        if(currentFamily->count.totalNumParticles == 0) {
								// then set the number of particles and time
								// from the first attribute file
								currentFamily->count.totalNumParticles = fh.numParticles;
                                                                time = fh.time;
                                                                }
                                                    
							//make sure the attribute file agrees on the number of particles
							if(fh.numParticles == currentFamily->count.totalNumParticles) {
								TypedArray attribute;
								attribute.dimensions = fh.dimensions;
								attribute.code = fh.code;
								readAttributes(&xdrs, attribute, 0);
								currentFamily->attributes[makeString(attributeName)] = attribute;
								attributeFiles[currentFamily->familyName + ":" + makeString(attributeName)] = filename;
							}
						}
						xdr_destroy(&xdrs);
						fclose(infile);
					}
				}
				else {
				    throw(FileError(filename));
				    }
				
			}
		}
	}
}

void SiXFormatReader::endElement(const XMLCh *const uri, const XMLCh *const localname, const XMLCh *const qname) {
	if(XMLString::equals(localname, familyString)) {
		//family has been specified, insert its entry into the map (we're the map)
		insert(make_pair(currentFamily->familyName, *currentFamily));
		delete currentFamily;
		currentFamily = 0;
	}
}

bool SiXFormatReader::loadAttribute(const string& familyName, const string& attributeName, int64_t numParticles, const u_int64_t startParticle) {
	iterator familyIter = find(familyName);
	if(familyIter == end()) {
	    throw(NameError(familyName));
	    return false;
	    }
	ParticleFamily& family = familyIter->second;
	//if asked for a negative number of particles, load them all
	if(numParticles < 0)
		numParticles = family.count.totalNumParticles;
	//is this the first attribute loaded?
	if(family.count.numParticles == 0) {
		family.count.numParticles = numParticles;
		family.count.startParticle = startParticle;
	    }
	else if(family.count.numParticles != (u_int64_t) numParticles
		|| family.count.startParticle != startParticle) {
	    std::cerr << "Bad Particle Number: " << numParticles << std::endl;
	    return false;
	    }
	
	if(numParticles + startParticle > family.count.totalNumParticles) {
	    std::cerr << "Bad Particle Number: " << numParticles << std::endl;
	    return false;
	    }
	
	AttributeMap::iterator attrIter = family.attributes.find(attributeName);
	if(attrIter == family.attributes.end()) {
	    throw(NameError(attributeName));
	    return false;
	    }
	
	//if you asked for zero particles, we're done
	if(numParticles == 0)
		return true;
	TypedArray& array = attrIter->second;
	array.release();
	bool value = true;
	std::string sFileName((attributeFiles[familyName +":"+ attributeName]));
	FILE* infile = fopen(sFileName.c_str(), "rb");
	if(infile) {
		XDR xdrs;
		xdrstdio_create(&xdrs, infile, XDR_DECODE);
		FieldHeader fh;
		if(!xdr_template(&xdrs, &fh)) {
		    std::string message(sFileName + ": XDR error");
		    throw(FileError(message.c_str()));
		    value = false;
		    }
		if(fh.magic != FieldHeader::MagicNumber) {
		    std::string message(sFileName + ": bad magic");
		    throw(FileError(message.c_str()));
		    value = false;
		    }
		if(fh.numParticles != family.count.totalNumParticles) {
		    std::string message(sFileName + ": bad particle number");
		    throw(FileError(message.c_str()));
		    value = false;
		    }
		if(array.dimensions != fh.dimensions) {
		    std::string message(sFileName + ": bad dimensions");
		    throw(FileError(message.c_str()));
		    value = false;
		    }
		if(array.code != fh.code) {
		    std::string message(sFileName + ": bad type");
		    throw(FileError(message.c_str()));
		    value = false;
		    }
		try {
		    readAttributes(&xdrs, array, family.count.numParticles,
				   family.count.startParticle);
		    }
		catch(XDRReadError &e) {
		    
		    std::string message(sFileName + ": bad readAttributes "
					+ e.getText());
		    throw(FileError(message.c_str()));
		    value = false;
		    }
		xdr_destroy(&xdrs);
	}
	else {
	    std::cerr << "loadAttribute: Failed to open file" << std::endl;
	    throw(FileError(sFileName.c_str()));
	    value = false;
	    }
	
	fclose(infile);
	return value;
}

bool SiXFormatWriter::save( Simulation* sim, const std::string& passedPath, int thisIndex) {
	const SiXFormatReader* six_sim = dynamic_cast<const SiXFormatReader *>(sim);
	if(!six_sim) {
		std::cerr << "Writing a SiX format that wasn't SiX format to begin with not yet supported!" << std::endl;
		return false;
	}
	std::string path;
	if(passedPath == "") path = six_sim->pathPrefix;
	else path = passedPath;
	std::ofstream xmlfile;
	if(thisIndex == 0){
	  std::cerr << "In save" << "\n";
	  std::cerr << "path: " << path << "\n";
	  std::cerr << "six_sim->pathPrefix: " << six_sim->pathPrefix << "\n";
	  struct stat buf;
	  if(stat(path.c_str(),&buf) < 0){
	    std::cerr << "making directory: " << path << "\n";
	    if(mkdir(path.c_str(),0755) < 0) throw(FileError("mkdir fails for "+path));
	  }
	  xmlfile.open((path + "/description.xml").c_str());
	  if(!xmlfile) throw(FileError("problem opening xml file.\n"));
	  xmlfile << "<?xml version=\"1.0\" encoding=\"iso-8859-1\"?>\n";
	  xmlfile << "<simulation>\n";
	}
	for(Simulation::const_iterator iter = sim->begin(); iter != sim->end(); ++iter) {
	  std::string achDirName=path +"/"+ iter->first;
	  if(thisIndex == 0){
	    xmlfile << "<family name=\"" << iter->first << "\">\n";
	    struct stat buf;
	    if(stat(achDirName.c_str(),&buf) < 0){
	      std::cerr << "making directory: " << achDirName << "\n"
			<< std::endl;
	      if(mkdir(achDirName.c_str(),0755) < 0) throw(FileError("mkdir fails for "+achDirName));
	    }
	  }
	  for(AttributeMap::const_iterator attrIter = iter->second.attributes.begin(); attrIter != iter->second.attributes.end(); ++attrIter) {
	    if(attrIter->second.data == NULL){   //make sure values are allocated
	      sim->loadAttribute(iter->first, attrIter->first, 
				 iter->second.count.numParticles, 
				 iter->second.count.startParticle);
	    }
	    FILE* outfile = fopen((path + "/" + iter->first + "/" + attrIter->first).c_str(), "wb");
	    if(!outfile) throw(FileError("unable to create "+achDirName+attrIter->first));
	    XDR xdrs;
	    xdrstdio_create(&xdrs, outfile, XDR_ENCODE);
	    writeAttributes(&xdrs, attrIter->second, 
			    iter->second.count.startParticle);
	    xdr_destroy(&xdrs);
	    fclose(outfile);
	    //write attribute xml tag
	    string filename;
	    std::map<std::string, std::string>::const_iterator mapIter = six_sim->attributeFiles.find(iter->first + ":" + attrIter->first);
	    if(mapIter == six_sim->attributeFiles.end()) { //didn't have an entry before
	      filename = path + "/" + iter->first + "/" + attrIter->first;
	      //attributeFiles[iter->first + ":" + attrIter->first] = filename;
	    } else
	      filename = mapIter->second;
	    string::size_type index = filename.rfind(iter->first + "/");
	    if(index == string::npos) {
	      std::cerr << "Couldn't find family directory in attribute filename" << std::endl;
	    } else {
	      filename = filename.substr(index);
	      if(thisIndex == 0) xmlfile << "\t\t<attribute name=\"" << attrIter->first << "\" link=\"" << filename << "\"/>\n";
	    }
	  }
	  if(thisIndex == 0) xmlfile << "\t</family>\n";
	}
	if(thisIndex == 0) {
	  xmlfile << "</simulation>\n";
	  xmlfile.close();
	}
	return true;
}

    // Exceptions: should go in separate file
    //#include <iostream>

std::ostream & operator <<(std::ostream &os, SimulationHandlingException &e) {
   os << e.getText();
   return os;
}

SimulationHandlingException::SimulationHandlingException() : d("") {
}

SimulationHandlingException::SimulationHandlingException(const string & desc)  : d(desc) {
}

string SimulationHandlingException::getText() const throw() {
  if(d=="")
    return "Unknown Simulation exception";
  else
    return d;
}

const char* SimulationHandlingException::what() const throw() {
  return getText().c_str();
}

FileError::FileError(string fn) : fileName(fn) {
}

string FileError::getText() const throw() {
  return "Error in file: " + fileName;
}

NameError::NameError(string fn) : badName(fn) {
}

string NameError::getText() const throw() {
  return "Error in attribute or family Name: " + badName;
}

} //close namespace SimulationHandling
