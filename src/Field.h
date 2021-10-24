#ifndef __PARATREET_FIELD_H__
#define __PARATREET_FIELD_H__

#include "Value.h"

namespace paratreet {
class GenericField {
  std::string name_;
  bool required_;

 public:
  GenericField(const std::string& name, const bool& reqd)
      : name_(name), required_(reqd) {}

  const std::string& name(void) const { return this->name_; }
  const bool& required(void) const { return this->required_; }

  virtual void accept(const Value& val) = 0;
};

template <typename T>
class Field : public GenericField {
  T& storage_;

 public:
  Field(const std::string& name, const bool& reqd, T& storage)
      : GenericField(name, reqd), storage_(storage) {}

  virtual void accept(const Value& val) override {
    new (&this->storage_) T((T)val);
  }
};
}  // namespace paratreet

#endif
