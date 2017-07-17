/** @file Group.h
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created June 28, 2004
 @version 1.0
 */
 
#ifndef GROUP_H__7h237h327yh4th7y378w37aw7ho4wvta8
#define GROUP_H__7h237h327yh4th7y378w37aw7ho4wvta8

#include <string>
#include <vector>
#include <set>

#include <boost/shared_ptr.hpp>
#include <boost/iterator/iterator_facade.hpp>

#include "Simulation.h"

namespace SimulationHandling {

using namespace TypeHandling;

/** The base class for group iterators.
 Pointers to instances of this class or subclasses form the implementation
 of the concrete \c GroupIterator class.
 Individual types of group will subclass this and implement their
 own increment, equal and dereference methods.
 */
class GroupIteratorInstance {
	u_int64_t index;
public:
	
	GroupIteratorInstance() : index(0) { }
	GroupIteratorInstance(u_int64_t start) : index(start) { }
	virtual ~GroupIteratorInstance() { }
	
	/// For criterion-based groups, this method should move to the next member of the parent group that satisfies the criterion, or the end
	virtual void increment() {
		++index;
	}
	
	/// For non-trivial groups, check both array index and criterion data
	virtual bool equal(GroupIteratorInstance* const& other) const {
		return index == other->index;
	}
	
	/// This should return the array index of the current group member
	virtual u_int64_t dereference() const {
		return index;
	}
	
	/// Return the number of particles between here and there
	virtual long distance_to(GroupIteratorInstance* const& other) {
		return other->index-index;
	}
};

/** A concrete class that uses the pimpl idiom to handle many types of
 group.  Instances of \c Group and its subclasses will provide
 objects of this type holding the appropriate \c impl pointer
 to the actual iterator object.
 */
class GroupIterator : public boost::iterator_facade<GroupIterator, u_int64_t, boost::forward_traversal_tag, u_int64_t> {
public:
	// Create an uninitialized iterator--any use of this will immediately crash.
	GroupIterator() { }
	
	// Typical constructor.
	explicit GroupIterator(boost::shared_ptr<GroupIteratorInstance> impl_) : impl(impl_) { }
	
private:
	friend class boost::iterator_core_access;

	void increment() { impl->increment(); }
	
	bool equal(GroupIterator const& other) const {
		return impl->equal(other.impl.get());
	}
	
	u_int64_t dereference() const { return impl->dereference(); }
	
	long distance_to(GroupIterator const& other) const {
		return impl->distance_to(other.impl.get());
	}

	boost::shared_ptr<GroupIteratorInstance> impl;
};

/** A \c Group object has virtual functions that provide iterators for
 the particular kind of group, and lists the families it is valid for.
 The big picture is: keep a collection of pointers to \c Group objects.
 When processing a group, use the virtual member functions to create
 concrete \c GroupIterator objects.  These iterators use the pimpl
 idiom to implement the appropriate behavior for the group they work for.
 */
class Group {
	//disable copying
	Group(Group const&);
	Group& operator=(Group const&);
	
public:
	
	Group(boost::shared_ptr<Group> const& parent = boost::shared_ptr<Group>()) : parentGroup(parent) { }
	virtual ~Group() { }
		
	typedef std::set<std::string> GroupFamilies;
	GroupFamilies families;
	
	virtual GroupIterator make_begin_iterator(std::string const& familyName) = 0;
	virtual GroupIterator make_end_iterator(std::string const& familyName) = 0;

protected:
	boost::shared_ptr<Group> parentGroup;
};

/// The "All" group iterator is the default behavior, provide a typedef.
typedef GroupIteratorInstance AllGroupIterator;

/// The All group doesn't have a parent
class AllGroup : public Group {
	Simulation const& sim;
public:
		
	AllGroup(Simulation const& s) : sim(s) {
		for(Simulation::const_iterator iter = sim.begin(); iter != sim.end(); ++iter)
			families.insert(iter->first);
	}
	
	GroupIterator make_begin_iterator(std::string const& familyName) {
		boost::shared_ptr<AllGroupIterator> p(new AllGroupIterator(0));
		return GroupIterator(p);
	}

	GroupIterator make_end_iterator(std::string const& familyName) {
		GroupFamilies::const_iterator famIter = families.find(familyName);
		if(famIter == families.end())
			return make_begin_iterator(familyName);
		Simulation::const_iterator simIter = sim.find(familyName);
		boost::shared_ptr<AllGroupIterator> p(new AllGroupIterator(simIter->second.count.numParticles));
		return GroupIterator(p);
	}
};

template <typename T>
class AttributeRangeIterator : public GroupIteratorInstance {
	template <typename>
	friend class AttributeRangeGroup;
	
	//u_int64_t index;
	//parent group idiom: Instead of index, use iterator into parent group
	GroupIterator parentIter;
	T minValue;
	T maxValue;
	T const* array; //bare pointer is okay here, default copy constructor will do what we want
	u_int64_t length;
	
public:
	
	AttributeRangeIterator(GroupIterator parentBegin, T minValue_, T maxValue_, T const* array_, u_int64_t length_) : parentIter(parentBegin), minValue(minValue_), maxValue(maxValue_), array(array_), length(length_) {
		while(*parentIter < length && (array[*parentIter] < minValue || maxValue < array[*parentIter]))
			++parentIter;
	}

	void increment() {
		//parent group idiom: increment parentIter instead of index
		if(*parentIter < length)
			for(++parentIter; *parentIter < length && (array[*parentIter] < minValue || maxValue < array[*parentIter]); ++parentIter);
	}
	
	bool equal(GroupIteratorInstance* const& other) const {
		if(AttributeRangeIterator* const o = dynamic_cast<AttributeRangeIterator* const>(other))
			return minValue == o->minValue && maxValue == o->maxValue && array == o->array && parentIter == o->parentIter;
		else
			return false;
	}
	
	u_int64_t dereference() const {
		//parent group idiom: instead of index value, dereference iterator
		return *parentIter;
	}
	
	/// Return the number of particles between here and there
	long distance_to(GroupIteratorInstance* const& other) {
		long distance=0; while (!equal(other)) { distance++; increment(); }
		return distance;
	}
};

template <typename T>
class AttributeRangeGroup : public Group {
	Simulation const& sim;
	std::string attributeName;
	T minValue;
	T maxValue;
public:
		
	AttributeRangeGroup(Simulation const& s, boost::shared_ptr<Group> const& parent, std::string const& attributeName_, T minValue_, T maxValue_) : Group(parent), sim(s), attributeName(attributeName_), minValue(minValue_), maxValue(maxValue_) {
		for(Simulation::const_iterator simIter = sim.begin(); simIter != sim.end(); ++simIter) {
			AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
			if(attrIter != simIter->second.attributes.end())
				families.insert(simIter->first);
		}
	}
	
	GroupIterator make_begin_iterator(std::string const& familyName) {
		GroupFamilies::const_iterator famIter = families.find(familyName);
		if(famIter == families.end())
			return make_end_iterator(familyName);
		Simulation::const_iterator simIter = sim.find(familyName);
		TypedArray const& array = simIter->second.attributes.find(attributeName)->second;
		//parent group idiom: start with parent group's begin iterator
		GroupIterator parentBegin = parentGroup->make_begin_iterator(familyName);
		boost::shared_ptr<AttributeRangeIterator<T> > p(new AttributeRangeIterator<T>(parentBegin, minValue, maxValue, array.getArray(Type2Type<T>()), array.length));
		return GroupIterator(p);
	}

	GroupIterator make_end_iterator(std::string const& familyName) {
		//parent group idiom: parent group can handle end iterator
		return parentGroup->make_end_iterator(familyName);
	}
};

inline
boost::shared_ptr<Group> make_AttributeRangeGroup(Simulation const& sim, boost::shared_ptr<Group> const& parent, std::string const& attributeName, double minValue, double maxValue) {
	boost::shared_ptr<Group> p;
	for(Simulation::const_iterator simIter = sim.begin(); simIter != sim.end(); ++simIter) {
		AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
		if(attrIter != simIter->second.attributes.end()) {
			//found a family with the attribute
			TypedArray const& arr = attrIter->second;
			if(arr.dimensions == 1) {
				switch(arr.code) {
					case int8:
						p.reset(new AttributeRangeGroup<Code2Type<int8>::type>(sim, parent, attributeName, static_cast<Code2Type<int8>::type>(minValue), static_cast<Code2Type<int8>::type>(maxValue)));
						break;
					case uint8:
						p.reset(new AttributeRangeGroup<Code2Type<uint8>::type>(sim, parent, attributeName, static_cast<Code2Type<uint8>::type>(minValue), static_cast<Code2Type<uint8>::type>(maxValue)));
						break;
					case int16:
						p.reset(new AttributeRangeGroup<Code2Type<int16>::type>(sim, parent, attributeName, static_cast<Code2Type<int16>::type>(minValue), static_cast<Code2Type<int16>::type>(maxValue)));
						break;
					case uint16:
						p.reset(new AttributeRangeGroup<Code2Type<uint16>::type>(sim, parent, attributeName, static_cast<Code2Type<uint16>::type>(minValue), static_cast<Code2Type<uint16>::type>(maxValue)));
						break;
					case TypeHandling::int32:
						p.reset(new AttributeRangeGroup<Code2Type<TypeHandling::int32>::type>(sim, parent, attributeName, static_cast<Code2Type<TypeHandling::int32>::type>(minValue), static_cast<Code2Type<TypeHandling::int32>::type>(maxValue)));
						break;
					case TypeHandling::uint32:
						p.reset(new AttributeRangeGroup<Code2Type<TypeHandling::uint32>::type>(sim, parent, attributeName, static_cast<Code2Type<TypeHandling::uint32>::type>(minValue), static_cast<Code2Type<TypeHandling::uint32>::type>(maxValue)));
						break;
					case TypeHandling::int64:
						p.reset(new AttributeRangeGroup<Code2Type<TypeHandling::int64>::type>(sim, parent, attributeName, static_cast<Code2Type<TypeHandling::int64>::type>(minValue), static_cast<Code2Type<TypeHandling::int64>::type>(maxValue)));
						break;
					case TypeHandling::uint64:
						p.reset(new AttributeRangeGroup<Code2Type<TypeHandling::uint64>::type>(sim, parent, attributeName, static_cast<Code2Type<TypeHandling::uint64>::type>(minValue), static_cast<Code2Type<TypeHandling::uint64>::type>(maxValue)));
						break;
					case float32:
						p.reset(new AttributeRangeGroup<Code2Type<float32>::type>(sim, parent, attributeName, static_cast<Code2Type<float32>::type>(minValue), static_cast<Code2Type<float32>::type>(maxValue)));
						break;
					case float64:
						p.reset(new AttributeRangeGroup<Code2Type<float64>::type>(sim, parent, attributeName, static_cast<Code2Type<float64>::type>(minValue), static_cast<Code2Type<float64>::type>(maxValue)));
						break;
				}
			}
			break;
		}
	}
	return p;
}

/*
class NoCriterionGroupIterator : public GroupIteratorInstance {
	GroupIterator parentIter;	
public:
	
	NoCriterionGroupIterator(GroupIterator parentBegin) : parentIter(parentBegin) { }

	void increment() {
		if(*parentIter < length)
			++parentIter;
	}
	
	bool equal(GroupIteratorInstance* const& other) const {
		if(NoCriterionGroupIterator* const o = dynamic_cast<NoCriterionGroupIterator* const>(other))
			return parentIter == o->parentIter;
		else
			return false;
	}
	
	u_int64_t dereference() const {
		return *parentIter;
	}
};
*/
		
template <typename T>
class SphericalIterator : public GroupIteratorInstance {
	template <typename>
	friend class SphericalGroup;
	
	GroupIterator parentIter;
	Vector3D<T> centerVector;
	T radiusValue;
	Vector3D<T> const* array; //bare pointer is okay here, default copy constructor will do what we want
	u_int64_t length;
	
public:
	
	SphericalIterator(GroupIterator parentBegin, Vector3D<T> centerVector_, T radiusValue_, Vector3D<T> const* array_, u_int64_t length_) : parentIter(parentBegin), centerVector(centerVector_), radiusValue(radiusValue_), array(array_), length(length_) {
		while(*parentIter < length && (array[*parentIter]-centerVector).length() > radiusValue)
		    ++parentIter;
	    }

	void increment() {
	    if(*parentIter < length)
		for(++parentIter; *parentIter < length && ((array[*parentIter]-centerVector).length() > radiusValue); ++parentIter);
	}
	
	bool equal(GroupIteratorInstance* const& other) const {
		if(SphericalIterator* const o = dynamic_cast<SphericalIterator* const>(other))
			return centerVector == o->centerVector && radiusValue == o->radiusValue && array == o->array && parentIter == o->parentIter;
		else
			return false;
	}
	
	u_int64_t dereference() const {
		return *parentIter;
	}
	
	/// Return the number of particles between here and there
	long distance_to(GroupIteratorInstance* const& other) {
		long distance=0; while (!equal(other)) { distance++; increment(); }
		return distance;
	}
};

template <typename T>
class SphericalGroup : public Group {
	Simulation const& sim;
	std::string attributeName;
	Vector3D<T> centerVector;
	T radiusValue;
public:
		
	SphericalGroup(Simulation const& s, boost::shared_ptr<Group> const& parent, std::string const& attributeName_, Vector3D<T> centerVector_, T radiusValue_) :
	    Group(parent), sim(s), attributeName(attributeName_), centerVector(centerVector_), radiusValue(radiusValue_) {
		for(Simulation::const_iterator simIter = sim.begin(); simIter != sim.end(); ++simIter) {
			AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
			if(attrIter != simIter->second.attributes.end())
				families.insert(simIter->first);
		}
	}
	
	GroupIterator make_begin_iterator(std::string const& familyName) {
		GroupFamilies::const_iterator famIter = families.find(familyName);
		if(famIter == families.end()) {
		    return make_end_iterator(familyName);
		    }
		Simulation::const_iterator simIter = sim.find(familyName);
		AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
		TypedArray const& array = attrIter->second;
		GroupIterator parentBegin = parentGroup->make_begin_iterator(familyName);
		boost::shared_ptr<SphericalIterator<T> > p(new SphericalIterator<T>(parentBegin, centerVector, radiusValue, array.getArray(Type2Type<Vector3D<T> >()), array.length));
		return GroupIterator(p);
	}

	GroupIterator make_end_iterator(std::string const& familyName) {
		return parentGroup->make_end_iterator(familyName);
	}
};

inline
boost::shared_ptr<Group> make_SphericalGroup(Simulation const& sim, boost::shared_ptr<Group> const& parent, std::string const& attributeName, Vector3D<double> centerVector, double radiusValue) {
	boost::shared_ptr<Group> p;
	for(Simulation::const_iterator simIter = sim.begin(); simIter != sim.end(); ++simIter) {
		AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
		if(attrIter != simIter->second.attributes.end()) {
			//found a family with the attribute
			TypedArray const& arr = attrIter->second;
			if(arr.dimensions == 3) {
				switch(arr.code) {
					case int8:
						p.reset(new SphericalGroup<Code2Type<int8>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<int8>::type> >(centerVector), static_cast<Code2Type<int8>::type>(radiusValue)));
						break;
					case uint8:
						p.reset(new SphericalGroup<Code2Type<uint8>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<uint8>::type> >(centerVector), static_cast<Code2Type<uint8>::type>(radiusValue)));
						break;
					case int16:
						p.reset(new SphericalGroup<Code2Type<int16>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<int16>::type> >(centerVector), static_cast<Code2Type<int16>::type>(radiusValue)));
						break;
					case uint16:
						p.reset(new SphericalGroup<Code2Type<uint16>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<uint16>::type> >(centerVector), static_cast<Code2Type<uint16>::type>(radiusValue)));
						break;
					case TypeHandling::int32:
						p.reset(new SphericalGroup<Code2Type<TypeHandling::int32>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::int32>::type> >(centerVector), static_cast<Code2Type<TypeHandling::int32>::type>(radiusValue)));
						break;
					case TypeHandling::uint32:
						p.reset(new SphericalGroup<Code2Type<TypeHandling::uint32>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::uint32>::type> >(centerVector), static_cast<Code2Type<TypeHandling::uint32>::type>(radiusValue)));
						break;
					case TypeHandling::int64:
						p.reset(new SphericalGroup<Code2Type<TypeHandling::int64>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::int64>::type> >(centerVector), static_cast<Code2Type<TypeHandling::int64>::type>(radiusValue)));
						break;
					case TypeHandling::uint64:
						p.reset(new SphericalGroup<Code2Type<TypeHandling::uint64>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::uint64>::type> >(centerVector), static_cast<Code2Type<TypeHandling::uint64>::type>(radiusValue)));
						break;
					case float32:
						p.reset(new SphericalGroup<Code2Type<float32>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<float32>::type> >(centerVector), static_cast<Code2Type<float32>::type>(radiusValue)));
						break;
					case float64:
						p.reset(new SphericalGroup<Code2Type<float64>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<float64>::type> >(centerVector), static_cast<Code2Type<float64>::type>(radiusValue)));
						break;
				}
			}
			break;
		}
	}
	return p;
}

template <typename T>
class ShellIterator : public GroupIteratorInstance {
	template <typename>
	friend class ShellGroup;
	
	GroupIterator parentIter;
	Vector3D<T> centerVector;
	T radiusMin;
	T radiusMax;
	Vector3D<T> const* array; //bare pointer is okay here, default copy constructor will do what we want
	u_int64_t length;
	
public:
	
	ShellIterator(GroupIterator parentBegin, Vector3D<T> centerVector_, T radiusMin_, T radiusMax_, Vector3D<T> const* array_, u_int64_t length_) : parentIter(parentBegin), centerVector(centerVector_), radiusMin(radiusMin_) ,radiusMax(radiusMax_), array(array_), length(length_) {
	    while(*parentIter < length
		  && ((array[*parentIter]-centerVector).length() >= radiusMax
		      || (array[*parentIter]-centerVector).length() < radiusMin))
		++parentIter;
	    }

	void increment() {
	    if(*parentIter < length)
		for(++parentIter; *parentIter < length
			&& ((array[*parentIter]-centerVector).length() >= radiusMax
			    || (array[*parentIter]-centerVector).length() < radiusMin);
		    ++parentIter);
	}
	
	bool equal(GroupIteratorInstance* const& other) const {
		if(ShellIterator* const o = dynamic_cast<ShellIterator* const>(other))
			return centerVector == o->centerVector
			    && radiusMax == o->radiusMax
			    && radiusMin == o->radiusMin
			    && array == o->array
			    && parentIter == o->parentIter;
		else
			return false;
	}
	
	u_int64_t dereference() const {
		return *parentIter;
	}
	
	/// Return the number of particles between here and there
	long distance_to(GroupIteratorInstance* const& other) {
		long distance=0; while (!equal(other)) { distance++; increment(); }
		return distance;
	}
};

template <typename T>
class ShellGroup : public Group {
	Simulation const& sim;
	std::string attributeName;
	Vector3D<T> centerVector;
	T radiusMin;
	T radiusMax;
public:
		
	ShellGroup(Simulation const& s, boost::shared_ptr<Group> const& parent, std::string const& attributeName_, Vector3D<T> centerVector_, T radiusMin_, T radiusMax_) :
	    Group(parent), sim(s), attributeName(attributeName_), centerVector(centerVector_), radiusMin(radiusMin_), radiusMax(radiusMax_) {
		for(Simulation::const_iterator simIter = sim.begin(); simIter != sim.end(); ++simIter) {
			AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
			if(attrIter != simIter->second.attributes.end())
				families.insert(simIter->first);
		}
	}
	
	GroupIterator make_begin_iterator(std::string const& familyName) {
		GroupFamilies::const_iterator famIter = families.find(familyName);
		if(famIter == families.end()) {
		    return make_end_iterator(familyName);
		    }
		Simulation::const_iterator simIter = sim.find(familyName);
		AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
		TypedArray const& array = attrIter->second;
		GroupIterator parentBegin = parentGroup->make_begin_iterator(familyName);
		boost::shared_ptr<ShellIterator<T> > p(new ShellIterator<T>(parentBegin, centerVector, radiusMin, radiusMax, array.getArray(Type2Type<Vector3D<T> >()), array.length));
		return GroupIterator(p);
	}

	GroupIterator make_end_iterator(std::string const& familyName) {
		return parentGroup->make_end_iterator(familyName);
	}
};

inline
boost::shared_ptr<Group> make_ShellGroup(Simulation const& sim, boost::shared_ptr<Group> const& parent, std::string const& attributeName, Vector3D<double> centerVector, double radiusMin, double radiusMax) {
	boost::shared_ptr<Group> p;
	for(Simulation::const_iterator simIter = sim.begin(); simIter != sim.end(); ++simIter) {
		AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
		if(attrIter != simIter->second.attributes.end()) {
			//found a family with the attribute
			TypedArray const& arr = attrIter->second;
			if(arr.dimensions == 3) {
				switch(arr.code) {
					case int8:
						p.reset(new ShellGroup<Code2Type<int8>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<int8>::type> >(centerVector), static_cast<Code2Type<int8>::type>(radiusMin), static_cast<Code2Type<int8>::type>(radiusMax)));
						break;
					case uint8:
						p.reset(new ShellGroup<Code2Type<uint8>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<uint8>::type> >(centerVector), static_cast<Code2Type<uint8>::type>(radiusMin), static_cast<Code2Type<uint8>::type>(radiusMax)));
						break;
					case int16:
						p.reset(new ShellGroup<Code2Type<int16>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<int16>::type> >(centerVector), static_cast<Code2Type<int16>::type>(radiusMin), static_cast<Code2Type<int16>::type>(radiusMax)));
						break;
					case uint16:
						p.reset(new ShellGroup<Code2Type<uint16>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<uint16>::type> >(centerVector), static_cast<Code2Type<uint16>::type>(radiusMin), static_cast<Code2Type<uint16>::type>(radiusMax)));
						break;
					case TypeHandling::int32:
						p.reset(new ShellGroup<Code2Type<TypeHandling::int32>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::int32>::type> >(centerVector), static_cast<Code2Type<TypeHandling::int32>::type>(radiusMin), static_cast<Code2Type<TypeHandling::int32>::type>(radiusMax)));
						break;
					case TypeHandling::uint32:
						p.reset(new ShellGroup<Code2Type<TypeHandling::uint32>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::uint32>::type> >(centerVector), static_cast<Code2Type<TypeHandling::uint32>::type>(radiusMin), static_cast<Code2Type<TypeHandling::uint32>::type>(radiusMax)));
						break;
					case TypeHandling::int64:
						p.reset(new ShellGroup<Code2Type<TypeHandling::int64>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::int64>::type> >(centerVector), static_cast<Code2Type<TypeHandling::int64>::type>(radiusMin), static_cast<Code2Type<TypeHandling::int64>::type>(radiusMax)));
						break;
					case TypeHandling::uint64:
						p.reset(new ShellGroup<Code2Type<TypeHandling::uint64>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::uint64>::type> >(centerVector), static_cast<Code2Type<TypeHandling::uint64>::type>(radiusMin), static_cast<Code2Type<TypeHandling::uint64>::type>(radiusMax)));
						break;
					case float32:
						p.reset(new ShellGroup<Code2Type<float32>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<float32>::type> >(centerVector), static_cast<Code2Type<float32>::type>(radiusMin), static_cast<Code2Type<float32>::type>(radiusMax)));
						break;
					case float64:
						p.reset(new ShellGroup<Code2Type<float64>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<float64>::type> >(centerVector), static_cast<Code2Type<float64>::type>(radiusMin), static_cast<Code2Type<float64>::type>(radiusMax)));
						break;
				}
			}
			break;
		}
	}
	return p;
}

template <typename T>
class BoxIterator : public GroupIteratorInstance {
	template <typename>
	friend class BoxGroup;
	
	GroupIterator parentIter;
	Vector3D<T> cornerVector;
	Vector3D<T> edge1Vector;
	Vector3D<T> edge2Vector;
	Vector3D<T> edge3Vector;
	Vector3D<T> const* array; //bare pointer is okay here, default copy constructor will do what we want
	u_int64_t length;
	
public:
	
	BoxIterator(GroupIterator parentBegin, Vector3D<T> cornerVector_, Vector3D<T> edge1Vector_, Vector3D<T> edge2Vector_, Vector3D<T> edge3Vector_, Vector3D<T> const* array_, u_int64_t length_) : parentIter(parentBegin), cornerVector(cornerVector_), edge1Vector(edge1Vector_), edge2Vector(edge2Vector_), edge3Vector(edge3Vector_), array(array_), length(length_) {
	    while(*parentIter < length) {
		// signed type is needed in order for subtraction and
		// comparision to make sense
		Vector3D<double> vec = array[*parentIter]-cornerVector;
		if((dot(vec,edge1Vector) < edge1Vector.lengthSquared()) &&
		   (dot(vec,edge1Vector) >= 0)  &&
		   (dot(vec,edge2Vector) < edge2Vector.lengthSquared()) &&
		   (dot(vec,edge2Vector) >= 0) &&
		   (dot(vec,edge3Vector) < edge3Vector.lengthSquared()) &&
		   (dot(vec,edge3Vector) >= 0))
		    break;
		++parentIter;
		}
	    }

	void increment() {
	    if(*parentIter < length) {
		for(++parentIter; *parentIter < length; ++parentIter) {
		    // signed type is needed in order for subtraction and
		    // comparision to make sense
		    Vector3D<double> vec = array[*parentIter]-cornerVector;
		    if((dot(vec,edge1Vector) < edge1Vector.lengthSquared()) &&
		       (dot(vec,edge1Vector) >= 0)  &&
		       (dot(vec,edge2Vector) < edge2Vector.lengthSquared()) &&
		       (dot(vec,edge2Vector) >= 0) &&
		       (dot(vec,edge3Vector) < edge3Vector.lengthSquared()) &&
		       (dot(vec,edge3Vector) >= 0))
			break;
		    }
		}
	}
	
	bool equal(GroupIteratorInstance* const& other) const {
		if(BoxIterator* const o = dynamic_cast<BoxIterator* const>(other))
			return cornerVector == o->cornerVector && edge1Vector == o->edge1Vector && edge2Vector == o->edge2Vector && edge3Vector == o->edge3Vector && array == o->array && parentIter == o->parentIter;
		else
			return false;
	}
	
	u_int64_t dereference() const {
		return *parentIter;
	}
	
	/// Return the number of particles between here and there
	long distance_to(GroupIteratorInstance* const& other) {
		long distance=0; while (!equal(other)) { distance++; increment(); }
		return distance;
	}
};

template <typename T>
class BoxGroup : public Group {
	Simulation const& sim;
	std::string attributeName;
	Vector3D<T> cornerVector;
	Vector3D<T> edge1Vector;
	Vector3D<T> edge2Vector;
	Vector3D<T> edge3Vector;
public:
		
	BoxGroup(Simulation const& s, boost::shared_ptr<Group> const& parent, std::string const& attributeName_, Vector3D<T> cornerVector_, Vector3D<T> edge1Vector_, Vector3D<T> edge2Vector_, Vector3D<T> edge3Vector_) : Group(parent), sim(s), attributeName(attributeName_), cornerVector(cornerVector_), edge1Vector(edge1Vector_), edge2Vector(edge2Vector_), edge3Vector(edge3Vector_) {
		for(Simulation::const_iterator simIter = sim.begin(); simIter != sim.end(); ++simIter) {
			AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
			if(attrIter != simIter->second.attributes.end())
				families.insert(simIter->first);
		}
	}
	
	GroupIterator make_begin_iterator(std::string const& familyName) {
		GroupFamilies::const_iterator famIter = families.find(familyName);
		if(famIter == families.end()) {
		    return make_end_iterator(familyName);
		    }
		Simulation::const_iterator simIter = sim.find(familyName);
		AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
		TypedArray const& array = attrIter->second;
		GroupIterator parentBegin = parentGroup->make_begin_iterator(familyName);
		boost::shared_ptr<BoxIterator<T> > p(new BoxIterator<T>(parentBegin, cornerVector, edge1Vector, edge2Vector, edge3Vector, array.getArray(Type2Type<Vector3D<T> >()), array.length));
		return GroupIterator(p);
	}

	GroupIterator make_end_iterator(std::string const& familyName) {
		return parentGroup->make_end_iterator(familyName);
	}
};

inline
boost::shared_ptr<Group> make_BoxGroup(Simulation const& sim, boost::shared_ptr<Group> const& parent, std::string const& attributeName, Vector3D<double> cornerVector, Vector3D<double> edge1Vector, Vector3D<double> edge2Vector, Vector3D<double> edge3Vector) {
	boost::shared_ptr<Group> p;
	for(Simulation::const_iterator simIter = sim.begin(); simIter != sim.end(); ++simIter) {
		AttributeMap::const_iterator attrIter = simIter->second.attributes.find(attributeName);
		if(attrIter != simIter->second.attributes.end()) {
			//found a family with the attribute
			TypedArray const& arr = attrIter->second;
			if(arr.dimensions == 3) {
				switch(arr.code) {
					case int8:
						p.reset(new BoxGroup<Code2Type<int8>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<int8>::type> >(cornerVector), static_cast<Vector3D<Code2Type<int8>::type> >(edge1Vector), static_cast<Vector3D<Code2Type<int8>::type> >(edge2Vector), static_cast<Vector3D<Code2Type<int8>::type> >(edge3Vector)));
						break;
					case uint8:
						p.reset(new BoxGroup<Code2Type<uint8>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<uint8>::type> >(cornerVector), static_cast<Vector3D<Code2Type<uint8>::type> >(edge1Vector), static_cast<Vector3D<Code2Type<uint8>::type> >(edge2Vector), static_cast<Vector3D<Code2Type<uint8>::type> >(edge3Vector)));
						break;
					case int16:
						p.reset(new BoxGroup<Code2Type<int16>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<int16>::type> >(cornerVector), static_cast<Vector3D<Code2Type<int16>::type> >(edge1Vector), static_cast<Vector3D<Code2Type<int16>::type> >(edge2Vector), static_cast<Vector3D<Code2Type<int16>::type> >(edge3Vector)));
						break;
					case uint16:
						p.reset(new BoxGroup<Code2Type<uint16>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<uint16>::type> >(cornerVector), static_cast<Vector3D<Code2Type<uint16>::type> >(edge1Vector), static_cast<Vector3D<Code2Type<uint16>::type> >(edge2Vector), static_cast<Vector3D<Code2Type<uint16>::type> >(edge3Vector)));
						break;
					case TypeHandling::int32:
						p.reset(new BoxGroup<Code2Type<TypeHandling::int32>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::int32>::type> >(cornerVector), static_cast<Vector3D<Code2Type<TypeHandling::int32>::type> >(edge1Vector), static_cast<Vector3D<Code2Type<TypeHandling::int32>::type> >(edge2Vector), static_cast<Vector3D<Code2Type<TypeHandling::int32>::type> >(edge3Vector)));
						break;
					case TypeHandling::uint32:
						p.reset(new BoxGroup<Code2Type<TypeHandling::uint32>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::uint32>::type> >(cornerVector), static_cast<Vector3D<Code2Type<TypeHandling::uint32>::type> >(edge1Vector), static_cast<Vector3D<Code2Type<TypeHandling::uint32>::type> >(edge2Vector), static_cast<Vector3D<Code2Type<TypeHandling::uint32>::type> >(edge3Vector)));
						break;
					case TypeHandling::int64:
						p.reset(new BoxGroup<Code2Type<TypeHandling::int64>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::int64>::type> >(cornerVector), static_cast<Vector3D<Code2Type<TypeHandling::int64>::type> >(edge1Vector), static_cast<Vector3D<Code2Type<TypeHandling::int64>::type> >(edge2Vector), static_cast<Vector3D<Code2Type<TypeHandling::int64>::type> >(edge3Vector)));
						break;
					case TypeHandling::uint64:
						p.reset(new BoxGroup<Code2Type<TypeHandling::uint64>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<TypeHandling::uint64>::type> >(cornerVector), static_cast<Vector3D<Code2Type<TypeHandling::uint64>::type> >(edge1Vector), static_cast<Vector3D<Code2Type<TypeHandling::uint64>::type> >(edge2Vector), static_cast<Vector3D<Code2Type<TypeHandling::uint64>::type> >(edge3Vector)));
						break;
					case float32:
						p.reset(new BoxGroup<Code2Type<float32>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<float32>::type> >(cornerVector), static_cast<Vector3D<Code2Type<float32>::type> >(edge1Vector), static_cast<Vector3D<Code2Type<float32>::type> >(edge2Vector), static_cast<Vector3D<Code2Type<float32>::type> >(edge3Vector)));
						break;
					case float64:
						p.reset(new BoxGroup<Code2Type<float64>::type>(sim, parent, attributeName, static_cast<Vector3D<Code2Type<float64>::type> >(cornerVector), static_cast<Vector3D<Code2Type<float64>::type> >(edge1Vector), static_cast<Vector3D<Code2Type<float64>::type> >(edge2Vector), static_cast<Vector3D<Code2Type<float64>::type> >(edge3Vector)));
						break;
				}
			}
			break;
		}
	}
	return p;
}

class FamilyGroup : public Group {
	Simulation const& sim;
public:
		
	FamilyGroup(Simulation const& s, boost::shared_ptr<Group> const& parent, std::string const& familyName) : Group(parent), sim(s) {
		families.insert(familyName);
	}
	
	GroupIterator make_begin_iterator(std::string const& familyName) {
		GroupFamilies::const_iterator simIter = families.find(familyName);
		if(simIter == families.end())
			return make_end_iterator(familyName);
		return parentGroup->make_begin_iterator(familyName);
	}

	GroupIterator make_end_iterator(std::string const& familyName) {
		return parentGroup->make_end_iterator(familyName);
	}
};

//Static groups

//Tree-based groups

} //close namespace SimulationHandling

#endif //GROUP_H__7h237h327yh4th7y378w37aw7ho4wvta8
