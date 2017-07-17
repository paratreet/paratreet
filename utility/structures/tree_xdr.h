/** @file tree_xdr.h
 Provides class definitions for tree and field headers used in
 tree-based file format.
 @author Graeme Lufkin (gwl@u.washington.edu)
 @version 1.5
 */

#ifndef TREE_XDR_H
#define TREE_XDR_H

#include "Vector3D.h"
#include "OrientedBox.h"
#include "xdr_template.h"
#include "TypedArray.h"

class XDRException : public std::exception {
  public:
    ~XDRException() throw() { }; 
    XDRException();
    XDRException(const std::string & desc);
    virtual std::string getText() const throw();
    virtual const char* what() const throw();
  private:
    std::string d;
  };

class XDRReadError : public XDRException {
  public:
    ~XDRReadError() throw() { }; 
    XDRReadError(std::string s, int64_t w);
    std::string getText() const throw();
 private:
    std::string oper;
    int64_t where;
  };

inline std::ostream & operator <<(std::ostream &os, XDRException &e) {
   os << e.getText();
   return os;
}

inline XDRException::XDRException() : d("") {
}

inline XDRException::XDRException(const std::string & desc)  : d(desc) {
}

inline std::string XDRException::getText() const throw() {
  if(d=="")
    return "Unknown XDR exception";
  else
    return d;
}

inline const char* XDRException::what() const throw() {
  return getText().c_str();
}

inline XDRReadError::XDRReadError(std::string s, int64_t w) : oper(s),
     where(w) {
}

inline std::string XDRReadError::getText() const throw() {
    char sWhere[128];
    sprintf(sWhere, "%ld", (long) where) ;
  return oper + " Error at " + sWhere;
}
/** The header present in every tree file. 
 In conjunction with the positions file, provides all the information 
 needed to perform a tree-based region query on the particle data 
 held in field files.
 */
class TreeHeader {
public:
	static const unsigned int sizeBytes = 84;

	static const int MagicNumber = 3184622;
	
	static const u_int64_t PeriodicFlag = static_cast<u_int64_t>(1) << 0;

	int magic;
	
	double time;
	u_int64_t numNodes;
	u_int64_t numParticles;
	OrientedBox<double> boundingBox;
	u_int64_t flags;
	
	TreeHeader() : magic(MagicNumber), flags(0) { }
	
	/// Output operator, used for formatted display
	friend std::ostream& operator<< (std::ostream& os, const TreeHeader& h) {
		os << "Time: " << h.time
				<< "\nTotal number of particles: " << h.numParticles
				<< "\nTotal number of nodes: " << h.numNodes
				<< "\nBounding box: " << h.boundingBox
				<< "\nFlags: ";
		if(h.flags == 0)
			os << "None";
		else {
			if(h.flags & TreeHeader::PeriodicFlag)
				os << "Periodic ";
		}
		return os;
	}
};

inline bool_t xdr_template(XDR* xdrs, TreeHeader* h) {
	return (xdr_template(xdrs, &(h->magic))
			&& xdr_template(xdrs, &(h->time))
			&& xdr_template(xdrs, &(h->numNodes))
			&& xdr_template(xdrs, &(h->numParticles))
			&& xdr_template(xdrs, &(h->boundingBox))
			&& xdr_template(xdrs, &(h->flags)));
}

/** The header present in every file of attribute data (a field).
 */
class FieldHeader {
public:
	
	static const unsigned int sizeBytes = 28;

	static const int MagicNumber = 1062053;

	int magic;
	double time;
	u_int64_t numParticles;
	unsigned int dimensions; //1 for scalar, 3 for vector
	TypeHandling::DataTypeCode code;
	
	FieldHeader() : magic(MagicNumber) { }
	
	FieldHeader(const TypeHandling::TypedArray& arr) : magic(MagicNumber), numParticles(arr.length), dimensions(arr.dimensions), code(arr.code) { }
	
	/// Output operator, used for formatted display
	friend std::ostream& operator<< (std::ostream& os, const FieldHeader& h) {
		return os << "Time: " << h.time
				<< "\nTotal number of particles: " << h.numParticles
				<< "\nDimensions: " << h.dimensions
				<< "\nData Type: " << h.code;
	}
};

inline bool_t xdr_template(XDR* xdrs, FieldHeader* h) {
	return (xdr_template(xdrs, &(h->magic))
			&& xdr_template(xdrs, &(h->time))
			&& xdr_template(xdrs, &(h->numParticles))
			&& xdr_template(xdrs, &(h->dimensions))
			&& xdr_template(xdrs, reinterpret_cast<enum_t *>(&(h->code))));
}

// XDR has a minimum size of 4 bytes.
inline unsigned int mySizeof(TypeHandling::DataTypeCode code) {
	switch(code) {
		case TypeHandling::int8:
		case TypeHandling::uint8:
		case TypeHandling::int16:
		case TypeHandling::uint16:
		case TypeHandling::int32:
		case TypeHandling::uint32:
			return 4;
		case TypeHandling::int64:
		case TypeHandling::uint64:
			return 8;
		case TypeHandling::float32:
			return 4;
		case TypeHandling::float64:
			return 8;
		default:
			return 0;
	}
}

/** Given a FieldHeader, check that the dimensionality and type
 match the one you expect.  That is, checkType<T>(fh) will compare the
 dimensionality and contained type of T with the type specified by fh. */
template <typename T>
inline bool checkType(const FieldHeader& fh) {
	return TypeHandling::Type2Dimensions<T>::dimensions == fh.dimensions && TypeHandling::Type2Code<T>::code == fh.code;
}

/** Write a field to an XDR stream.  You have to write the header and min/max 
 yourself before you call this if you want to use the result again.
 */
template <typename T>
inline bool writeField(XDR* xdrs, const u_int64_t N, T* data) {
	for(u_int64_t i = 0; i < N; ++i) {
		if(!xdr_template(xdrs, data + i)) {
			return false;
		}
	}
	return true;
}

/** For three-dimensional types, change cast to a Vector3D of the type. */
template <typename T>
inline bool writeField(XDR* xdrs, const unsigned int dimensions, const u_int64_t N, T* data) {
	if(dimensions == 3)
		return writeField(xdrs, N, reinterpret_cast<Vector3D<T> * >(data));
	else
		return writeField(xdrs, N, data);
}

/** Given a type-code, cast to the appropriate type of pointer. */
inline bool writeFieldSwitch(XDR* xdrs, TypeHandling::DataTypeCode code, const unsigned int dimensions, const u_int64_t N, void* data) {
	switch(code) {
		case TypeHandling::int8:
			return writeField(xdrs, dimensions, N, reinterpret_cast<char *>(data));
		case TypeHandling::uint8:
			return writeField(xdrs, dimensions, N, reinterpret_cast<unsigned char *>(data));
		case TypeHandling::int16:
			return writeField(xdrs, dimensions, N, reinterpret_cast<short *>(data));
		case TypeHandling::uint16:
			return writeField(xdrs, dimensions, N, reinterpret_cast<unsigned short *>(data));
		case TypeHandling::int32:
			return writeField(xdrs, dimensions, N, reinterpret_cast<int *>(data));
		case TypeHandling::uint32:
			return writeField(xdrs, dimensions, N, reinterpret_cast<unsigned int *>(data));
		case TypeHandling::int64:
			return writeField(xdrs, dimensions, N, reinterpret_cast<int64_t *>(data));
		case TypeHandling::uint64:
			return writeField(xdrs, dimensions, N, reinterpret_cast<u_int64_t *>(data));
		case TypeHandling::float32:
			return writeField(xdrs, dimensions, N, reinterpret_cast<float *>(data));
		case TypeHandling::float64:
			return writeField(xdrs, dimensions, N, reinterpret_cast<double *>(data));
		default:
			return false;
	}
}

/** Given the type code in the header, write the correct data type
 for the field.  You need to pass a correctly filled-in header, the array
 of values, and pointers to the minimum and maximum values.
 */
inline bool writeField(FieldHeader fh, XDR* xdrs, void* data, void* minValue, void* maxValue) {
	if(fh.dimensions != 1 && fh.dimensions != 3)
		return false;
	if(!xdr_template(xdrs, &fh))
		return false;
	return writeFieldSwitch(xdrs, fh.code, fh.dimensions, 1, minValue)
			&& writeFieldSwitch(xdrs, fh.code, fh.dimensions, 1, maxValue)
			&& writeFieldSwitch(xdrs, fh.code, fh.dimensions, fh.numParticles, data);
}

/** Allocate for and read in a field from an XDR stream.  You need to have
 read the header already.  The min/max pair are put at the end of the array.
 */
template <typename T>
inline T* readField(XDR* xdrs, const u_int64_t N, const u_int64_t startParticle = 0) {
	T* data = new T[N + 2];
	//put min/max at the end
	if(!xdr_template(xdrs, data + N) || !xdr_template(xdrs, data + N + 1)) {
		delete[] data;
		return 0;
	}
	if(data[N] == data[N + 1]) { 
		//if all elements are the same, just copy the value into the array
		for(u_int64_t i = 0; i < N; ++i)
			data[i] = data[N];
	} else {
		off_t offset = FieldHeader::sizeBytes
				+ (startParticle + 2) * TypeHandling::Type2Dimensions<T>::dimensions * mySizeof(TypeHandling::Type2Code<T>::code);
/* XXX NASTY kludge to get around 4 byte limit of xdr functions */
#define __BILLION 1000000000
		fseek((FILE *)xdrs->x_private, 0, 0);
		while(offset > __BILLION) {
			offset -= __BILLION;
			fseek((FILE *)xdrs->x_private, __BILLION,
				SEEK_CUR);
		}
		if(fseek((FILE *)xdrs->x_private, offset, SEEK_CUR) != 0) {
		   std::cerr << "readField seek failed: " << offset << std::endl;
			delete[] data;
			return 0;
		}
#if 0
 		if(!xdr_setpos(xdrs, FieldHeader::sizeBytes + (startParticle + 2) * TypeHandling::Type2Dimensions<T>::dimensions * mySizeof(TypeHandling::Type2Code<T>::code))) {
			delete[] data;
			return 0;
		}
#endif
 		for(u_int64_t i = 0; i < N; ++i) {
			if(!xdr_template(xdrs, data + i)) {
				delete[] data;
				return 0;
			}
		}
	}
	return data;
}

template <typename T>
inline void* readField(XDR* xdrs, const unsigned int dimensions, const u_int64_t N, const u_int64_t startParticle = 0) {
	if(dimensions == 3)
		return readField<Vector3D<T> >(xdrs, N, startParticle);
	else
		return readField<T>(xdrs, N, startParticle);
}

/** Given the type code in the header, reads in the correct type of data.
 */
inline void* readField(const FieldHeader& fh, XDR* xdrs, u_int64_t numParticles = 0, const u_int64_t startParticle = 0) {
	if(fh.dimensions != 1 && fh.dimensions != 3)
		return 0;
	if(numParticles == 0)
		numParticles = fh.numParticles;
	switch(fh.code) {
		case TypeHandling::int8:
			return readField<char>(xdrs, fh.dimensions, numParticles, startParticle);
		case TypeHandling::uint8:
			return readField<unsigned char>(xdrs, fh.dimensions, numParticles, startParticle);
		case TypeHandling::int16:
			return readField<short>(xdrs, fh.dimensions, numParticles, startParticle);
		case TypeHandling::uint16:
			return readField<unsigned short>(xdrs, fh.dimensions, numParticles, startParticle);
		case TypeHandling::int32:
			return readField<int>(xdrs, fh.dimensions, numParticles, startParticle);
		case TypeHandling::uint32:
			return readField<unsigned int>(xdrs, fh.dimensions, numParticles, startParticle);
		case TypeHandling::int64:
			return readField<int64_t>(xdrs, fh.dimensions, numParticles, startParticle);
		case TypeHandling::uint64:
			return readField<u_int64_t>(xdrs, fh.dimensions, numParticles, startParticle);
		case TypeHandling::float32:
			return readField<float>(xdrs, fh.dimensions, numParticles, startParticle);
		case TypeHandling::float64:
			return readField<double>(xdrs, fh.dimensions, numParticles, startParticle);
		default:
			return 0;
	}
}

/** Using an array of original indices, allocate for and read in a field,
 putting the elements back in the original order. 
 */
template <typename T>
inline T* readField_reorder(XDR* xdrs, const u_int64_t N, const unsigned int* uids) {
	T* data = new T[N + 2];
	//put min/max at the end
	if(!xdr_template(xdrs, data + N) || !xdr_template(xdrs, data + N + 1)) {
		delete[] data;
		return 0;
	}
	if(data[N] == data[N + 1]) { 
		//if all elements are the same, just copy the value into the array
		for(u_int64_t i = 0; i < N; ++i)
			data[i] = data[N];
	} else {
		for(u_int64_t i = 0; i < N; ++i) {
			if(uids[i] >= N || !xdr_template(xdrs, data + uids[i])) {
				delete[] data;
				return 0;
			}
		}
	}
	return data;
}

template <typename T>
inline void* readField_reorder(XDR* xdrs, const unsigned int dimensions, const u_int64_t N, const unsigned int* uids) {
	if(dimensions == 3)
		return readField_reorder<Vector3D<T> >(xdrs, N, uids);
	else
		return readField_reorder<T>(xdrs, N, uids);
}

/** Using the type code in the header, read in and reorder a field. 
 */
inline void* readField_reorder(const FieldHeader& fh, XDR* xdrs, const unsigned int* uids) {
	if(fh.dimensions != 1 && fh.dimensions != 3)
		return 0;
	switch(fh.code) {
		case TypeHandling::int8:
			return readField_reorder<char>(xdrs, fh.dimensions, fh.numParticles, uids);
		case TypeHandling::uint8:
			return readField_reorder<unsigned char>(xdrs, fh.dimensions, fh.numParticles, uids);
		case TypeHandling::int16:
			return readField_reorder<short>(xdrs, fh.dimensions, fh.numParticles, uids);
		case TypeHandling::uint16:
			return readField_reorder<unsigned short>(xdrs, fh.dimensions, fh.numParticles, uids);
		case TypeHandling::int32:
			return readField_reorder<int>(xdrs, fh.dimensions, fh.numParticles, uids);
		case TypeHandling::uint32:
			return readField_reorder<unsigned int>(xdrs, fh.dimensions, fh.numParticles, uids);
		case TypeHandling::int64:
			return readField_reorder<int64_t>(xdrs, fh.dimensions, fh.numParticles, uids);
		case TypeHandling::uint64:
			return readField_reorder<u_int64_t>(xdrs, fh.dimensions, fh.numParticles, uids);
		case TypeHandling::float32:
			return readField_reorder<float>(xdrs, fh.dimensions, fh.numParticles, uids);
		case TypeHandling::float64:
			return readField_reorder<double>(xdrs, fh.dimensions, fh.numParticles, uids);
		default:
			return 0;
	}
}

template <typename T>
inline bool deleteField(const unsigned int dimensions, void*& data) {
	if(dimensions == 3)
		delete[] reinterpret_cast<Vector3D<T> *>(data);
	else
		delete[] reinterpret_cast<T *>(data);
	//zero the pointer (it was passed by reference, so this WILL affect the pointer you called with)
	data = 0;
	return true;
}

inline bool deleteField(const FieldHeader& fh, void*& data) {
	if(fh.dimensions != 1 && fh.dimensions != 3)
		return false;
	switch(fh.code) {
		case TypeHandling::int8:
			return deleteField<char>(fh.dimensions, data);
		case TypeHandling::uint8:
			return deleteField<unsigned char>(fh.dimensions, data);
		case TypeHandling::int16:
			return deleteField<short>(fh.dimensions, data);
		case TypeHandling::uint16:
			return deleteField<unsigned short>(fh.dimensions, data);
		case TypeHandling::int32:
			return deleteField<int>(fh.dimensions, data);
		case TypeHandling::uint32:
			return deleteField<unsigned int>(fh.dimensions, data);
		case TypeHandling::int64:
			return deleteField<int64_t>(fh.dimensions, data);
		case TypeHandling::uint64:
			return deleteField<u_int64_t>(fh.dimensions, data);
		case TypeHandling::float32:
			return deleteField<float>(fh.dimensions, data);
		case TypeHandling::float64:
			return deleteField<double>(fh.dimensions, data);
		default:
			return false;
	}
}

/** Given a header and a stream, seek to the desired location in the field stream.
 */
inline bool_t seekField(const FieldHeader& fh, XDR* xdrs, const u_int64_t index) {
	off_t offset = FieldHeader::sizeBytes + (index + 2) * fh.dimensions * mySizeof(fh.code);
	/* XXX NASTY kludge to get around 4 byte limit of xdr functions */
	fseek((FILE *)xdrs->x_private, 0, 0);
	while(offset > __BILLION) {
		offset -= __BILLION;
		fseek((FILE *)xdrs->x_private, __BILLION,
			SEEK_CUR);
	}
	if(fseek((FILE *)xdrs->x_private, offset, SEEK_CUR) != 0) {
	   std::cerr << "seekField failed: " << offset << std::endl;
		return false;
	}
	return true;
#if 0
	return xdr_setpos(xdrs, FieldHeader::sizeBytes + (index + 2) * fh.dimensions * mySizeof(fh.code));
#endif
}

/** The tree file contains these structures.  With the total number of
 nodes and particles and the bounding box (found in the tree header)
 you can do a tree-based search for particles using these nodes.
 */
struct BasicTreeNode {
	u_int64_t numNodesLeft;
	u_int64_t numParticlesLeft;
	
	bool operator==(const BasicTreeNode& n) const { 
		return (numNodesLeft == n.numNodesLeft) && (numParticlesLeft == n.numParticlesLeft);
	}
};

inline bool_t xdr_template(XDR* xdrs, BasicTreeNode* node) {
	return xdr_template(xdrs, &(node->numNodesLeft)) 
			&& xdr_template(xdrs, &(node->numParticlesLeft));
}

//New, preferred naming convention is *attribute*

template <typename T>
inline bool readAttributes_typed(XDR* xdrs, TypeHandling::TypedArray& arr, const u_int64_t N, const u_int64_t start = 0) {
	//seek to just past the header
	if(!xdr_setpos(xdrs, FieldHeader::sizeBytes)) {
	    throw XDRReadError("Seek", FieldHeader::sizeBytes);
	    }
	//allocate for and read the minimum value
	T* minVal = new T;
	if(!xdr_template(xdrs, minVal)) {
		delete minVal;
		throw XDRReadError("ReadMin", FieldHeader::sizeBytes);
	}
	//set the minimum value
	arr.minValue = minVal;
	//allocate for and read the maximum value
	T* maxVal = new T;
	if(!xdr_template(xdrs, maxVal)) {
		delete maxVal;
		throw XDRReadError("ReadMax", xdr_getpos(xdrs));
	}
	//set the minimum value
	arr.maxValue = maxVal;
	
	T* data = 0;
	if(N != 0) {
		if(*minVal == *maxVal) {
			//all values are the same (file doesn't contain replicas of the value)
			data = new T[N];
			for(u_int64_t i = 0; i < N; ++i)
				data[i] = *minVal;
		} else {
			int64_t offset = FieldHeader::sizeBytes
					+ (start + 2) * TypeHandling::Type2Dimensions<T>::dimensions * mySizeof(TypeHandling::Type2Code<T>::code);
	/* XXX NASTY kludge to get around 4 byte limit of xdr functions */
			fseek((FILE *)xdrs->x_private, 0, 0);
			while(offset > __BILLION) {
				offset -= __BILLION;
				fseek((FILE *)xdrs->x_private, __BILLION,
					SEEK_CUR);
			}
			if(fseek((FILE *)xdrs->x_private, offset, SEEK_CUR) != 0) {
			   throw XDRReadError("fseek", offset);
			}
			//seek to the starting value
#if 0
			if(!xdr_setpos(xdrs, FieldHeader::sizeBytes + (start + 2) * TypeHandling::Type2Dimensions<T>::dimensions * mySizeof(TypeHandling::Type2Code<T>::code)))
				//throw FileReadException;
				return false;
#endif
			//allocate for the array of values
			data = new T[N];
			for(u_int64_t i = 0; i < N; ++i) {
				if(!xdr_template(xdrs, data + i)) {
					delete[] data;
					throw XDRReadError("data read", i);
				}
			}
		}
	}
	//set the array of values
	arr.data = data;
	arr.length = N;
	
	return true;
}

template <typename T>
inline bool readAttributes_dimensions(XDR* xdrs, TypeHandling::TypedArray& arr, const u_int64_t N, const u_int64_t start = 0) {
	if(arr.dimensions == 1)
		return readAttributes_typed<T>(xdrs, arr, N, start);
	else if(arr.dimensions == 3)
		return readAttributes_typed<Vector3D<T> >(xdrs, arr, N, start);
	else
		//throw UnsupportedDimensionalityException;
		return false;
}

inline bool readAttributes(XDR* xdrs, TypeHandling::TypedArray& arr, const u_int64_t N, const u_int64_t start = 0) {
	switch(arr.code) {
		case TypeHandling::int8:
			return readAttributes_dimensions<char>(xdrs, arr, N, start);
		case TypeHandling::uint8:
			return readAttributes_dimensions<unsigned char>(xdrs, arr, N, start);
		case TypeHandling::int16:
			return readAttributes_dimensions<short>(xdrs, arr, N, start);
		case TypeHandling::uint16:
			return readAttributes_dimensions<unsigned short>(xdrs, arr, N, start);
		case TypeHandling::int32:
			return readAttributes_dimensions<int>(xdrs, arr, N, start);
		case TypeHandling::uint32:
			return readAttributes_dimensions<unsigned int>(xdrs, arr, N, start);
		case TypeHandling::int64:
			return readAttributes_dimensions<int64_t>(xdrs, arr, N, start);
		case TypeHandling::uint64:
			return readAttributes_dimensions<u_int64_t>(xdrs, arr, N, start);
		case TypeHandling::float32:
			return readAttributes_dimensions<float>(xdrs, arr, N, start);
		case TypeHandling::float64:
			return readAttributes_dimensions<double>(xdrs, arr, N, start);
		default:
			//throw UnsupportedTypeException;
			return false;
	}
}

template <typename T, typename PromotedT>
inline bool readAttributesPromote_typed(XDR* xdrs, TypeHandling::TypedArray& arr, const u_int64_t N, const u_int64_t start = 0) {
	//seek to just past the header
	if(!xdr_setpos(xdrs, FieldHeader::sizeBytes)) {
	    throw XDRReadError("Seek", FieldHeader::sizeBytes);
	    return false;
	    }
	
	//read the values in, then convert them to the promoted type
		
	//allocate for and read the minimum value
	PromotedT* minVal = new PromotedT;
	T value;
	if(!xdr_template(xdrs, &value)) {
		delete minVal;
		throw XDRReadError("ReadMin", FieldHeader::sizeBytes);
		return false;
	}
	*minVal = value;
	//set the minimum value
	arr.minValue = minVal;
	//allocate for and read the maximum value
	PromotedT* maxVal = new PromotedT;
	if(!xdr_template(xdrs, &value)) {
		delete maxVal;
		throw XDRReadError("ReadMax", xdr_getpos(xdrs));
		return false;
	}
	*maxVal = value;
	//set the minimum value
	arr.maxValue = maxVal;
	
	PromotedT* data = 0;
	if(*minVal == *maxVal) {
		//all values are the same (file doesn't contain replicas of the value)
		data = new PromotedT[N];
		for(u_int64_t i = 0; i < N; ++i)
			data[i] = *minVal;
	} else if(N != 0) {
		off_t offset = FieldHeader::sizeBytes
				+ (start + 2) * TypeHandling::Type2Dimensions<T>::dimensions * mySizeof(TypeHandling::Type2Code<T>::code);
	/* XXX NASTY kludge to get around 4 byte limit of xdr functions */
		fseek((FILE *)xdrs->x_private, 0, 0);
		while(offset > __BILLION) {
			offset -= __BILLION;
			fseek((FILE *)xdrs->x_private, __BILLION,
				SEEK_CUR);
		}
		if(fseek((FILE *)xdrs->x_private, offset, SEEK_CUR) != 0) {
		    throw XDRReadError("fseek", offset);
		    return false;
		}
		//seek to the starting value
#if 0
		if(!xdr_setpos(xdrs, FieldHeader::sizeBytes + (start + 2) * TypeHandling::Type2Dimensions<T>::dimensions * mySizeof(TypeHandling::Type2Code<T>::code)))
			//throw FileReadException;
			return false;
#endif
		//allocate for the array of values
		data = new PromotedT[N];
		for(u_int64_t i = 0; i < N; ++i) {
			if(!xdr_template(xdrs, &value)) {
				delete[] data;
				throw XDRReadError("data read", i);
				return false;
			}
			data[i] = value;
		}
	}
	//set the array of values
	arr.data = data;
	arr.length = N;
	//set the type of the array to the promoted type
	arr.code = TypeHandling::Type2Code<PromotedT>::code;
	
	return true;
}

template <typename T>
inline bool writeAttributes_typed(XDR* xdrs, const TypeHandling::TypedArray& arr, const u_int64_t startParticle = 0) {
	FieldHeader fh(arr);
        if(startParticle == 0){
            //write the header
            if(!xdr_template(xdrs, &fh))
                    return false;
            
            //write minimum and maximum values
            if(!xdr_template(xdrs, reinterpret_cast<T *>(arr.minValue)) || !xdr_template(xdrs, reinterpret_cast<T *>(arr.maxValue)))
                    return false;
	} else {
            //seek to the starting value
            int64_t offset = FieldHeader::sizeBytes + 
                    (startParticle + 2) * TypeHandling::Type2Dimensions<T>::dimensions * mySizeof(TypeHandling::Type2Code<T>::code);
	/* XXX NASTY kludge to get around 4 byte limit of xdr functions */
            fseek((FILE *)xdrs->x_private, 0, 0);
            while(offset > __BILLION) {
                    offset -= __BILLION;
                    fseek((FILE *)xdrs->x_private, __BILLION,
                            SEEK_CUR);
            }
            if(fseek((FILE *)xdrs->x_private, offset, SEEK_CUR) != 0) {
               throw XDRReadError("fseek", offset);
            }
        }
        
	//write the attribute values
        T* data = reinterpret_cast<T *>(arr.data);
        for(u_int64_t i = 0; i < arr.length; ++i)
                if(!xdr_template(xdrs, data + i))
                        return false;
	return true;
}

template <typename T>
inline bool writeAttributes_dimensions(XDR* xdrs, const TypeHandling::TypedArray& arr, const u_int64_t startParticle = 0) {
	if(arr.dimensions == 1)
		return writeAttributes_typed<T>(xdrs, arr, startParticle);
	else if(arr.dimensions == 3)
		return writeAttributes_typed<Vector3D<T> >(xdrs, arr, startParticle);
	else
		//throw UnsupportedDimensionalityException;
		return false;
}

inline bool writeAttributes(XDR* xdrs, const TypeHandling::TypedArray& arr, const u_int64_t startParticle = 0) {
	switch(arr.code) {
		case TypeHandling::int8:
			return writeAttributes_dimensions<char>(xdrs, arr, startParticle);
		case TypeHandling::uint8:
			return writeAttributes_dimensions<unsigned char>(xdrs, arr, startParticle);
		case TypeHandling::int16:
			return writeAttributes_dimensions<short>(xdrs, arr, startParticle);
		case TypeHandling::uint16:
			return writeAttributes_dimensions<unsigned short>(xdrs, arr, startParticle);
		case TypeHandling::int32:
			return writeAttributes_dimensions<int>(xdrs, arr, startParticle);
		case TypeHandling::uint32:
			return writeAttributes_dimensions<unsigned int>(xdrs, arr, startParticle);
		case TypeHandling::int64:
			return writeAttributes_dimensions<int64_t>(xdrs, arr, startParticle);
		case TypeHandling::uint64:
			return writeAttributes_dimensions<u_int64_t>(xdrs, arr, startParticle);
		case TypeHandling::float32:
			return writeAttributes_dimensions<float>(xdrs, arr, startParticle);
		case TypeHandling::float64:
			return writeAttributes_dimensions<double>(xdrs, arr, startParticle);
		default:
			//throw UnsupportedTypeException;
			return false;
	}
}

#ifdef __CHARMC__
#include "pup.h"

inline void operator|(PUP::er& p, TreeHeader& h) {
	p | h.magic;
	p | h.time;
	p | h.numNodes;
	p | h.numParticles;
	p | h.boundingBox;
	p | h.flags;
}

inline void operator|(PUP::er& p, FieldHeader& h) {
	p | h.magic;
	p | h.time;
	p | h.numParticles;
	p | h.dimensions;
	int enum_int;
	if(p.isUnpacking()) {
		p | enum_int;
		h.code = TypeHandling::DataTypeCode(enum_int);
	} else {
		enum_int = static_cast<int>(h.code);
		p | enum_int;
	}
}

inline void operator|(PUP::er& p, BasicTreeNode& n) {
	p | n.numNodesLeft;
	p | n.numParticlesLeft;
}

#endif //__CHARMC__


#endif //TREE_XDR_H
