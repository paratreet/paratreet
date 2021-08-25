#ifndef _PARATREET_LOADABLE_HPP_
#define _PARATREET_LOADABLE_HPP_

#include "Field.hpp"

namespace paratreet {
using parameters = std::map<std::string, Value>;

parameters load_parameters(const char* file);

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
