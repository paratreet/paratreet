/** @file Simulation.h
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created September 17, 2003
 @version 1.0
 */
 
#ifndef SIMULATION_H__iunfediunhtew8976378042iuhnrvge9o43
#define SIMULATION_H__iunfediunhtew8976378042iuhnrvge9o43

#include <string>
#include <map>
#include <list>
#include <vector>

#include <sys/types.h>

#include "tree_xdr.h"

namespace SimulationHandling {

using namespace TypeHandling;

struct ParticleCount {
	u_int64_t numParticles;
	u_int64_t startParticle;
	u_int64_t totalNumParticles;
	
	ParticleCount() : numParticles(0), startParticle(0), totalNumParticles(0) { }
};

typedef std::map<std::string, TypedArray> AttributeMap;
//typedef std::map<std::string, variant_attribute_type> AttributeMap;
typedef std::vector<u_int64_t> ParticleGroup;

/** A ParticleFamily represents a group of attributes that particles
 have in common.
 */
class ParticleFamily {
public:

	std::string familyName;
	ParticleCount count;
	AttributeMap attributes;
	std::map<std::string, ParticleGroup> groups;
	
	ParticleFamily(const std::string& name = "") : familyName(name) { }
	
	template <typename T>
	T* getAttribute(const std::string& attributeName, Type2Type<T> bob = Type2Type<T>()) {
		AttributeMap::iterator iter = attributes.find(attributeName);
		if(iter == attributes.end())
			return 0;
		return iter->second.getArray(Type2Type<T>());
	}
	
	void addAttribute(const std::string& attributeName, const TypeHandling::TypedArray& arr) {
	//template <typename T>
	//void addAttribute(const std::string& attributeName, const TypeHandling::ArrayWithLimits<T>& arr) {
		AttributeMap::iterator iter = attributes.find(attributeName);
		if(iter != attributes.end())
			iter->second.release();
			//apply_visitor(Releaser(), iter->second);
		if(arr.length == count.numParticles)
			attributes[attributeName] = arr;
	}
	/*
	void addAttribute(const std::string& attributeName, variant_attribute_type& attribute) {
		AttributeMap::iterator iter = attributes.find(attributeName);
		if(iter != attributes.end())
			apply_visitor(Releaser(), iter->second);
		attributes[attributeName] = attribute;
	}
	*/
	template <typename T>
	void addAttribute(const std::string& attributeName, T* data) {
		AttributeMap::iterator iter = attributes.find(attributeName);
		if(iter != attributes.end())
			iter->second.release();
			//apply_visitor(Releaser(), iter->second);
		TypeHandling::TypedArray& arr = attributes[attributeName];
		arr.dimensions = Type2Dimensions<T>::dimensions;
		arr.code = Type2Code<T>::code;
		arr.length = count.numParticles;
		arr.data = data;
		arr.calculateMinMax();
		
		//attributes[attributeName] = ArrayWithLimits<T>(data, count.numParticles);
	}
	
	bool releaseAttribute(const std::string& attributeName) {
		AttributeMap::iterator iter = attributes.find(attributeName);
		if(iter == attributes.end())
			return false;
		iter->second.release();
		//apply_visitor(Releaser(), iter->second);
		return true;
	}
	
	void release() {
		for(AttributeMap::iterator iter = attributes.begin(); iter != attributes.end(); ++iter)
			iter->second.release();
			//apply_visitor(Releaser(), iter->second);
	}
	
	template <typename T>
	ParticleGroup& createGroup(const std::string& groupName, const std::string& attributeName, T minValue, T maxValue) {
		ParticleGroup& g = groups[groupName];
		g.clear();
		if(T* values = getAttribute(attributeName, Type2Type<T>())) {
			for(u_int64_t i = 0; i < count.numParticles; ++i) {
				if(minValue <= values[i] && values[i] <= maxValue)
					g.push_back(i);
			}
		}
		return g;
	}

	template <typename T>
	ParticleGroup& createGroup(const std::string& groupName, const std::string& attributeName, Vector3D<T> minValue, Vector3D<T> maxValue) {
		ParticleGroup& g = groups[groupName];
		g.clear();
		if(Vector3D<T>* values = getAttribute(attributeName, Type2Type<Vector3D<T> >())) {
			OrientedBox<T> box(minValue, maxValue);
			for(u_int64_t i = 0; i < count.numParticles; ++i) {
				if(box.contains(values[i]))
					g.push_back(i);
			}
		}
		return g;
	}

	ParticleGroup& createGroup(const std::string& groupName, const std::string& attributeName, void* minData, void* maxData) {
		ParticleGroup& g = groups[groupName];
		g.clear();
		AttributeMap::iterator iter = attributes.find(attributeName);
		if(iter == attributes.end())
			return g;
		switch(iter->second.dimensions) {
			case 1:
				switch(iter->second.code) {
					case int8:
						return createGroup(groupName, attributeName, *static_cast<char *>(minData), *static_cast<char *>(maxData));
					case uint8:
						return createGroup(groupName, attributeName, *static_cast<unsigned char *>(minData), *static_cast<unsigned char *>(maxData));
					case int16:
						return createGroup(groupName, attributeName, *static_cast<short *>(minData), *static_cast<short *>(maxData));
					case uint16:
						return createGroup(groupName, attributeName, *static_cast<unsigned short *>(minData), *static_cast<unsigned short *>(maxData));
					case TypeHandling::int32:
						return createGroup(groupName, attributeName, *static_cast<int *>(minData), *static_cast<int *>(maxData));
					case TypeHandling::uint32:
						return createGroup(groupName, attributeName, *static_cast<unsigned int *>(minData), *static_cast<unsigned int *>(maxData));
					case TypeHandling::int64:
						return createGroup(groupName, attributeName, *static_cast<int64_t *>(minData), *static_cast<int64_t *>(maxData));
					case TypeHandling::uint64:
						return createGroup(groupName, attributeName, *static_cast<u_int64_t *>(minData), *static_cast<u_int64_t *>(maxData));
					case float32:
						return createGroup(groupName, attributeName, *static_cast<float *>(minData), *static_cast<float *>(maxData));
					case float64:
						return createGroup(groupName, attributeName, *static_cast<double *>(minData), *static_cast<double *>(maxData));
				}
				break;
			case 3:
				switch(iter->second.code) {
					case int8:
						return createGroup(groupName, attributeName, *static_cast<Vector3D<char> *>(minData), *static_cast<Vector3D<char> *>(maxData));
					case uint8:
						return createGroup(groupName, attributeName, *static_cast<Vector3D<unsigned char> *>(minData), *static_cast<Vector3D<unsigned char> *>(maxData));
					case int16:
						return createGroup(groupName, attributeName, *static_cast<Vector3D<short> *>(minData), *static_cast<Vector3D<short> *>(maxData));
					case uint16:
						return createGroup(groupName, attributeName, *static_cast<Vector3D<unsigned short> *>(minData), *static_cast<Vector3D<unsigned short> *>(maxData));
					case TypeHandling::int32:
						return createGroup(groupName, attributeName, *static_cast<Vector3D<int> *>(minData), *static_cast<Vector3D<int> *>(maxData));
					case TypeHandling::uint32:
						return createGroup(groupName, attributeName, *static_cast<Vector3D<unsigned int> *>(minData), *static_cast<Vector3D<unsigned int> *>(maxData));
					case TypeHandling::int64:
						return createGroup(groupName, attributeName, *static_cast<Vector3D<int64_t> *>(minData), *static_cast<Vector3D<int64_t> *>(maxData));
					case TypeHandling::uint64:
						return createGroup(groupName, attributeName, *static_cast<Vector3D<u_int64_t> *>(minData), *static_cast<Vector3D<u_int64_t> *>(maxData));
					case float32:
						return createGroup(groupName, attributeName, *static_cast<Vector3D<float> *>(minData), *static_cast<Vector3D<float> *>(maxData));
					case float64:
						return createGroup(groupName, attributeName, *static_cast<Vector3D<double> *>(minData), *static_cast<Vector3D<double> *>(maxData));
				}
				break;
		}
		return g;
	}
	
};

/** Represents a simulation as a collection of families.
 Simulation objects are created by subclasses, and then manipulated using
 this interface.  When a Simulation object has been created, it should
 hold all the families, the count for each family, and the type, dimensionality
 and min/max of each family's attributes.  The actual values should not
 be loaded until calls to loadAttribute().
 */
class Simulation : public std::map<std::string, ParticleFamily> {
public:
		
	std::string name;
	double time;
	
	ParticleFamily& getFamily(const std::string& familyName) {
		return operator[](familyName);
	}
	
	/// A sub-class implements the loading of values for a particular attribute
	virtual bool loadAttribute(const std::string& familyName, const std::string& attributeName, int64_t numParticles = 0, const u_int64_t startParticle = 0) = 0;

	void release() {
		for(iterator iter = begin(); iter != end(); ++iter)
			iter->second.release();
	}
	
	u_int64_t numParticles() const {
		u_int64_t numParticles = 0;
		for(const_iterator iter = begin(); iter != end(); ++iter)
			numParticles += iter->second.count.numParticles;
		return numParticles;
	}
	
	u_int64_t totalNumParticles() const {
		u_int64_t numParticles = 0;
		for(const_iterator iter = begin(); iter != end(); ++iter)
			numParticles += iter->second.count.totalNumParticles;
		return numParticles;
	}
	
	virtual ~Simulation() { }
	
};

class SimulationWriter {
public:
	/** Write the given simulation to disk in a format determined by the implementing sub-class.
	All non-null attribute arrays will be written to disk.  To prevent an attribute
	from being written do disk, explicitly release it before calling this method. */
	virtual bool save(Simulation*, const std::string&, int thisIndex) = 0;

	//virtual bool write(const Simulation*, const std::string&) = 0;

	virtual ~SimulationWriter() { }
};

class SimulationHandlingException : public std::exception {
  public:
    ~SimulationHandlingException() throw() { }; 
    SimulationHandlingException();
    SimulationHandlingException(const std::string & desc);
    virtual std::string getText() const throw();
    virtual const char* what() const throw();
  private:
    std::string d;
  };

class FileError : public SimulationHandlingException {
  public:
    ~FileError() throw() { }; 
    FileError(std::string fn);
    std::string getText() const throw();
    std::string fileName;
  };

class NameError : public SimulationHandlingException {
  public:
    ~NameError() throw() { }; 
    NameError(std::string fn);
    std::string getText() const throw();
    std::string badName;
  };

extern std::ostream & operator <<(std::ostream &os, SimulationHandlingException &e);

} //close namespace SimulationHandling

#endif //SIMULATION_H__iunfediunhtew8976378042iuhnrvge9o43
