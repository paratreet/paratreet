#ifndef SIMPLE_BUFFERED_VEC_H_
#define SIMPLE_BUFFERED_VEC_H_

#include "common.h"

template<typename T>
class BufferedVec {
  CkVec<T>* new_vec;
  CkVec<T>* buf_vec;

  public:
  BufferedVec(){
    new_vec = new CkVec<T>;
    buf_vec = new CkVec<T>;
  }

  void add(T &t){
    new_vec->push_back(t);
  }

  void add(T t){
    new_vec->push_back(t);
  }

  void buffer(){
    CkVec<T>* tmp = new_vec;
    new_vec = buf_vec;;
    buf_vec = tmp;
    new_vec->resize(0);
  }

  T &get(int i){
    return (*buf_vec)[i];
  }

  CkVec<T>& get(){
    return *buf_vec;
  }

  int size(){
    return buf_vec->size();
  }

  ~BufferedVec(){
    delete new_vec;
    delete buf_vec;
  }
};

#endif // SIMPLE_BUFFERED_VEC_H_
