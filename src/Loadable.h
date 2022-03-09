#ifndef _PARATREET_LOADABLE_H_
#define _PARATREET_LOADABLE_H_

#include "Field.h"

#include <memory>
#include <vector>

namespace paratreet {
class Loadable {
 protected:
  using field_type = std::unique_ptr<GenericField>;
  std::vector<field_type> fields_;
  const char* config_arg_; 

 public:

  Loadable(void)
  : config_arg_(nullptr) {}

  Loadable(const char* config_arg)
  : config_arg_(config_arg) {}

  Loadable(const Loadable&) {}

  inline void register_field(const std::string& name, const char* arg, bool& storage, bool value = true) {
    storage = !value; // reset default value

    this->fields_.emplace_back(new Field<bool>(name, arg, storage, value));
  }

  template <typename T>
  inline void register_field(const std::string& name, const char* arg, T& storage) {
    this->fields_.emplace_back(new Field<T>(name, arg, storage));
  }

  FieldOrigin origin_of(const std::string& name) const;

  void load(const char* file);

  void parse(int&, char**);

  void pup(PUP::er& p) {
    // p | this->config_arg_;
    p | this->fields_;
  }
};
}  // namespace paratreet

#endif
