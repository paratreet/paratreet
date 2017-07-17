//Interval.h

//Created 11/30/2001
//Graeme Lufkin, gwl@u.washington.edu

#ifndef INTERVAL_H
#define INTERVAL_H

#include <iostream>
#include <cmath>

//this class represents a closed interval in one dimension
template <class T>
class Interval {
public:
	T min;
	T max;
	
	Interval() : min(HUGE_VAL), max(-HUGE_VAL) { }
	
	Interval(const T& minimum) : min(minimum), max(minimum) { }

	Interval(const T& minimum, const T& maximum) : min(minimum), max(maximum) { }
	
	~Interval() { };
	
	T size() const {
		return max - min;
	}
	
	inline bool contains(const T& point) const {
		return min <= point && point <= max;
	}
	
	inline void grow(const T& point) {
		if(point < min)
			min = point;
		if(point > max)
			max = point;
	}
	
	inline bool intersects(const Interval<T>& interval) const {
		return !(interval.max < min || max < interval.min);
	}

	//output operator, used for formatted display
	friend std::ostream& operator<< (std::ostream& os, const Interval<T>& interval) {
		os << '[' << interval.min << ',' << interval.max << ']';
		return os;
	}
	
};

#ifdef __CHARMC__
#include "pup.h"

template <typename T>
inline void operator|(PUP::er& p, Interval<T>& v) {
	p | v.min;
	p | v.max;
}

#endif //__CHARMC__

#endif //INTERVAL_H
