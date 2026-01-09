#include "log.h"

#include "console.h"

class Logger::Impl {
public:
    Console* console = nullptr;
    LogLevel level = LINFO;
    bool verbose = false;
};

Logger::Logger() : pimpl(std::make_unique<Impl>()) {}
Logger::~Logger() = default;

Logger& Logger::operator=(const Logger&)
{
    return *this;
}

Logger& Logger::instance()
{
    static Logger inst;
    return inst;
}

void Logger::init(Console& console, LogLevel level, bool verbose)
{
    pimpl->console = &console;
    pimpl->level = level;
    pimpl->verbose = verbose;
}

void Logger::set_level(LogLevel lvl)
{
    pimpl->level = lvl;
}
void Logger::set_verbose(bool verbose)
{
    pimpl->verbose = verbose;
}

LogLevel Logger::get_level() const
{
    return pimpl->level;
}
bool Logger::get_verbose() const
{
    return pimpl->verbose;
}

void Logger::vlog(const char* fmt, va_list ap)
{
    Console::PrintGuard guard(*pimpl->console);
    pimpl->console->rvprint(fmt, ap);
    pimpl->console->rprint("\n");
}
void Logger::vwlog(const wchar_t* fmt, va_list ap)
{
    Console::PrintGuard guard(*pimpl->console);
    pimpl->console->rvwprint(fmt, ap);
    pimpl->console->rprint("\n");
}

void Logger::vlogverbose(const char* fmt, va_list ap)
{
    if (!pimpl->verbose)
        return;
    vlog(fmt, ap);
}
void Logger::vwlogverbose(const wchar_t* fmt, va_list ap)
{
    if (!pimpl->verbose)
        return;
    vwlog(fmt, ap);
}

bool check_level(LogLevel level, LogLevel min_level)
{
    return level <= min_level;
}

void loginfo(const char* fmt, ...)
{
    Logger& logger = Logger::instance();
    if (!check_level(logger.get_level(), LINFO))
        return;
    va_list args;
    va_start(args, fmt);
    logger.vlog(fmt, args);
    va_end(args);
}
void logwarning(const char* fmt, ...)
{
    Logger& logger = Logger::instance();
    if (!check_level(logger.get_level(), LWARNING))
        return;
    va_list args;
    va_start(args, fmt);
    logger.vlog(fmt, args);
    va_end(args);
}
void logerror(const char* fmt, ...)
{
    Logger& logger = Logger::instance();
    if (!check_level(logger.get_level(), LERROR))
        return;
    va_list args;
    va_start(args, fmt);
    logger.vlog(fmt, args);
    va_end(args);
}
void logdebug(const char* fmt, ...)
{
    Logger& logger = Logger::instance();
    if (!check_level(logger.get_level(), LDEBUG))
        return;
    va_list args;
    va_start(args, fmt);
    logger.vlog(fmt, args);
    va_end(args);
}
void loginfow(const wchar_t* fmt, ...)
{
    Logger& logger = Logger::instance();
    if (!check_level(logger.get_level(), LINFO))
        return;
    va_list args;
    va_start(args, fmt);
    logger.vwlog(fmt, args);
    va_end(args);
}
void logwarningw(const wchar_t* fmt, ...)
{
    Logger& logger = Logger::instance();
    if (!check_level(logger.get_level(), LWARNING))
        return;
    va_list args;
    va_start(args, fmt);
    logger.vwlog(fmt, args);
    va_end(args);
}
void logerrorw(const wchar_t* fmt, ...)
{
    Logger& logger = Logger::instance();
    if (!check_level(logger.get_level(), LERROR))
        return;
    va_list args;
    va_start(args, fmt);
    logger.vwlog(fmt, args);
    va_end(args);
}
void logdebugw(const wchar_t* fmt, ...)
{
    Logger& logger = Logger::instance();
    if (!check_level(logger.get_level(), LDEBUG))
        return;
    va_list args;
    va_start(args, fmt);
    logger.vwlog(fmt, args);
    va_end(args);
}

void logverbose(const char* fmt, ...)
{
    Logger& logger = Logger::instance();
    if (!logger.get_verbose())
        return;
    va_list args;
    va_start(args, fmt);
    logger.vlogverbose(fmt, args);
    va_end(args);
}
void logverbosew(const wchar_t* fmt, ...)
{
    Logger& logger = Logger::instance();
    if (!logger.get_verbose())
        return;
    va_list args;
    va_start(args, fmt);
    logger.vwlogverbose(fmt, args);
    va_end(args);
}