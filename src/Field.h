#ifndef __PARATREET_FIELD_H__
#define __PARATREET_FIELD_H__

#include <sstream>
#include <cstring>
#include <pup_stl.h>

namespace paratreet {

enum class FieldOrigin : std::uint8_t {
  CommandLine, File, Unknown
};

class GenericField {
  std::string name_;
  const char* arg_;
  FieldOrigin origin_;

 public:
  GenericField(const std::string& name, const char* arg)
      : name_(name), arg_(arg), origin_(FieldOrigin::Unknown) {}

  GenericField(void)
      : arg_(nullptr), origin_(FieldOrigin::Unknown) {}

  const char* arg(void) const { return this->arg_; }
  const std::string& name(void) const { return this->name_; }

  virtual void accept(const char*) {
    CmiAbort(
      "errant attempt to set value of generic field %s",
      this->name_.c_str()
    );
  }

  virtual bool takes_value(void) const {
    return false;
  }

  virtual bool required(void) const { return false; }

  FieldOrigin& origin(void) {
    return this->origin_;
  }

  FieldOrigin origin(void) const {
    return this->origin_;
  }

  virtual void pup(PUP::er& p) {
    p | this->name_;
    // p | this->arg_;
    p | reinterpret_cast<std::uint8_t&>(this->origin_);
  }

  virtual operator std::string(void) const {
    std::stringstream ss;
    ss << "GenericField(name=\"" << this->name_ << "\"";
    switch (this->origin_) {
      case FieldOrigin::CommandLine:
        ss << ",origin=CLI";
        break;
      case FieldOrigin::File:
        ss << ",origin=FILE";
        break;
      case FieldOrigin::Unknown:
        break;
    }
    ss << ")";
    return ss.str();
  }
};

template<typename T>
struct FieldConverter;

template <typename T>
class Field : public GenericField {
  T* storage_;

 public:
  Field(const std::string& name, const char* arg, T& storage)
      : GenericField(name, arg), storage_(&storage) {}

  Field(void) : GenericField(), storage_(nullptr) {}

  virtual bool takes_value(void) const override {
    return true;
  }

  virtual void accept(const char* val) override {
    FieldConverter<T> op;
    new (this->storage_) T(op(val));
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
  bool* storage_;
  bool value_;

 public:
  Field(const std::string& name, const char* arg, bool& storage, bool value)
      : GenericField(name, arg), storage_(&storage), value_(value) {}

  Field(void) : GenericField() {}

  virtual void accept(const char* val) override {
    if (val) {
      *(this->storage_) = FieldConverter<bool>()(val);
    } else {
      *(this->storage_) = this->value_;
    }
  }
};
}  // namespace paratreet

#endif
