#ifndef RIFE_LOG_H
#define RIFE_LOG_H

#include "console.h"

#define LDEBUG 0
#define LINFO 1
#define LWARNING 2
#define LERROR 3

extern int gloglevel;
extern bool gverbose;
extern Console console;

#define rprint(fmt,...)    console.rprint(fmt, ##__VA_ARGS__)
#define rwprint(fmt,...)   console.rwprint(fmt, ##__VA_ARGS__)

#define rife_log(level, fmt,...) \
    do { \
        if (level >= gloglevel) { \
            console.lock_acquire(); \
            rprint(fmt, ##__VA_ARGS__); \
            rprint("\n"); \
            console.lock_release(); \
        } \
    } while (0)
#define rife_logw(level, fmt,...) \
    do { \
        if (level >= gloglevel) { \
            console.lock_acquire(); \
            rwprint(fmt, ##__VA_ARGS__); \
            rwprint(L"\n"); \
            console.lock_release(); \
        } \
    } while (0)
#define logverbose(fmt,...) \
    do { \
        if (gverbose) { \
            console.lock_acquire(); \
            rprint(fmt, ##__VA_ARGS__); \
            rprint("\n"); \
            console.lock_release(); \
        } \
    } while (0)
#define logverbosew(fmt,...) \
    do { \
        if (gverbose) { \
            console.lock_acquire(); \
            rwprint(fmt, ##__VA_ARGS__); \
            rwprint(L"\n"); \
            console.lock_release(); \
        } \
    } while (0)
#define loginfo(fmt,...)  rife_log(LINFO, fmt, ##__VA_ARGS__)
#define logwarning(fmt,...) rife_log(LWARNING, fmt, ##__VA_ARGS__)
#define logerror(fmt,...)  rife_log(LERROR, fmt, ##__VA_ARGS__)
#define logdebug(fmt,...) rife_log(LDEBUG, fmt, ##__VA_ARGS__)
#define loginfow(fmt,...) rife_logw(LINFO, fmt, ##__VA_ARGS__)
#define logwarningw(fmt,...) rife_logw(LWARNING, fmt, ##__VA_ARGS__)
#define logerrorw(fmt,...) rife_logw(LERROR, fmt, ##__VA_ARGS__)
#define logdebugw(fmt,...) rife_logw(LDEBUG, fmt, ##__VA_ARGS__)

#endif // RIFE_LOG_H