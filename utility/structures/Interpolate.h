/** \file Interpolate.h
 This file defines classes that represent interpolated functions. 
 \author Graeme Lufkin (gwl@u.washington.edu)
 \date Created May 1, 2000
 \version 2.0
 \todo Fully document this file.
 */

#ifndef INTERPOLATE_H
#define INTERPOLATE_H

#include <vector>

/**
 * This class does linear interpolation on a data set with function values 
 * specified initially at a number of values of the independent variable.
 * The independent variable values need not be equally spaced.
 * After initialization, calls to this object with a value of the independent
 * variable will return the linear interpolation of the value of the function
 * at the requested point, based on the surrounding values.  If you ask for
 * a value outside the initially specified range, the end value will be returned.
 */
template <typename T = double>
class LinearInterpolator {
private:
	
	bool ready;
	std::vector<T> indeps;
	std::vector<T> deps;
	std::vector<T> coefs;
	unsigned int size;
	mutable int klo, khi;
	
public:
	
	/// Default constructor, is non-working
	LinearInterpolator() {
		ready = false;
	}
	
	//create a linear interpolator object, given iterator pairs for the independent
	//variables and the dependent variables (the function values at the specified points).
	template <typename InputIterator, typename InputIterator2>
	LinearInterpolator(InputIterator beginIndep, InputIterator endIndep, 
			InputIterator2 beginDep, InputIterator2 endDep) : indeps(beginIndep, endIndep), 
			deps(beginDep, endDep) {

		size = indeps.size();
		if(size != deps.size() || size == 0) {
			ready = false;
			return;
		}
		
		coefs.assign(size - 1, 0);
		
		//calculate the coefficients to be used when interpolating
		for(unsigned int i = 0; i < size - 1; i++)
			coefs[i] = (deps[i + 1] - deps[i]) / (indeps[i + 1] - indeps[i]);
		
		klo = 0;
		khi = size - 1;		
		
		ready = true;
	}
	
	/// The linear interpolator object can be called just like a regular function
	T operator()(T x) const {
		if(x < indeps[klo]) { //lower bracket than last time
			if(x < indeps[0])
				return deps[0];
			else if(indeps[klo - 1] < x) { //it's the next one down
				klo--;
				khi--;
			} else //set low location as low as possible
				klo = 0;
		} else if(indeps[khi] < x) { //greater bracket than last time
			if(indeps[size - 1] < x)
				return deps[size - 1];
			else if(x < indeps[khi + 1]) { //it's the next one up
				khi++;
				klo++;
			} else //set high location as high as possible
				khi = size - 1;
		} //else, it's the same bracket as last time, don't change bounds
		
		int k;
		while(khi - klo > 1) { //do binary search to find bracket
			k = (khi + klo) / 2;
			if(x < indeps[k])
				khi = k;
			else
				klo = k;
		}
		
		//use the coefficients to give f(x) = a * x + b
		return coefs[klo] * (x - indeps[klo]) + deps[klo];
	}
	
	bool isReady() const {
		return ready;
	}
};

/** Bilinear interpolation.  The independent variables must be equally spaced. */
template <typename T = double>
class BilinearInterpolator {
	bool ready;
	T min_x, min_y, max_x, max_y;
	unsigned int num_x, num_y;
	std::vector<T> deps;
	T delta_x, delta_y;
	mutable int klo_x, khi_x, klo_y, khi_y;
	
public:
	
	/// Default constructor, is non-working
	BilinearInterpolator() {
		ready = false;
	}
	
	/// Values are specified at the center of equally spaced bins (min/max specify the edge of the boundary grid cells)
	template <typename Iterator>
	BilinearInterpolator(T xmin, T ymin, T xmax, T ymax, 
			unsigned int nx, unsigned int ny, 
			Iterator beginDep, Iterator endDep) 
		: ready(false), min_x(xmin), min_y(ymin), max_x(xmax), max_y(ymax), num_x(nx), num_y(ny), deps(beginDep, endDep) {

		if(num_x < 2 || num_y < 2 || num_x * num_y != deps.size())
			return;
		
		delta_x = (max_x - min_x) / (num_x - 1);
		delta_y = (max_y - min_y) / (num_y - 1);
		
		klo_x = klo_y = 0;
		khi_x = num_x - 1;
		khi_y = num_y - 1;
		
		ready = true;
	}
	
	T operator()(T x, T y) const {
		int k;
		if(x < min_x + klo_x * delta_x) { //lower bracket than last time
			if(x < min_x) { //out of bounds on the left side
				klo_x = 0;
				khi_x = 1;
				x = min_x;
			} else if(min_x + (klo_x - 1) * delta_x < x) { //it's the next one down
				klo_x--;
				khi_x--;
			} else //set low location as low as possible
				klo_x = 0;
		} else if(min_x + khi_x * delta_x < x) { //greater bracket than last time
			if(max_x < x) { //out of bounds on the right side
				klo_x = num_x - 1;
				khi_x = num_x - 1;
				x = max_x;
			} else if(x < min_x + (khi_x + 1) * delta_x) { //it's the next one up
				khi_x++;
				klo_x++;
			} else //set high location as high as possible
				khi_x = num_x - 1;
		} //else, it's the same bracket as last time, don't change bounds
		
		while(khi_x - klo_x > 1) { //do binary search to find bracket
			k = (khi_x + klo_x) / 2;
			if(x < min_x + k * delta_x)
				khi_x = k;
			else
				klo_x = k;
		}
		
		if(y < min_y + klo_y * delta_y) { //lower bracket than last time
			if(y < min_y) { //out of bounds on the left side
				klo_y = 0;
				khi_y = 1;
				y = min_y;
			} else if(min_y + (klo_y - 1) * delta_y < y) { //it's the next one down
				klo_y--;
				khi_y--;
			} else //set low location as low as possible
				klo_y = 0;
		} else if(min_y + khi_y * delta_y < y) { //greater bracket than last time
			if(max_y < y) { //out of bounds on the right side
				klo_y = num_y - 1;
				khi_y = num_y - 1;
				y = max_y;
			} else if(y < min_y + (khi_y + 1) * delta_y) { //it's the next one up
				khi_y++;
				klo_y++;
			} else //set high location as high as possible
				khi_y = num_y - 1;
		} //else, it's the same bracket as last time, don't change bounds
		
		while(khi_y - klo_y > 1) { //do binary search to find bracket
			k = (khi_y + klo_y) / 2;
			if(y < min_y + k * delta_y)
				khi_y = k;
			else
				klo_y = k;
		}
		
		//give f(x,y)
		T A = (x - min_x) / delta_x - klo_x;
		T B = (y - min_y) / delta_y - klo_y;
		return deps[klo_x + num_x * klo_y] * (1 - A - B + A * B)
				+ deps[khi_x + num_x * klo_y] * (A - A * B)
				+ deps[klo_x + num_x * khi_y] * (B - A * B)
				+ deps[khi_x + num_x * khi_y] * A * B;
	}
	
	bool isReady() const {
		return ready;
	}
};

template <typename T>
class SplineDerivative;

/**
 * This class does cubic spline interpolation on a data set with function values 
 * specified initially at a number of values of the independent variable.
 * It functions identically to the linear interpolator.  The first derivative
 * at the first and last points can be specified.  If they are not, then the
 * "natural" spline is used, where the second derivative is zero at the end
 * points.  If you ask for a value outside the initially specified range,
 * the linear extrapolation is returned, using the value of the first 
 * derivative at the end point.
 */
template <typename T = double>
class SplineInterpolator {
	friend class SplineDerivative<T>;
private:
	
	bool ready;
	std::vector<T> indeps;
	std::vector<T> deps;
	std::vector<T> secondDerivs;
	T beginFirstDeriv;
	T endFirstDeriv;
	unsigned int size;
	mutable int klo, khi;
	
public:
	
	//default constructor, is non-working
	SplineInterpolator() {
		ready = false;
	}
		
	/* Create a spline interpolator object.
	 * This version takes specific values for the first derivatives at the end points.
	 */
	template <typename InputIterator, typename InputIterator2>
	SplineInterpolator(InputIterator beginIndep, InputIterator endIndep,
			InputIterator2 beginDep, InputIterator2 endDep, T beginPrime,
			T endPrime) : indeps(beginIndep, endIndep), deps(beginDep, endDep),
			beginFirstDeriv(beginPrime), endFirstDeriv(endPrime) {
		
		size = indeps.size();
		if(size != deps.size() || size == 0) {
			ready = false;
			return;
		}
		
		secondDerivs.assign(size, 0);
		
		std::vector<T> u(size - 1);
		T sig, p, qn, un;
		
		secondDerivs[0] = -0.5;
		u[0] = (3.0 / (indeps[1] - indeps[0])) * ((deps[1] - deps[0]) / (indeps[1] - indeps[0]) - beginFirstDeriv);

		for(unsigned int i = 1; i < size - 1; i++) {
			sig = (indeps[i] - indeps[i - 1]) / (indeps[i + 1] - indeps[i - 1]);
			p = sig * secondDerivs[i - 1] + 2.0;
			secondDerivs[i] = (sig - 1.0) / p;
			u[i] = (deps[i + 1] - deps[i]) / (indeps[i + 1] - indeps[i]) - (deps[i] - deps[i - 1]) / (indeps[i] - indeps[i - 1]);
			u[i] = (6.0 * u[i] / (indeps[i + 1] - indeps[i - 1]) - sig * u[i - 1]) / p;
		}
		
		qn = 0.5;
		un = (3.0 / (indeps[size - 1] - indeps[size - 2])) * (endFirstDeriv - (deps[size - 1] - deps[size - 2]) / (indeps[size - 1] - indeps[size - 2]));

		secondDerivs[size - 1] = (un - qn * u[size - 2]) / (qn * secondDerivs[size - 2] + 1.0);
		for(int i = size - 2; i >= 0; i--)
			secondDerivs[i] = secondDerivs[i] * secondDerivs[i + 1] + u[i];
				
		klo = 0;
		khi = size - 1;		
		
		ready = true;
	}
	
	/* Create a spline interpolator object.
	 * This version creates a "natural" spline, with the second derivative
	 * equal to zero at the end points.
	 */
	template <typename InputIterator, typename InputIterator2>
	SplineInterpolator(InputIterator beginIndep, InputIterator endIndep,
			InputIterator2 beginDep, InputIterator2 endDep) : indeps(beginIndep, endIndep),
			deps(beginDep, endDep) {
		
		size = indeps.size();
		if(size != deps.size() || size == 0) {
			ready = false;
			return;
		}

		//calculate the value of the first derivative at the end points
		beginFirstDeriv = (deps[1] - deps[0]) / (indeps[1] - indeps[0]);
		endFirstDeriv = (deps[size - 1] - deps[size - 2]) / (indeps[size - 1] - indeps[size - 2]);
		
		secondDerivs.assign(size, 0);
		
		std::vector<T> u(size - 1);
		T sig, p;
		
		secondDerivs[0] = 0;
		u[0] = 0;
		
		for(unsigned int i = 1; i < size - 1; i++) {
			sig = (indeps[i] - indeps[i - 1]) / (indeps[i + 1] - indeps[i - 1]);
			p = sig * secondDerivs[i - 1] + 2.0;
			secondDerivs[i] = (sig - 1.0) / p;
			u[i] = (deps[i + 1] - deps[i]) / (indeps[i + 1] - indeps[i]) - (deps[i] - deps[i - 1]) / (indeps[i] - indeps[i - 1]);
			u[i] = (6.0 * u[i] / (indeps[i + 1] - indeps[i - 1]) - sig * u[i - 1]) / p;
		}

		//copy(u.begin(), u.end(), ostream_iterator<double>(cerr, " "));
		//cerr << endl;
		
		secondDerivs[size - 1] = 0;
		for(int i = size - 2; i >= 0; i--)
			secondDerivs[i] = secondDerivs[i] * secondDerivs[i + 1] + u[i];
		
		klo = 0;
		khi = size - 1;		
		
		ready = true;
	}
	
	//the spline interpolator object can be called just like a regular function
	T operator()(T x) const {
		if(x < indeps[klo]) { //lower bracket than last time
			if(x < indeps[0]) //before the beginning
				return deps[0] - beginFirstDeriv * (indeps[0] - x);
			else if(indeps[klo - 1] < x) { //it's the next one down
				klo--;
				khi--;
			} else //set low location as low as possible
				klo = 0;
		} else if(indeps[khi] < x) { //greater bracket than last time
			if(indeps[size - 1] < x) //beyond the end
				return deps[size - 1] + endFirstDeriv * (x - indeps[size - 1]);
			else if(x < indeps[khi + 1]) { //it's the next one up
				khi++;
				klo++;
			} else //set high location as high as possible
				khi = size - 1;
		} //else, it's the same bracket as last time, don't change bounds

		int k;
		while(khi - klo > 1) { //do binary search to find bracket
			k = (khi + klo) / 2;
			if(x < indeps[k])
				khi = k;
			else
				klo = k;
		}
		
		//return the interpolation of the function
		T h = indeps[khi] - indeps[klo];
		T A = (indeps[khi] - x) / h;
		T B = (x - indeps[klo]) / h;
		return A * deps[klo] + B * deps[khi] 
				+ h * h * (A * A * A - A) * secondDerivs[klo] / 6.0
				+ h * h * (B * B * B - B) * secondDerivs[khi] / 6.0;
	}
	
	bool isReady() const {
		return ready;
	}
};

/**
 * This class represents the derivative of a cubic-spline interpolated function.
 */
template <typename T = double>
class SplineDerivative {
private:
	
	bool ready;
	std::vector<T> indeps;
	std::vector<T> deps;
	std::vector<T> secondDerivs;
	T beginFirstDeriv;
	T endFirstDeriv;
	unsigned int size;
	mutable int klo, khi;
	
public:
	
	//default constructor, is non-working
	SplineDerivative() {
		ready = false;
	}
	
	SplineDerivative(const SplineInterpolator<T>& si) : indeps(si.indeps), deps(si.deps), 
			secondDerivs(si.secondDerivs), beginFirstDeriv(si.beginFirstDeriv),
			endFirstDeriv(si.endFirstDeriv) {
		
		size = indeps.size();
		if(size != deps.size()) {
			ready = false;
			return;
		}
		
		klo = 0;
		khi = size - 1;		
		
		ready = true;
	}
	
	//the spline derivative object can be called just like a regular function
	T operator()(T x) const {
		if(x < indeps[klo]) { //lower bracket than last time
			if(x < indeps[0]) //before the beginning
				return beginFirstDeriv;
			else if(indeps[klo - 1] < x) { //it's the next one down
				klo--;
				khi--;
			} else //set low location as low as possible
				klo = 0;
		} else if(indeps[khi] < x) { //greater bracket than last time
			if(indeps[size - 1] < x) //beyond the end
				return endFirstDeriv;
			else if(x < indeps[khi + 1]) { //it's the next one up
				khi++;
				klo++;
			} else //set high location as high as possible
				khi = size - 1;
		} //else, it's the same bracket as last time, don't change bounds

		int k;
		while(khi - klo > 1) { //do binary search to find bracket
			k = (khi + klo) / 2;
			if(x < indeps[k])
				khi = k;
			else
				klo = k;
		}
		
		//return the derivative of the interpolation of the function
		
		T h = indeps[khi] - indeps[klo];
		T A = (indeps[khi] - x) / h;
		T B = (x - indeps[klo]) / h;
		return (deps[khi] - deps[klo]) / h 
				- (3 * A * A * A - 1) * h * secondDerivs[klo] / 6
				+ (3 * B * B * B - 1) * h * secondDerivs[khi] / 6;
	}

	bool isReady() const {
		return ready;
	}
};

#endif //INTERPOLATE_H
