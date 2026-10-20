#if !defined(ERROR_H)
#define ERROR_H

#include <cstdarg>
#include <cstdio>
#include <stdexcept>
#include <string>

// an error that stops the compilation. caught in main(), and no pmd is written
class compile_error : public std::runtime_error
{
public:
    explicit compile_error(const std::string& msg) : std::runtime_error(msg) {}
};

// lineno <= 0: the error isn't tied to a line
inline std::string format_error(int lineno, const char* fmt, va_list ap)
{
    char msg[1024];
    vsnprintf(msg, sizeof msg, fmt, ap);
    if(lineno <= 0)
        return msg;
    return "line " + std::to_string(lineno) + ": " + msg;
}

[[noreturn]] inline void fatal(int lineno, const char* fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    std::string msg = format_error(lineno, fmt, ap);
    va_end(ap);
    throw compile_error(msg);
}

#endif // !defined(ERROR_H)
