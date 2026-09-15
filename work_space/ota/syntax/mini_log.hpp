#pragma once
#include <iostream>
#include <mutex>
#include <sstream>
#include <string>
#include <string_view>
#include <utility>

namespace mini{
    template <typename T>
    std::string toStr(const T& value) {
        std::ostringstream oss;
        oss << value;
        return oss.str();
    }


inline std::string toStr(bool value) {
    return value ? "true" : "false";
}
inline std::string toStr(std::string_view value) {
    return std::string(value);
}

inline std::string toStr(const char *value) {
    return value ? std::string(value) : "(null)";
}

inline std::string toStr(signed char value) {
    return std::to_string(static_cast<int>(value));
}
inline std::string toStr(unsigned char value){
    return std::to_string(static_cast<int>(value));
}

inline void replaceFirstPlaceholder(std::string &out,std::size_t &pos,const std::string& value){
    const auto p = out.find("{}",pos);
    if(p == std::string::npos) {
        return ;
    }
    out.replace(p,2,value);
    pos = p + value.size();
}

template <typename... Args>
std::string format(std::string_view pattern,const Args&... args){
    std::string out{pattern};
    std::size_t pos = 0;
    (replaceFirstPlaceholder(out,pos,toStr(args)),...);
    return out;
}

inline std::string format(std::string_view pattern) {
    return std::string(pattern);
}

enum class Level{
    Trace,Debug,Info,Warn,Error,Fatal
};

inline const char * toString(Level level){
    switch (level) {
        case Level::Trace: return "TRACE";
        case Level::Debug: return "DEBUG";
        case Level::Info:  return "INFO ";
        case Level::Warn:  return "WARN ";
        case Level::Error: return "ERROR";
        case Level::Fatal: return "FATAL";
    }
    return "?????";
}

inline std::mutex & logMutex(){
    static std::mutex mtx;
    return mtx;
}

template <typename... Args>
void log(Level level,std::string_view pattern,const Args&... args){
    const std::string line = format(pattern,args...);
    std::lock_guard<std::mutex> lock(logMutex());
    std::cout << "["<< toString(level) <<"]" <<line <<'\n';
    std::cout.flush();
}
}

#define MINI_TRACE(...) ::mini::log(::mini::Level::Trace, __VA_ARGS__)
#define MINI_DEBUG(...) ::mini::log(::mini::Level::Debug, __VA_ARGS__)
#define MINI_INFO(...)  ::mini::log(::mini::Level::Info, __VA_ARGS__)
#define MINI_WARN(...)  ::mini::log(::mini::Level::Warn, __VA_ARGS__)
#define MINI_ERROR(...) ::mini::log(::mini::Level::Error, __VA_ARGS__)

inline void printTitle(const std::string &title){
    std::cout << "\n===========" << title << " ==========\n";
}

inline void printStep(const std::string & step) {
    std::cout << "---- " << step << " ----\n";
}

inline void printSourceHint(const std::string & source) {
    std::cout << "(真实工程出处: " << source << ")\n";
}