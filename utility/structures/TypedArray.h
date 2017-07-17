/** @file TypedArray.h
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created September 23, 2003
 @version 1.0
 */

#ifndef TYPEDARRAY_H
#define TYPEDARRAY_H

#include "TypeHandling.h"

#include "OrientedBox.h"

namespace TypeHandling {

/** A TypedArray is the recommended way to store an array of values
 whose dimensionality and type are determined at runtime.  It provides
 methods for accessing the array of values in safe and unsafe ways,
 and offers safe ways to get the minimum and maximum values, as taken
 from the attribute file. */
class TypedArray : public TypeInformation {
	template <typename T>
	void calculateMinMax(Type2Type<T>) {
		T* minVal = reinterpret_cast<T *>(minValue);
		T* maxVal = reinterpret_cast<T *>(maxValue);
		T* array = reinterpret_cast<T *>(data);
		*minVal = array[0];
		*maxVal = array[0];
		for(u_int64_t i = 1; i < length; ++i) {
			if(array[i] < *minVal)
				*minVal = array[i];
			if(*maxVal < array[i])
				*maxVal = array[i];
		}
	}
	
	template <typename T>
	void calculateMinMaxVector(Type2Type<T>) {
		Vector3D<T>* array = reinterpret_cast<Vector3D<T> *>(data);
		OrientedBox<T> box(array[0], array[0]);
		for(u_int64_t i = 1; i < length; ++i)
			box.grow(array[i]);
		*reinterpret_cast<Vector3D<T> *>(minValue) = box.lesser_corner;
		*reinterpret_cast<Vector3D<T> *>(maxValue) = box.greater_corner;
	}
	
	template <typename T>
	void calculateMinMax_dimensions(Type2Type<T>) {
		if(dimensions == 1)
			calculateMinMax(Type2Type<T>());
		else if(dimensions == 3)
			calculateMinMaxVector(Type2Type<T>());
		//else
			//throw UnsupportedDimensionalityException;
	}
	
public:
		
	void* minValue;
	void* maxValue;
	
	u_int64_t length;
	void* data;

	TypedArray() : minValue(0), maxValue(0), length(0), data(0) { }	

	template <typename T>
	TypedArray(T* array, const u_int64_t n) {
		code = Type2Code<T>::code;
		dimensions = Type2Dimensions<T>::dimensions;
		length = n;
		data = array;
		minValue = new T;
		maxValue = new T;
		if(length > 0)
			calculateMinMax();
		else {
			*static_cast<T *>(minValue) = 0;
			*static_cast<T *>(maxValue) = 0;
		}
	}
	
	template <typename T>
	T* getArray(Type2Type<T>) {
	    if(Type2Dimensions<T>::dimensions != dimensions || Type2Code<T>::code != code) {
		throw TypeMismatchException("dimensions wrong");
		return 0;
		}
	    
		return reinterpret_cast<T *>(data);
	}

	template <typename T>
	T const* getArray(Type2Type<T>) const {
	    if(Type2Dimensions<T>::dimensions != dimensions || Type2Code<T>::code != code) {
		throw TypeMismatchException("dimensions wrong");
		return 0;
		}
		return reinterpret_cast<T const*>(data);
	}

	template <typename T>
	T getMinValue(Type2Type<T>) const {
		//if(Type2Dimensions<T>::dimensions != dimensions || Type2Code<T>::code != code)
		//	throw TypeMismatchException;
		return *reinterpret_cast<const T *>(minValue);
	}

	template <typename T>
	T getMaxValue(Type2Type<T>) const {
		//if(Type2Dimensions<T>::dimensions != dimensions || Type2Code<T>::code != code)
		//	throw TypeMismatchException;
		return *reinterpret_cast<const T *>(maxValue);
	}
	
	void release() {
		deleteArray(*this, data);
		deleteValue(*this, minValue);
		deleteValue(*this, maxValue);
	}
	
	void calculateMinMax() {
		if(length != 0) {
			if(minValue == 0)
				minValue = allocateValue(*this);
			if(maxValue == 0)
				maxValue = allocateValue(*this);
			switch(code) {
				case int8:
					return calculateMinMax_dimensions(Type2Type<char>());
				case uint8:
					return calculateMinMax_dimensions(Type2Type<unsigned char>());
				case int16:
					return calculateMinMax_dimensions(Type2Type<short>());
				case uint16:
					return calculateMinMax_dimensions(Type2Type<unsigned short>());
				case int32:
					return calculateMinMax_dimensions(Type2Type<int>());
				case uint32:
					return calculateMinMax_dimensions(Type2Type<unsigned int>());
				case int64:
					return calculateMinMax_dimensions(Type2Type<int64_t>());
				case uint64:
					return calculateMinMax_dimensions(Type2Type<u_int64_t>());
				case float32:
					return calculateMinMax_dimensions(Type2Type<float>());
				case float64:
					return calculateMinMax_dimensions(Type2Type<double>());
				//default:
					//throw UnsupportedTypeException;
			}
		}
	}
	friend std::ostream& operator<<(std::ostream& os, const TypedArray& arr) {
		return os << "Array type " << arr.code << ", dimensionality " << arr.dimensions << ", length " << arr.length;
	}
};

template <typename T>
inline double getScalarValue(const unsigned int dimensions, const void* value) {
	if(dimensions == 1)
		return static_cast<double>(*reinterpret_cast<const T *>(value));
	else if(dimensions == 3)
		return static_cast<double>(reinterpret_cast<const Vector3D<T> *>(value)->length());
	else
		//throw UnsupportedDimensionalityException;
		return 0;
}

inline double getScalarMin(const TypedArray& arr) {
	switch(arr.code) {
		case int8:
			return getScalarValue<char>(arr.dimensions, arr.minValue);
		case uint8:
			return getScalarValue<unsigned char>(arr.dimensions, arr.minValue);
		case int16:
			return getScalarValue<short>(arr.dimensions, arr.minValue);
		case uint16:
			return getScalarValue<unsigned short>(arr.dimensions, arr.minValue);
		case int32:
			return getScalarValue<int>(arr.dimensions, arr.minValue);
		case uint32:
			return getScalarValue<unsigned int>(arr.dimensions, arr.minValue);
		case int64:
			return getScalarValue<int64_t>(arr.dimensions, arr.minValue);
		case uint64:
			return getScalarValue<u_int64_t>(arr.dimensions, arr.minValue);
		case float32:
			return getScalarValue<float>(arr.dimensions, arr.minValue);
		case float64:
			return getScalarValue<double>(arr.dimensions, arr.minValue);
		default:
			//throw UnsupportedTypeException;
			return HUGE_VAL;
	}
}

inline double getScalarMax(const TypedArray& arr) {
	switch(arr.code) {
		case int8:
			return getScalarValue<char>(arr.dimensions, arr.maxValue);
		case uint8:
			return getScalarValue<unsigned char>(arr.dimensions, arr.maxValue);
		case int16:
			return getScalarValue<short>(arr.dimensions, arr.maxValue);
		case uint16:
			return getScalarValue<unsigned short>(arr.dimensions, arr.maxValue);
		case int32:
			return getScalarValue<int>(arr.dimensions, arr.maxValue);
		case uint32:
			return getScalarValue<unsigned int>(arr.dimensions, arr.maxValue);
		case int64:
			return getScalarValue<int64_t>(arr.dimensions, arr.maxValue);
		case uint64:
			return getScalarValue<u_int64_t>(arr.dimensions, arr.maxValue);
		case float32:
			return getScalarValue<float>(arr.dimensions, arr.maxValue);
		case float64:
			return getScalarValue<double>(arr.dimensions, arr.maxValue);
		default:
			//throw UnsupportedTypeException;
			return -HUGE_VAL;
	}
}

template <typename T>
struct Manipulator {
	virtual T operator()(T value) {
		return value;
	}
};

template <typename T>
T coerce(DataTypeCode code, void* data, int index, Type2Type<T>) {
	switch(code) {
		case int8: return static_cast<T>(reinterpret_cast<char *>(data)[index]);
		case uint8: return static_cast<T>(reinterpret_cast<unsigned char *>(data)[index]);
		case int16: return static_cast<T>(reinterpret_cast<short *>(data)[index]);
		case uint16: return static_cast<T>(reinterpret_cast<unsigned short *>(data)[index]);
		case int32: return static_cast<T>(reinterpret_cast<int *>(data)[index]);
		case uint32: return static_cast<T>(reinterpret_cast<unsigned int *>(data)[index]);
		case int64: return static_cast<T>(reinterpret_cast<int64_t *>(data)[index]);
		case uint64: return static_cast<T>(reinterpret_cast<u_int64_t *>(data)[index]);
		case float32: return static_cast<T>(reinterpret_cast<float *>(data)[index]);
		case float64: return static_cast<T>(reinterpret_cast<double *>(data)[index]);
		default: return static_cast<T>(reinterpret_cast<double *>(data)[index]);
	}
}

template <typename T>
Vector3D<T> coerce(DataTypeCode code, void* data, int index, Type2Type<Vector3D<T> >) {
	switch(code) {
		case int8: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<char> *>(data)[index]);
		case uint8: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<unsigned char> *>(data)[index]);
		case int16: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<short> *>(data)[index]);
		case uint16: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<unsigned short> *>(data)[index]);
		case int32: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<int> *>(data)[index]);
		case uint32: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<unsigned int> *>(data)[index]);
		case int64: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<int64_t> *>(data)[index]);
		case uint64: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<u_int64_t> *>(data)[index]);
		case float32: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<float> *>(data)[index]);
		case float64: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<double> *>(data)[index]);
		default: return static_cast<Vector3D<T> >(reinterpret_cast<Vector3D<double> *>(data)[index]);
	}
}

template <typename T>
class CoerciveExtractor {
	const TypedArray& array;
		
public:
	
	CoerciveExtractor(const TypedArray& arr) : array(arr) {
		//if(array.dimensions != Type2Dimensions<T>::dimensions)
			//throw IncompatibleTypesException;
	}

	T operator[](int index) {
		//if array.dimensions != Type2Dimensions<T>::dimensions BAD THINGS WILL HAPPEN!
		return coerce(array.code, array.data, index, Type2Type<T>());
	}
};

} //close namespace TypeHandling

#endif //TYPEDARRAY_H
