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

  virtual void accept(const char*) = 0;
  virtual bool takes_value(void) const = 0;

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

  virtual bool takes_value(void) const override {
    return true;
  }

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

template <>
class Field<bool> : public GenericField {
  bool& storage_;
  bool value_;

 public:
  Field(const std::string& name, const char* arg, bool& storage, bool value)
      : GenericField(name, arg), storage_(storage), value_(value) {}

  virtual bool takes_value(void) const override {
    return false;
  }

  virtual void accept(const char* val) override {
    if (val) {
      this->storage_ = FieldConverter<bool>()(val);
    } else {
      this->storage_ = this->value_;
    }
  }
};
}  // namespace paratreet

#endif
