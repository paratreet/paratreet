/** @file merge_tipsy_array.cpp
 Given a SiX format file and a Tipsy array file, merge the array
 values into the SiX file.
 */

#include <iostream>
#include <fstream>

#include "Simulation.h"
#include "SiXFormat.h"

template <typename IndexType, typename LengthType, typename ValueType>
void reorder_array(IndexType* indexArray, LengthType N, ValueType* valueArray) {
	IndexType index;
	for(LengthType i = 0; i < N; ++i) {
		index = i;
		while((index = indexArray[index]) < i);
		std::swap(valueArray[i], valueArray[index]);
	}
}

template <typename Iterator>
bool unique_values(Iterator begin, Iterator end) {
	if(begin == end)
		return false;
	Iterator next(begin);
	if(++next == end) //a single value is unique
		return true;
	typename std::iterator_traits<Iterator>::value_type value = *begin;
	while(++begin != end) {
		if(*begin != value)
			return true;
	}
	return false;
}

using namespace std;
using namespace SimulationHandling;

template <typename T>
bool mergeScalarAttribute(Simulation* sim, const string& familyName, const string& attributeName, u_int64_t& numPrevious, istream& infile, bool binaryFormat) {
	Simulation::iterator iter = sim->find(familyName);
	if(iter != sim->end()) { //there are some particles of this type
		ParticleFamily& family = iter->second;
		unsigned int* uids;
                try {
                    sim->loadAttribute(iter->first, "uid", family.count.totalNumParticles);
                    TypedArray& arr = family.attributes["uid"]; 
                    uids = arr.getArray(Type2Type<unsigned int>());
                    unsigned int minIndex = arr.getMinValue(Type2Type<unsigned int>());
                    for(u_int64_t i = 0; i < family.count.totalNumParticles; ++i)
			uids[i] -= minIndex;
                }
                catch(NameError &e) 
                    {
			cerr << "SiX format did not contain Tipsy-order uids!" << endl;
			cerr << "Assume Tipsy order." << endl;
                        uids = NULL;
                    }
		
		T* values = new T[family.count.totalNumParticles];
		if(binaryFormat) {
			for(unsigned int i = 0; i < family.count.totalNumParticles; ++i) {
				infile.read(reinterpret_cast<char *>(values + i), sizeof(T));
				if(!infile) {
					cerr << "Problem reading values from array file" << endl;
					return false;
				}
			}
		} else {
			for(unsigned int i = 0; i < family.count.totalNumParticles; ++i) {
				double tmp;
				infile >> tmp;	// Avoid format errors
				values[i] = (T) tmp;
				if(!infile) {
					cerr << "Problem reading values from array file at " << i << endl;
					return false;
				}
			}
		}
		
		
		if(unique_values(values, values + family.count.totalNumParticles)) {	
                    if(uids)
                        reorder_array(uids, family.count.totalNumParticles, values);
                    family.addAttribute(attributeName, values);
		} else
			delete[] values;
		
		numPrevious += family.count.totalNumParticles;	
		family.releaseAttribute("uid");
	}
	
	return true;
}

template <typename T>
bool mergeVectorAttribute(Simulation* sim, const string& familyName, const string& attributeName, u_int64_t& numPrevious, istream& infile, bool binaryFormat) {
	Simulation::iterator iter = sim->find(familyName);
	if(iter != sim->end()) { //there are some particles of this type
		ParticleFamily& family = iter->second;
		unsigned int* uids;
                try {
                    sim->loadAttribute(iter->first, "uid", family.count.totalNumParticles);
                    TypedArray& arr = family.attributes["uid"]; 
                    uids = arr.getArray(Type2Type<unsigned int>());
                    unsigned int minIndex = arr.getMinValue(Type2Type<unsigned int>());
                    for(u_int64_t i = 0; i < family.count.totalNumParticles; ++i)
			uids[i] -= minIndex;
                }
                catch(NameError &e) 
                    {
			cerr << "SiX format did not contain Tipsy-order uids!" << endl;
			cerr << "Assume Tipsy order." << endl;
                        uids = NULL;
                    }
                
		Vector3D<T>* values = new Vector3D<T>[family.count.totalNumParticles];
		if(binaryFormat) {
			for(unsigned int i = 0; i < family.count.totalNumParticles; ++i) {
				infile.read(reinterpret_cast<char *>(&values[i].x), sizeof(T));
				if(!infile) {
					cerr << "Problem reading values from array file" << endl;
					return false;
				}
			}
			for(unsigned int i = 0; i < family.count.totalNumParticles; ++i) {
				infile.read(reinterpret_cast<char *>(&values[i].y), sizeof(T));
				if(!infile) {
					cerr << "Problem reading values from array file" << endl;
					return false;
				}
			}
			for(unsigned int i = 0; i < family.count.totalNumParticles; ++i) {
				infile.read(reinterpret_cast<char *>(&values[i].z), sizeof(T));
				if(!infile) {
					cerr << "Problem reading values from array file" << endl;
					return false;
				}
			}
		} else {
			for(unsigned int i = 0; i < family.count.totalNumParticles; ++i) {
				infile >> values[i].x;
				if(!infile) {
					cerr << "Problem reading values from array file" << endl;
					return false;
				}
			}
			for(unsigned int i = 0; i < family.count.totalNumParticles; ++i) {
				infile >> values[i].y;
				if(!infile) {
					cerr << "Problem reading values from array file" << endl;
					return false;
				}
			}
			for(unsigned int i = 0; i < family.count.totalNumParticles; ++i) {
				infile >> values[i].z;
				if(!infile) {
					cerr << "Problem reading values from array file" << endl;
					return false;
				}
			}
		}
		
		if(unique_values(values, values + family.count.totalNumParticles)) {	
			reorder_array(uids, family.count.totalNumParticles, values);
			family.addAttribute(attributeName, values);
		} else
			delete[] values;
		
		numPrevious += family.count.totalNumParticles;	
		family.releaseAttribute("uid");
	}
	
	return true;
}

template <typename T>
bool mergeAttribute_dimensions(Simulation* sim, const string& familyName, const string& attributeName, TypeInformation attributeType, u_int64_t& numPrevious, istream& infile, bool binaryFormat) {
	if(attributeType.dimensions == 1)
		return mergeScalarAttribute<T>(sim, familyName, attributeName, numPrevious, infile, binaryFormat);
	else if(attributeType.dimensions == 3)
		return mergeVectorAttribute<T>(sim, familyName, attributeName, numPrevious, infile, binaryFormat);
	else
		return false;
}

bool mergeAttribute(Simulation* sim, const string& familyName, const string& attributeName, TypeInformation attributeType, u_int64_t& numPrevious, istream& infile, bool binaryFormat) {
	switch(attributeType.code) {
		case int8: return mergeAttribute_dimensions<char>(sim, familyName, attributeName, attributeType, numPrevious, infile, binaryFormat);
		case uint8: return mergeAttribute_dimensions<unsigned char>(sim, familyName, attributeName, attributeType, numPrevious, infile, binaryFormat);
		case int16: return mergeAttribute_dimensions<short>(sim, familyName, attributeName, attributeType, numPrevious, infile, binaryFormat);
		case uint16: return mergeAttribute_dimensions<unsigned short>(sim, familyName, attributeName, attributeType, numPrevious, infile, binaryFormat);
		case int32: return mergeAttribute_dimensions<int>(sim, familyName, attributeName, attributeType, numPrevious, infile, binaryFormat);
		case uint32: return mergeAttribute_dimensions<unsigned int>(sim, familyName, attributeName, attributeType, numPrevious, infile, binaryFormat);
		case int64: return mergeAttribute_dimensions<int64_t>(sim, familyName, attributeName, attributeType, numPrevious, infile, binaryFormat);
		case uint64: return mergeAttribute_dimensions<u_int64_t>(sim, familyName, attributeName, attributeType, numPrevious, infile, binaryFormat);
		case float32: return mergeAttribute_dimensions<float>(sim, familyName, attributeName, attributeType, numPrevious, infile, binaryFormat);
		case float64: return mergeAttribute_dimensions<double>(sim, familyName, attributeName, attributeType, numPrevious, infile, binaryFormat);
		default: return false;
	}
}

int main(int argc, char** argv) {
	
	if(argc < 6) {
		cerr << "Usage: merge_tipsy_array SiXFile arrayfile (vector | scalar) typecode attribute_name" << endl;
		return 1;
	}
	
	Simulation* sim = new SiXFormatReader(argv[1]);
	if(sim->size() == 0) {
		cerr << "Problems opening SiX format file" << endl;
		return 2;
	}
	
	ifstream infile(argv[2]);
	if(!infile) {
		cerr << "Problems reading Tipsy array file" << endl;
		return 2;
	}
	
	int numParticles;
	bool binaryFormat = false;
	//read number of particles from array file
	infile >> numParticles;
	if(!infile) {
		//reopen file, try binary format
		infile.close();
		infile.clear();
		infile.open(argv[2]);
		infile.read(reinterpret_cast<char *>(&numParticles), sizeof(int));
		if(!infile) {
			cerr << "Couldn't read number of particles from array file" << endl;
			return 2;
		} else {
			cerr << "I think this file is binary format" << endl;
			binaryFormat = true;
		}
	}
	
	if(numParticles != sim->totalNumParticles()) {
		cerr << "Number of particles in array doesn't match SiX format file" << endl;
		return 2;
	}
	
	string s;
	
	TypeInformation attributeType;
	
	s = argv[3];
	if(s == "vector")
		attributeType.dimensions = 3;
	else
		attributeType.dimensions = 1;
	
	s = argv[4];
	if(s == "int8")
		attributeType.code = int8;
	else if(s == "uint8")
		attributeType.code = uint8;
	else if(s == "int16")
		attributeType.code = int16;
	else if(s == "uint16")
		attributeType.code = uint16;
	else if(s == "int32")
		attributeType.code = int32;
	else if(s == "uint32")
		attributeType.code = uint32;
	else if(s == "int64")
		attributeType.code = int64;
	else if(s == "uint64")
		attributeType.code = uint64;
	else if(s == "float32")
		attributeType.code = float32;
	else if(s == "float64")
		attributeType.code = float64;
	else
		attributeType.code = float32;
	
	string attributeName = argv[5];
	
	u_int64_t numPrevious = 0;
	
	//merge the attribute into the Tipsy families
	if(!mergeAttribute(sim, "gas", attributeName, attributeType, numPrevious, infile, binaryFormat))
		return 3;
	if(!mergeAttribute(sim, "dark", attributeName, attributeType, numPrevious, infile, binaryFormat))
		return 3;
	if(!mergeAttribute(sim, "star", attributeName, attributeType, numPrevious, infile, binaryFormat))
		return 3;
	
	cerr << "Attribute merged from array file, writing to disk ..." << endl;
	
	SimulationWriter* writer = new SiXFormatWriter;
	writer->save(sim, "", 0);
	
	delete writer;
	delete sim;
	
	cerr << "Done." << endl;
}
