#ifndef __PARATREET_FIELD_H__
#define __PARATREET_FIELD_H__

#include <string>
#include <cstring>

namespace paratreet {
class GenericField {
  std::string name_;
  const char* arg_;

 public:
  GenericField(const std::string& name, const char* arg)
      : name_(name), arg_(arg) {}

  const char* arg(void) const { return this->arg_; }
  const std::string& name(void) const { return this->name_; }

  virtual void accept(const char* val) = 0;
  virtual bool required(void) const { return false; }
};

template<typename T>
struct FieldConverter;

template <typename T>
class Field : public GenericField {
  T& storage_;

 public:
  Field(const std::string& name, const char* arg, T& storage)
      : GenericField(name, arg), storage_(storage) {}

  virtual void accept(const char* val) override {
    FieldConverter<T> op;
    new (&this->storage_) T(op(val));
  }
};

template<>
struct FieldConverter<double> {
  double operator()(const char* val) {
    return atof(val);
  }
};

template<>
struct FieldConverter<int> {
  int operator()(const char* val) {
    return atoi(val);
  }
};

template<>
struct FieldConverter<bool> {
  bool operator()(const char* val) {
    return (bool)atoi(val);
  }
};

template<>
struct FieldConverter<std::string> {
  std::string operator()(const char* val) {
    return std::string(val);
  }
};
}  // namespace paratreet

#endif
