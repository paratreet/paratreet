#ifndef _PARATREET_LOADABLE_H_
#define _PARATREET_LOADABLE_H_

#include "Field.h"

namespace paratreet {
using parameter_map = std::map<std::string, Value>;

parameter_map load_parameters(const char* file);

class Loadable {
  std::vector<std::unique_ptr<GenericField>> fields_;

 public:

  Loadable(void) = default;
  Loadable(const Loadable&) {}

  template <typename T>
  inline void register_field(const std::string& name, const bool& reqd, T& storage) {
    this->fields_.emplace_back(new Field<T>(name, reqd, storage));
  }

  void load(const char* file);
};
}  // namespace paratreet

#endif
