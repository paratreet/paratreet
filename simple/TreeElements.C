#include "TreeElements.h"

void TreeElements::receiveData (Vector3D<Real> momenti, Real summassi) {
  moment += momenti;
  summass += summassi;
  waitcount--;
  if (waitcount == 0) {
    thisProxy[thisIndex >> 3].receiveData(moment, summass); // pass it up
  }
}
