/** \file TypeSelection.h
 \author Graeme Lufkin (gwl@u.washington.edu)
 \date Created August 18, 2003
 \version 1.0
 */

#ifndef TYPESELECTION_H
#define TYPESELECTION_H

/** A template structure that provides a typedef.
 If the flag template parameter is true, the typedef is of the first
 type, if false of the second.  By referring to 
 Select<condition, T, U>::Result you can choose between the types
 T and U based on the boolean condition. 
 This trick is taken from Andrei Alexandrescu's Modern C++ Design.*/
template <bool flag, typename T, typename U>
struct Select {
	typedef T Result;
};

template <typename T, typename U>
struct Select<false, T, U> {
	typedef U Result;
};

/** A template structure that provides a typedef of the wider type.
 The larger of the two template parameters is given the widerType typedef. */
template <typename T, typename T2>
struct wider {
	typedef typename Select<sizeof(T) >= sizeof(T2), T, T2>::Result widerType;
};

#endif //TYPESELECTION_H
