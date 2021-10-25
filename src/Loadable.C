#include "Loadable.h"

#include <fstream>
#include <ctype.h>

namespace paratreet {

bool split_line_(const std::string& line, std::string& left,
                 std::string& right) {
  char first = '\0';
  bool found = false;

  for (auto i = 0; i < line.size(); i += 1) {
    auto& curr = line[i];
    if (isspace(curr)) {
      continue;
    } else if (first == '\0') {
      first = curr;
    }

    if (line[i] == '=') {
      found = true;
    } else if (found) {
      right += curr;
    } else {
      left += curr;
    }
  }

  return found && (first != '#');
}

parameter_map load_parameters(const char* file) {
  parameter_map map;
  std::fstream fs(file, std::ios::in);
  CmiAssert(fs.is_open());

  std::string line;
  while (std::getline(fs, line)) {
    // skip comments and empty lines
    if (line.empty() || line[0] == '#') {
      continue;
    }
    std::string key, val;
    // try to match the line, if successful:
    if (split_line_(line, key, val)) {
      // determine the type from the key
      auto ty = Value::get_type(key);
      // emplace the value with the appropriate conversion
      auto ins = ([&](void) {
        using T = Value::type;
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
                 key.c_str());
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

      params.erase(search);
    } else if (field->required()) {
      CmiAbort("fatal> missing required parameter %s!", name.c_str());
    }
  }

  for (auto& param : params) {
    CmiError("warning> unused parameter %s.", param.first.c_str());
  }
}
}  // namespace paratreet
