#ifndef RIFE_LOG_H
#define RIFE_LOG_H

#include <cstdarg>
#include <memory>

enum LogLevel {
    LDEBUG = 0,
    LINFO,
    LWARNING,
    LERROR
};

class Console;

class Logger
{
public:
    static Logger& instance();    

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&);

    void init(Console& console, LogLevel level, bool verbose);

    void set_level(LogLevel level);
    void set_verbose(bool verbose);
    LogLevel get_level() const;
    bool get_verbose() const;

    void vlog(const char* fmt, va_list ap);
    void vwlog(const wchar_t* fmt, va_list ap);
    void vlogverbose(const char* fmt, va_list ap);
    void vwlogverbose(const wchar_t* fmt, va_list ap);
    ~Logger();
    private:
    Logger();
    class Impl;
    std::unique_ptr<Impl> pimpl;
};


void loginfo(const char* fmt, ...);
void logwarning(const char* fmt, ...);
void logerror(const char* fmt, ...);
void logdebug(const char* fmt, ...);
void loginfow(const wchar_t* fmt, ...);
void logwarningw(const wchar_t* fmt, ...);
void logerrorw(const wchar_t* fmt, ...);
void logdebugw(const wchar_t* fmt, ...);
void logverbose(const char* fmt, ...);
void logverbosew(const wchar_t* fmt, ...);


#endif // RIFE_LOG_H