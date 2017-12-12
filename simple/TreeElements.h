#include "simple.decl.h"
#include "common.h"
#include "Utility.h"


class TreeElements : public CBase_TreeElements {
private:
  Vector3D<Real> moment;
  Real summass;
  bool ifTP;
  int waitcount;

public:
  TreeElements() {
    moment = Vector3D<Real> (0, 0, 0);
    summass = 0;
  }
  void exist(bool ifTPi) {
    ifTP = ifTPi;
    waitcount = (ifTP) ? 1 : 8;
  };
  void receiveData (Vector3D<Real> momenti, Real summassi);
  Vector3D<Real> centroid () {
    return moment / summass;
  }
};
