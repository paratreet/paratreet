#include "Loadable.h"

#include <fstream>
#include <regex>

namespace paratreet {

parameters load_parameters(const char* file) {
  parameters map;
  std::fstream fs(file, std::ios::in);
  CmiAssert(fs.is_open());

  std::string line;
  std::regex line_regex(
      "([_a-zA-Z0-9]+)\\s*=\\s*([^\\s#]+).*");  // matches (id) = (value)
  while (std::getline(fs, line)) {
    // skip comments and empty lines
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::smatch m;
    // try to match the line
    if (std::regex_match(line, m, line_regex)) {
      // extract key and value if successful
      auto& key = m[1];
      auto& val = m[2];
      // determine the type from the key
      auto ty = value::get_type(key);
      // emplace the value with the appropriate conversion
      auto ins = ([&](void) {
        using T = value::type;
        switch (ty) {
          case T::kBool:
            return map.emplace(key, (bool)std::stoi(val));
          case T::kDouble:
            return map.emplace(key, std::stod(val));
          case T::kInteger:
            return map.emplace(key, std::stoi(val));
          case T::kString:
            return map.emplace(key, val);
          default:
            return std::make_pair(std::end(map), false);
        }
      })();
      // validate that a correct insertion occured
      if (!ins.second) {
        CmiAbort("insertion did not occur; was parameter %s defined twice?\n",
                 (key.str()).c_str());
      }
      CmiEnforce((ins.first->second).is_type(ty));
    }
  }

  fs.close();

  return std::move(map);
}

void Loadable::load(const char* file) {
  auto params = load_parameters(file);

  for (auto& field : this->fields_) {
    auto& name = field->name();
    auto search = params.find(name);
    if (search != std::end(params)) {
      field->accept(search->second);
    } else if (field->required()) {
      CmiAbort("fatal> missing required parameter %s!", name.c_str());
    }
  }
}
}  // namespace paratreet
