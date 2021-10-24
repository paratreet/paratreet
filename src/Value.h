#ifndef __PARATREET_PARAMETER_H__
#define __PARATREET_PARAMETER_H__

#include <pup_stl.h>

namespace paratreet {
struct Value {
  enum type : char {
    kBool = 'b',
    kDouble = 'd',
    kInteger = 'i',
    kString = 's'
  };

  static type get_type(const std::string& s) {
    CmiAssert(!s.empty());
    if (s.rfind("ach", 0) == 0) {
      return kString;
    } else if (s[0] == 'n') {
      return kInteger;
    } else {
      return (type)s[0];
    }
  }

 private:
  type type_;
  union u_value_ {
    bool b;
    double d;
    int i;
    std::string s;

    u_value_(void) = delete;
    u_value_(const u_value_&) = delete;
    ~u_value_() {}

    u_value_(PUP::reconstruct) {}

    u_value_(const bool& b_) : b(b_) {}
    u_value_(const double& d_) : d(d_) {}
    u_value_(const int& i_) : i(i_) {}
    u_value_(const std::string& s_) : s(s_) {}
  } value_;

  template <typename T>
  static inline void destruct(const T& t) {
    t.~T();
  }

 public:
  // copy the value of another value
  Value(const Value& other)
  : type_(other.type_), value_(PUP::reconstruct()) {
    switch (this->type_) {
      case kBool:
        new (&this->value_) u_value_((bool)other);
        break;
      case kDouble:
        new (&this->value_) u_value_((double)other);
        break;
      case kInteger:
        new (&this->value_) u_value_((int)other);
        break;
      case kString:
        new (&this->value_) u_value_((std::string)other);
        break;
    }
  }

  // used for deserialization
  Value(PUP::reconstruct tag) : value_(tag) {}

  // constructs a Value of a given type
  Value(const bool& b) : type_(kBool), value_(b) {}
  Value(const double& d) : type_(kDouble), value_(d) {}
  Value(const int& i) : type_(kInteger), value_(i) {}
  Value(const std::string& s) : type_(kString), value_(s) {}

  // calls the appropriate destructor
  ~Value() {
    switch (this->type_) {
      case kBool:
        destruct(this->value_.b);
        break;
      case kDouble:
        destruct(this->value_.d);
        break;
      case kInteger:
        destruct(this->value_.i);
        break;
      case kString:
        destruct(this->value_.s);
        break;
    }
  }

  inline const type& get_type(void) const { return this->type_; }

  inline bool is_type(const type& ty) const { return ty == this->type_; }

  // checks whether lhs and rhs have the same type and Value
  bool operator==(const Value& rhs) const {
    const auto& lhs = *this;
    return (lhs.type_ == rhs.type_) && ([&](void) -> bool {
             switch (lhs.type_) {
               case kBool:
                 return lhs.value_.b == rhs.value_.b;
               case kDouble:
                 return lhs.value_.d == rhs.value_.d;
               case kInteger:
                 return lhs.value_.i == rhs.value_.i;
               case kString:
                 return lhs.value_.s == rhs.value_.s;
             }
           })();
  }

  void pup(PUP::er& p) {
    auto& ty = reinterpret_cast<char&>(this->type_);
    p | ty;

    switch (this->type_) {
      case kBool:
        p | this->value_.b;
        break;
      case kDouble:
        p | this->value_.d;
        break;
      case kInteger:
        p | this->value_.i;
        break;
      case kString: {
        auto& s = this->value_.s;
        if (p.isUnpacking()) {
          new (&s) std::string();
        }
        p | s;
        break;
      }
    }
  }

  // converts this Value's Value to a string (e.g., for output)
  std::string to_string(void) const {
    switch (this->type_) {
      case kBool:
        return std::to_string(this->value_.b);
      case kDouble:
        return std::to_string(this->value_.d);
      case kInteger:
        return std::to_string(this->value_.i);
      case kString:
        return this->value_.s;
      default:
        return "(null)";
    }
  }

  // check whether a Value can be cast to a given type
  template <typename T>
  bool is(void) const {
    return false;
  }

  // cast this Value to the specified type
  explicit operator bool() const;
  explicit operator double() const;
  explicit operator int() const;
  explicit operator std::string() const;
};

template <>
inline bool Value::is<bool>(void) const {
  return this->type_ == kBool;
}

inline Value::operator bool() const {
  CmiAssert(this->is<bool>());
  return this->value_.b;
}

template <>
inline bool Value::is<double>(void) const {
  return this->type_ == kDouble;
}

inline Value::operator double() const {
  CmiAssert(this->is<double>());
  return this->value_.d;
}

template <>
inline bool Value::is<int>(void) const {
  return this->type_ == kInteger;
}

inline Value::operator int() const {
  CmiAssert(this->is<int>());
  return this->value_.i;
}

template <>
inline bool Value::is<std::string>(void) const {
  return this->type_ == kString;
}

inline Value::operator std::string() const {
  CmiAssert(this->is<std::string>());
  return this->value_.s;
}
}  // namespace paratreet

#endif
