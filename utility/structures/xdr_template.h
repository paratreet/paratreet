/** @file xdr_template.h
 Provides inline functions xdr_template() which perform
 XDR conversion of a value.  Numerous overloads
 of this function are provided for many common types, including
 Vector3D and OrientedBox (of arbitrary types).  If
 you attempt to use this function for a type that does not have
 a provided overload, you will get a compile time error.
 You can provide overloads for your own types by copying the
 syntax used below.
 This framework allows you to call xdr_template() whenever you wish
 to do XDR conversion, and not worry about the underlying calls
 to xdr_int, xdr_float, etc.
 @author Graeme Lufkin (gwl@u.washington.edu)
 @version 1.5
 */

#ifndef XDR_TEMPLATE_H
#define XDR_TEMPLATE_H

#include "config.h"

#include <assert.h>
#include <stdio.h>
#include <rpc/types.h>
#include <rpc/xdr.h>

#include "Vector3D.h"
#include "OrientedBox.h"

#ifndef HAVE_XDR_HYPER
/*
 * XDR hyper integers
 * same as xdr_u_hyper - open coded to save a proc call!
 */
static inline bool_t
xdr_hyper (XDR *xdrs, quad_t *llp)
{
  long t1;
  long t2;

  if (xdrs->x_op == XDR_ENCODE)
    {
      t1 = (long) ((*llp) >> 32);
      t2 = (long) (*llp);
      return (XDR_PUTLONG(xdrs, &t1) && XDR_PUTLONG(xdrs, &t2));
    }

  if (xdrs->x_op == XDR_DECODE)
    {
      if (!XDR_GETLONG(xdrs, &t1) || !XDR_GETLONG(xdrs, &t2))
	return FALSE;
      *llp = ((quad_t) t1) << 32;
      *llp |= t2;
      return TRUE;
    }

  if (xdrs->x_op == XDR_FREE)
    return TRUE;

  return FALSE;
}


/*
 * XDR hyper integers
 * same as xdr_hyper - open coded to save a proc call!
 */
static inline bool_t
xdr_u_hyper (XDR *xdrs, u_quad_t *ullp)
{
  unsigned long t1;
  unsigned long t2;

  if (xdrs->x_op == XDR_ENCODE)
    {
      t1 = (unsigned long) ((*ullp) >> 32);
      t2 = (unsigned long) (*ullp);
      return (XDR_PUTLONG(xdrs, &t1) && XDR_PUTLONG(xdrs, &t2));
    }

  if (xdrs->x_op == XDR_DECODE)
    {
      if (!XDR_GETLONG(xdrs, &t1) || !XDR_GETLONG(xdrs, &t2))
	return FALSE;
      *ullp = ((u_quad_t) t1) << 32;
      *ullp |= t2;
      return TRUE;
    }

  if (xdrs->x_op == XDR_FREE)
    return TRUE;

  return FALSE;
}
#endif

inline bool_t xdr_template(XDR* xdrs, unsigned char* val) {
	return xdr_u_char(xdrs, val);
}

inline bool_t xdr_template(XDR* xdrs, char* val) {
	return xdr_char(xdrs, val);
}

inline bool_t xdr_template(XDR* xdrs, unsigned short* val) {
	return xdr_u_short(xdrs, val);
}

inline bool_t xdr_template(XDR* xdrs, short* val) {
	return xdr_short(xdrs, val);
}

inline bool_t xdr_template(XDR* xdrs, unsigned int* val) {
	return xdr_u_int(xdrs, val);
}

inline bool_t xdr_template(XDR* xdrs, int* val) {
	return xdr_int(xdrs, val);
}

inline bool_t xdr_template(XDR* xdrs, u_int64_t* val) {
#ifdef _LONG_LONG
	return xdr_u_hyper(xdrs, (unsigned long long *)val);
#else
#ifdef HAVE_BOTH_INT64_AND_QUAD_T
	return xdr_u_hyper(xdrs, (u_quad_t *)val);
#else
	return xdr_u_hyper(xdrs, val);
#endif
#endif
}

inline bool_t xdr_template(XDR* xdrs, int64_t* val) {
#ifdef _LONG_LONG
	return xdr_hyper(xdrs, (long long *)val);
#else
#ifdef HAVE_BOTH_INT64_AND_QUAD_T
	return xdr_hyper(xdrs, (quad_t *)val);
#else
	return xdr_hyper(xdrs, val);
#endif
#endif
}

inline bool_t xdr_template(XDR* xdrs, float* val) {
	return xdr_float(xdrs, val);
}

inline bool_t xdr_template(XDR* xdrs, double* val) {
	return xdr_double(xdrs, val);
}

template <typename T>
inline bool_t xdr_template(XDR* xdrs, Vector3D<T>* val) {
	return (xdr_template(xdrs, &(val->x))
			&& xdr_template(xdrs, &(val->y))
			&& xdr_template(xdrs, &(val->z)));
}

template <typename T>
inline bool_t xdr_template(XDR* xdrs, OrientedBox<T>* val) {
	return (xdr_template(xdrs, &(val->lesser_corner))
			&& xdr_template(xdrs, &(val->greater_corner)));
}

#endif //XDR_TEMPLATE_H
