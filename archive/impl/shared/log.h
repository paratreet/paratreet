#ifndef PARATREET_SHARED_LOG_H
#define PARATREET_SHARED_LOG_H

namespace paratreet { namespace log {

template <typename T>
void print(T arg0) { ckerr << arg0; }

template <typename T, typename... Ts>
void print(T arg0, Ts... args) { print(arg0); print(args...); }

template <typename... Ts>
void println(Ts... args) { print(args..., endl); }

template <typename... Ts>
void info(Ts... args) { println("ParaTreet> ", args...); }

template <typename... Ts>
void debug(Ts... args) {
    #ifdef DEBUG
    println("ParaTreet DEBUG> ", args...);
    #endif
}

#define CINFO(class) #class
#define AINFO(class) #class " ", this->thisIndex, ": "

} } // paratreet::log

#include <string>
inline CkErrStream& operator<<(CkErrStream& os, const std::string str) {
    return os << str.c_str();
}

#endif