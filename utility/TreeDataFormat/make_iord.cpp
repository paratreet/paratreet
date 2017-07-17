// Create an iorder field file

#include <iostream>
#include <limits>
#include "tree_xdr.h"
#include "TypeHandling.h"

using namespace TypeHandling;

int main(int argc, char** argv) {
    if(argc < 3) {
        std::cerr << "Usage: make_iord istart nbodies" << std::endl;
        return 1;
        }
    int64_t istart = atoll(argv[1]);
    int64_t nbodies = atoll(argv[2]);

    XDR xdrsOut;
    xdrstdio_create(&xdrsOut, stdout, XDR_ENCODE);
    FieldHeader fho;
    fho.dimensions = 1;
    fho.code = uint64;
    fho.numParticles = nbodies;
    fho.time = 0.0;
    
    if(!xdr_template(&xdrsOut, &fho)) {
        std::cerr << "Couldn't write output header" << std::endl;
        return 2;
	}
    int64_t max = istart+nbodies;
    assert(xdr_template(&xdrsOut, &istart) == 1);
    assert(xdr_template(&xdrsOut, &max) == 1);
    
    for(int64_t i = 0; i < nbodies; i++) {
        assert(xdr_template(&xdrsOut, &istart) == 1);
        istart++;
        }

    xdr_destroy(&xdrsOut);
}
