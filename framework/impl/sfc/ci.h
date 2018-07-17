#ifndef PARATREET_SFC_CI_H
#define PARATREET_SFC_CI_H

#define PARATREET_CI_DECLARE_SFC(Meta) \
    namespace paratreet { namespace sfc { \
    group DataManager<Meta>; \
    array [1d] TreePiece<Meta>; \
    } }

#endif