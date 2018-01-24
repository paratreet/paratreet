#ifndef SIMPLE_BUFFERED_VEC_H_
#define SIMPLE_BUFFERED_VEC_H_

#include "common.h"

template<typename T>
class BufferedVec {
  std::vector<T>* new_vec;
  std::vector<T>* buf_vec;

  public:
  BufferedVec(){
    new_vec = new std::vector<T>;
    buf_vec = new std::vector<T>;
  }

  void add(T &t){
    new_vec->push_back(t);
  }

  void add(T t){
    new_vec->push_back(t);
  }

  void buffer(){
    std::vector<T>* tmp = new_vec;
    new_vec = buf_vec;;
    buf_vec = tmp;
    new_vec->resize(0);
  }

  T &get(int i){
    return (*buf_vec)[i];
  }

  std::vector<T>& get(){
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
