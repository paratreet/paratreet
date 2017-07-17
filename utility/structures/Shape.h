/** \file Shape.h
 \author Graeme Lufkin (gwl@u.washington.edu)
 \date Created November 8, 2001
 \version 1.1
 */

#ifndef SHAPE_H
#define SHAPE_H

/// An abstract base class defining a geometric shape in three dimensions
template <class T>
class Shape {
public:
	
	Shape() { }
	virtual ~Shape() { }
		
	/// What is the volume enclosed by this shape?
	virtual T volume() const = 0;
	
};

#endif //SHAPE_H
