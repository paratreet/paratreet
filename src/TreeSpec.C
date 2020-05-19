#include "TreeSpec.h"

void TreeSpec::receiveDecomposition(CkMarshallMsg* msg) {
  char *buffer = msg->msgBuf;
  PUP::fromMem pupper(buffer);
  PUP::detail::TemporaryObjectHolder<CkCallback> cb;
  this->getDecomposition()->pup(pupper);
  pupper | cb;
  contribute(cb.t);
}

Decomposition* TreeSpec::getDecomposition() {
  if (!decomp) {
    if (decomp_type == OCT_DECOMP) {
      decomp.reset(new OctDecomposition());
    } else if (decomp_type == SFC_DECOMP) {
      decomp.reset(new SfcDecomposition());
    }
  }

  return decomp.get();
}
