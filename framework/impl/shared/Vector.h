#ifndef PARATREET_SHARED_VECTOR_H
#define PARATREET_SHARED_VECTOR_H

#include <cmath>
#include <ostream>

#include "../config.h"

namespace paratreet {

template <unsigned D>
struct Vector {
    Float data[D];

    Float& operator[](unsigned i)               { return data[i]; }
    const Float& operator[](unsigned i) const   { return data[i]; }

    const Vector<D>& operator+=(const Vector<D>& rhs) {
        for (unsigned i = 0; i < D; i++) data[i] += rhs[i];
        return *this;
    }

    const Vector<D>& operator*=(Float a) {
        for (unsigned i = 0; i < D; i++) data[i] *= a;
        return *this;
    }

    Vector<D> operator-() {
        Vector<D> v;
        for (unsigned i = 0; i < D; i++) v[i] = -data[i];
        return v;
    }

    Vector<D> operator+(const Vector<D>& rhs) {
        Vector<D> v;
        for (unsigned i = 0; i < D; i++) v[i] = data[i] + rhs[i];
        return v;
    }

    Vector<D> operator-(const Vector<D>& rhs) {
        Vector<D> v;
        for (unsigned i = 0; i < D; i++) v[i] = data[i] - rhs[i];
        return v;
    }

    Vector<D> operator*(Float a) {
        Vector<D> v;
        for (unsigned i = 0; i < D; i++) v[i] = data[i] * a;
        return v;
    }

    Float length() const { return std::sqrt(lengthSq()); }
    Float lengthSq() const {
        Float sum = 0;
        for (unsigned i = 0; i < D; i++) sum += data[i] * data[i];
        return sum;
    }
};

template <class OutStream, unsigned D>
inline OutStream& operator<<(OutStream& os, const Vector<D>& v) {
    os << '[';
    for (unsigned i = 0; i < D; i++) {
        if (i) os << ' ';
        os << v[i];
    }
    os << ']';
    return os;
}

} // paratreet

#endif