#ifndef RIFE_NCNN_CONSOLE_H
#define RIFE_NCNN_CONSOLE_H

#include <stdarg.h>
#include "plat.h"

#define CURSOR_CLEAR_LINE "\033[2K"
#define CURSOR_SHOW "\033[?25h"
#define CURSOR_HIDE "\033[?25l"

// class _ConsoleIn
// {
// };

class Console
{
public:
    Console() 
    {
        // setvbuf(stderr, NULL, _IOLBF, 1024);
        last_progress[0] = '\0';
    }
    Console(Console& console) {}

    void show_cursor(bool show)
    {
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
        lock.lock();
        _vprint(fmt, args);
        lock.unlock();
    }

    void vwprint(const wchar_t* fmt, va_list args)
    {
        lock.lock();
        _vwprint(fmt, args);
        lock.unlock();
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
        lock.lock();
        _flush();
        lock.unlock();
    }

    void progress_start(int length)
    {
        lock.lock();
        progress_bar_length = length;
    }

    void progress_end()
    {
        in_progress = true;
        lock.unlock();
    }

    void rprint(const char* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        _vprint(fmt, args);
        va_end(args);
    }

    void rwprint(const wchar_t* fmt, ...)
    {
        va_list args;
        va_start(args, fmt);
        _vwprint(fmt, args);
        va_end(args);
    }

    void pprint(const char* msg)
    {
        fprintf(stderr, "%s", msg);
        snprintf(last_progress, MAX_PROGRESS_SIZE, "%s", msg);
    }

    void lock_acquire()
    {
        lock.lock();
    }

    void lock_release()
    {
        lock.unlock();
    }
private:
    void _vprint(const char* fmt, va_list args)
    {
        if (in_progress)
        {
            fprintf(stderr, "\r%s", CURSOR_CLEAR_LINE);
            vfprintf(stderr, fmt, args);
            fprintf(stderr, "%s", last_progress);
        }
        else
        {
            vfprintf(stderr, fmt, args);
        }
    }

    void _vwprint(const wchar_t* fmt, va_list args)
    {
        if (in_progress)
        {
            fprintf(stderr, "\r%s", CURSOR_CLEAR_LINE);
            vfwprintf(stderr, fmt, args);
            fprintf(stderr, "%s", last_progress);
        }
        else
        {
            vfwprintf(stderr, fmt, args);
        }
    }

    void _flush()
    {
        fflush(stderr);
    }

    Mutex lock;
    bool in_progress = false;
    char last_progress[MAX_PROGRESS_SIZE]; //  last progress bar message
    int progress_bar_length = 0;
};

#endif // RIFE_NCNN_CONSOLE_H