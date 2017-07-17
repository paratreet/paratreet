//SwapEndianness.h

#indef SWAPENDIANNESS_H
#define SWAPENDIANNESS_H

#include <endian.h>

template <typename T>
inline T swapEndianness(T val) {
	static const unsigned int size = sizeof(T);
	T swapped;
	unsigned char* source = reinterpret_cast<unsigned char *>(&val);
	unsigned char* dest = reinterpret_cast<unsigned char *>(&swapped);
	for(unsigned int i = 0; i < size; ++i)
		dest[i] = source[size - i - 1];
	return swapped;
}

#if __BYTE_ORDER == __BIG_ENDIAN

template <typename T>
inline T toBigEndian(T val) {
}

#else

template <typename T>
inline T toBigEndian(T val) {
	return swapEndianness(val);
}

#endif

/*
template <typename T, int size>
inline void swapBytes(T val);

template <typename T, 4>
inline void swapBytes(T val) {
	unsigned char temp;
	unsigned char* source = reinterpret_cast<unsigned char *>(&val);
	temp = source[3];
	source[3] = source[0];
	source[0] = temp;
	temp = source[2];
	source[2] = source[1];
	source[1] = temp;
}

template <typename T>
inline void swapEndianness(T val) {
	swapBytes<T, sizeof(T)>(val);
}

*/
		
#endif //SWAPENDIANNESS_H
