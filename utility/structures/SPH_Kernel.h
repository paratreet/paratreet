/** @file SPH_Kernel.h
 @author Graeme Lufkin (gwl@u.washington.edu)
 @date Created Summer 2002
 @version 1.0
 */

#ifndef SPH_KERNEL_H
#define SPH_KERNEL_H

namespace SPH {

class Kernel {
public:
	/** Evaluate the kernel for a given distance r with a given smoothing length h.
	 */
	virtual double evaluate(double r, double h) const = 0;
	
	/** Evaluate the gradient (almost) of the kernel for a given distance r with a given smoothing length h.
	 To get the full gradient, multiply the result of this function by the vector whose magnitude is r.
	 */
	virtual double evaluateGradient(double r, double h) const = 0;
	
	/** Evaluate the projected kernel for a given distance r with a given smoothing length h.
	 */
	virtual double evaluateProjection(double r, double h) const = 0;
	
	
	/** Evaluate the softened value of 1/r for use in the gravitational potential
	 for a mass distributed with this kernel. \Phi = -G m_j evaluatePotential(r, \epsilon)
	 */
	virtual double evaluatePotential(double r, double epsilon) const = 0;
	
	/** Evaluate the softened value of 1/r^3 for use in the gravitational acceleration
	 for a mass distributed with this kernel. \vec{a} = -m \vec{r} evaluateAcceleration(r, \epsilon)
	 */
	virtual double evaluateAcceleration(double r, double epsilon) const = 0;
	virtual ~Kernel() {}
};

static const double spline_PI = 3.14159265358979323846;
/** The standard cubic kernel used in SPH calculations.
 */
class SplineKernel : public Kernel {
	
	inline static double f(double x) {
		return (0.5 + 13.0 / 4 * x * x) * sqrt(1 - x * x) - (3 + 3.0 / 4 * x * x) * x * x * log((1 + sqrt(1 - x * x)) / x);
	}
public:

	inline double evaluate(double r, double h) const {
		double q = r / h;
		if(q < 1)
			return (1 - 1.5 * q * q + 0.75 * q * q * q) / spline_PI / h / h / h;
		else if(q < 2)
			return 0.25 * (2 - q) * (2 - q) * (2 - q) / spline_PI / h / h / h;
		else
			return 0;
	}
	
	inline double evaluateGradient(double r, double h) const {
		double q = r / h;
		if(q < 1)
			return (0.75 * q - 1) * 3 / spline_PI / h / h / h / h / h;
		else if(q < 2)
			return (-0.25 * q - 1 / q + 1) * 3 / spline_PI / h / h / h / h / h;
		else
			return 0;
	}
	
	inline double evaluateProjection(double r, double h) const {
		double q = r / h;
		if(q < 1) {
			if(q == 0)
				return 3.0 / 2 / spline_PI / h / h;
			else
				return (4 * f(q / 2) - f(q)) / spline_PI / h / h;
		} else if(q < 2)
			return 4 * f(q / 2) / spline_PI / h / h;
		else
			return 0;
	}
	
	inline double evaluatePotential(double r, double epsilon) const {
		double q = r / epsilon;
		if(q < 1)
			return (7.0 / 5 - q * q * (2.0 / 3 - q * q / 10 * (3 - q))) / epsilon;
		else if(q < 2)
			return (-1.0 / 15 / q + 8.0 / 5 - q * q * (4.0 / 3 - q * (1 - q / 10 * (3 - q / 3)))) / epsilon;
		else
			return  1 / r;
	}

	inline double evaluateAcceleration(double r, double epsilon) const {
		double q = r / epsilon;
		if(q < 1)
			return (4.0 / 3 - q * q / 2 * (12.0 / 5 - q)) / epsilon / epsilon / epsilon;
		else if(q < 2)
			return (-1.0 / 15 / q / q / q + 8.0 / 3 - q * (3 - q * (6.0 / 5 - q / 6))) / epsilon / epsilon / epsilon;
		else
			return  1 / r / r / r;
	}
};

} //close namespace SPH

#endif //SPH_KERNEL_H
