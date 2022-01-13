#include "Loadable.h"

#include <fstream>
#include <ctype.h>
#include <charm++.h>
#include <map>

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

static void compress_arguments_(int& argc, char** argv) {
  auto find = [&](int start, int end) {
    int i;
    for (i = start; ((i < end) && (argv[i] != nullptr)); i++);
    return i;
  };

  auto nullPos = find(0, argc);
  int nullCount = (nullPos != argc);

#if DEBUG
  std::stringstream ss;
  ss << "args> FIRST null found at " << nullPos << std::endl;
#endif

  for (auto i = (nullPos + 1); i < argc; i++) {
    if (argv[i] == nullptr) {
      nullCount += 1;
#if DEBUG
      ss << "args> null found at " << i << std::endl;
#endif
    } else {
      CkAssert(nullPos != argc);
      std::swap(argv[nullPos], argv[i]);
      // we seek up to the current position
      // since it definitely has a null!
      nullPos = find(nullPos + 1, i + 1);
    }
  }

  argc -= nullCount;

#if DEBUG
  ss << "there are " << argc << " compressed args after filtering out " \
     << nullCount << " nulls:" << std::endl;
  for (auto i = 0; i < argc; i++) {
    ss << argv[i] << " ";
  }

  CkPrintf("%s\n", ss.str().c_str());
#endif
}

void Loadable::parse(int& argc, char** argv) {
  std::map<std::string, field_type&> args;
  using iterator = typename decltype(args)::iterator;

  for (auto& field : this->fields_) {
    auto* arg = field->arg();
    if (arg) {
      args.emplace("-" + std::string(arg), field);
    }
  }

  // Seek configuration files ahead of the rest of the arguments
  // (to ensure they are overriden by command-line arguments!)
  if (this->config_arg_) {
    for (auto i = 1; i < argc; i += 1) {
      if (strcmp(argv[i], this->config_arg_) == 0) {
        argv[i] = nullptr;
        this->load(argv[++i]);
        argv[i] = nullptr;
      }
    }
  }

  for (auto i = 1; i < argc; i++) {
    iterator search;
    if (argv[i] == nullptr ||
      ((search = args.find(argv[i])) == std::end(args))) {
      continue;
    } else {
      // TODO ( currently assumes all flags have a value  )
      //      ( but boolean flags may be store true/false )
      auto& field = search->second;
      argv[i] = nullptr;
      field->accept(argv[++i]);
      argv[i] = nullptr;
    }
  }

  compress_arguments_(argc, argv);
}

using parameter_map = std::map<std::string, std::string>;

static parameter_map load_parameters_(const char* file) {
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
      auto ins = map.emplace(key, val);
      // validate that a correct insertion occured
      if (!ins.second) {
        CmiAbort("parameter %s defined twice in %s.", key.c_str(), file);
      }
    }
  }

  fs.close();

  return std::move(map);
}

void Loadable::load(const char* file) {
  auto params = load_parameters_(file);

  for (auto& field : this->fields_) {
    auto& name = field->name();
    auto search = params.find(name);
    if (search != std::end(params)) {
      field->accept((search->second).c_str());

      params.erase(search);
    }
  }

  for (auto& param : params) {
    CmiError("warning> unused parameter %s.", param.first.c_str());
  }
}
}  // namespace paratreet
