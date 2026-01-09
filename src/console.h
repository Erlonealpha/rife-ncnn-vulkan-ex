#ifndef RIFE_NCNN_CONSOLE_H
#define RIFE_NCNN_CONSOLE_H

#include <stdarg.h>
#include "plat.h"


// terminal detection
#if _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <stdio.h>
#include <io.h>

static bool __is_console(FILE* stream)
{
    if (!stream) return false;
    return (bool)_isatty(_fileno(stream));
}

static bool __is_vt_mode(int Handle)
{
    HANDLE hOut = GetStdHandle(Handle);
    if (hOut == INVALID_HANDLE_VALUE)
        return false;

    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode))
        return false;

    // Already enabled
    if (mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING)
        return true;

    return false;
}

static bool __is_legacy_console(int Handle)
{
    HANDLE hOut = GetStdHandle(Handle);

    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode))
        return true;

    return !(mode & ENABLE_VIRTUAL_TERMINAL_PROCESSING);
}

#else // POSIX (Linux / macOS)

#include <unistd.h>

bool __is_console(FILE* stream)
{
    return isatty(fileno(stream));
}

bool __enable_vt_mode(FILE* stream)
{
    // POSIX terminals: VT sequences are supported by default
    // If stream is not a TTY, we should not use VT
    return __is_console(stream);
}

bool __is_legacy_console(FILE* stream)
{
    // POSIX environments don't have "legacy console" concept
    // If it's a TTY, VT is available
    return !__is_console(stream);
}

#endif // _WIN32


#define CURSOR_CLEAR_LINE "\033[2K"
#define CURSOR_SHOW "\033[?25h"
#define CURSOR_HIDE "\033[?25l"


class Console
{
public:
    Console() 
    {
        last_progress[0] = '\0';
        _is_console = __is_console(stderr);
#if _WIN32
        _is_vt_mode = __is_vt_mode(STD_ERROR_HANDLE);
        _is_legacy_console = __is_legacy_console(STD_ERROR_HANDLE);
#else
        _is_vt_mode = _is_console;
        _is_legacy_console = !_is_console;
#endif
    }

    void show_cursor(bool show)
    {
        if (!_is_console || !_is_vt_mode || _is_legacy_console) return;
        if (show)
        {
            fprintf(stderr, CURSOR_SHOW);
        }
        else
        {
            fprintf(stderr, CURSOR_HIDE);
        }
    }

    void vprint(const char* fmt, va_list args)
    {
        PrintGuard guard(*this, true);
        _vprint(fmt, args);
        _print_last_progress(!_inside_block);
    }

    void vwprint(const wchar_t* fmt, va_list args)
    {
        PrintGuard guard(*this, true);
        _vwprint(fmt, args);
        _print_last_progress(!_inside_block);
    }

    void print(const char* fmt,...)
    {
        va_list args;
        va_start(args, fmt);
        vprint(fmt, args);
        va_end(args);
    }

    void wprint(const wchar_t* fmt,...)
    {
        va_list args;
        va_start(args, fmt);
        vwprint(fmt, args);
        va_end(args);
    }

    void flush()
    {
        PrintGuard guard(*this, true);
        _flush();
    }

    void progress_start(int length)
    {
        lock.lock();
        progress_bar_length = length;
    }

    void progress_end()
    {
        in_progress = true;
        _progress_clear = false;
        lock.unlock();
    }

    /*
        @attention: 
            If in_progress == true:
                start_print() must be called before first rprint()
                end_print() must be called after last rprint()
    */
    void rprint(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        _vprint(fmt, args);
        va_end(args);
    }

    /*
        @attention: 
            If in_progress == true:
                start_print() must be called before first rprint()
                end_print() must be called after last rprint()
    */
    void rwprint(const wchar_t* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        _vwprint(fmt, args);
        va_end(args);
    }

    // used by log.h
    void rvprint(const char* fmt, va_list args)
    {
        _vprint(fmt, args);
    }

    void rvwprint(const wchar_t* fmt, va_list args)
    {
        _vwprint(fmt, args);
    }

    void pprint(const char* msg)
    {
        if (!_is_console) return;
        fprintf(stderr, "%s", msg);
        snprintf(last_progress, MAX_PROGRESS_SIZE, "%s", msg);
    }

    void start_print()
    {
        _inside_block = true;
    }

    void end_print()
    {
        _inside_block = false;
        _print_last_progress();
    }

    void lock_acquire()
    {
        lock.lock();
    }

    void lock_release()
    {
        lock.unlock();
    }

    bool is_console()
    {
        return _is_console;
    }

    bool is_vt_mode()
    {
        return _is_vt_mode;
    }

    bool is_legacy_console()
    {
        return _is_legacy_console;
    }

    class PrintGuard {
    public:
        explicit PrintGuard(Console& c, bool only_lock = false)
            : console(c), only_lock(only_lock)
        {
            console.lock.lock();
            if (!only_lock)
                console.start_print();
        }

        ~PrintGuard()
        {
            if (!only_lock)
                console.end_print();
            console.lock.unlock();
        }

        PrintGuard(const PrintGuard&) = delete;
        PrintGuard& operator=(const PrintGuard&) = delete;
    private:
        Console& console;
        bool only_lock;
    };

    class ProgressGuard {
    public:
        explicit ProgressGuard(Console& c, int length)
            : console(c), length(length)
        {
            console.progress_start(length);
        }

        ~ProgressGuard()
        {
            console.progress_end();
        }

        ProgressGuard(const ProgressGuard&) = delete;
        ProgressGuard& operator=(const ProgressGuard&) = delete;
    private:
        Console& console;
        int length;
    };
private:
    void _clear_line_start()
    {
        if (!_is_vt_mode || _is_legacy_console)
            fprintf(stderr, "\r");
        else
            fprintf(stderr, "\r%s", CURSOR_CLEAR_LINE);
    }

    void _print_last_progress(bool need = true)
    {
        if (!need) return;
        if (in_progress && last_progress[0] != '\0')
        {
            fprintf(stderr, "%s", last_progress);
            _progress_clear = false;
        }
    }

    void _vprint(const char* fmt, va_list args)
    {
        if (in_progress && !_progress_clear)
        {
            _clear_line_start();
            _progress_clear = true;
        }
        vfprintf(stderr, fmt, args);
    }

    void _vwprint(const wchar_t* fmt, va_list args)
    {
        if (in_progress && !_progress_clear)
        {
            _clear_line_start();
            _progress_clear = true;
        }
        vfwprintf(stderr, fmt, args);
    }

    void _flush()
    {
        fflush(stderr);
    }

    Mutex lock;
    bool in_progress = false;
    char last_progress[MAX_PROGRESS_SIZE]; //  last progress bar message
    int progress_bar_length = 0;
    bool _progress_clear = true;
    bool _inside_block = false; // inside logical print block
    bool _is_console;
    bool _is_vt_mode;
    bool _is_legacy_console; // WIN32 only
};

#endif // RIFE_NCNN_CONSOLE_H