#ifndef SIMPLE_DATAINTERFACE_H_
#define SIMPLE_DATAINTERFACE_H_

#include <map>
//#include "simple.decl.h"
#include "common.h"
//#include "TreeElement.h"
#include "Utility.h"

template <typename Visitor, typename Data>
class CProxy_TreeElement;

template <typename Data>
class CProxy_TreePiece;

template <typename Visitor, typename Data>
class DataInterface {
private:
  CProxy_TreeElement<Visitor, Data> global_data;
  std::map<Key, Data> local_data;
  Key tp_key;
public:
  DataInterface<Visitor, Data> () {}
  DataInterface<Visitor, Data> (CProxy_TreeElement<Visitor, Data> global_datai, std::map<Key, Data>& local_datai, Key tp_keyi) :
    global_data(global_datai), local_data(local_datai), tp_key(tp_keyi) {}
  bool contribute(Key key, Data data) {
    if (key == tp_key >> 3) {
      global_data[key].receiveData(data, true, *this);
      return false;
    }
    else if (Utility::isPrefix(tp_key, key)) {
      local_data[key] = local_data[key] + data;
      return true;
    }
    return false;
  }
  void pup(PUP::er& p) {
    p | global_data;
    p | local_data;
    p | tp_key;
  }
};
#endif // SIMPLE_DATAINTERFACE_H_
