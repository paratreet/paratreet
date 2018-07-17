#ifndef PARATREET_CONFIG_H
#define PARATREET_CONFIG_H

#include <climits>
#include <cstdint>

#ifndef PARATREET_FLOAT_TYPE
#define PARATREET_FLOAT_TYPE double
#endif

#ifndef PARATREET_KEY_TYPE
#define PARATREET_KEY_TYPE uint64_t
#endif

namespace paratreet {

typedef PARATREET_FLOAT_TYPE Float;

typedef PARATREET_KEY_TYPE Key;
const unsigned KEY_BIT = sizeof(Key) * CHAR_BIT;

} // paratreet

#endif