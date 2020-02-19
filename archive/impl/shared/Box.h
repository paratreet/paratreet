#ifndef PARATREET_SHARED_BOX_H
#define PARATREET_SHARED_BOX_H

#include "Vector.h"

#include <algorithm>

namespace paratreet {

template <unsigned D>
struct Box {
    Vector<D> start, end;
    bool defined;

    Box() : defined(false) {}
    Box(const Vector<D>& start, const Vector<D>& end) : start(start), end(end), defined(true) {}

    bool contains(const Vector<D>& p) const {
        for (unsigned i = 0; i < D; i++) {
            if (p[i] < start[i] || p[i] > end[i]) return false;
        }
        return true;
    }

    void add(const Vector<D>& p) {
        if (defined) {
            for (unsigned i = 0; i < D; i++) {
                start[i] = std::min(start[i], p[i]);
                end[i]   = std::max(end[i],   p[i]);
            }
        } else {
            start = end = p;
            defined = true;
        }
    }

    const Box<D>& operator+=(const Box<D>& rhs) {
        if (defined && rhs.defined) {
            for (unsigned i = 0; i < D; i++) {
                start[i] = std::min(start[i], rhs.start[i]);
                end[i]   = std::max(end[i]  , rhs.end[i]);
            }
            return *this;
        } else if (rhs.defined) {
            *this = rhs;
            return *this;
        } else {
            return *this;
        }
    }
};

template <class OutStream, unsigned D>
inline OutStream& operator<<(OutStream& os, const Box<D>& box)
{ return os << "Box(" << box.start << ',' << box.end << ')'; }

} // paratreet

#endif