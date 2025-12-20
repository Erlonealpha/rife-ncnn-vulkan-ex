#include <chrono>
#include <sstream>
#include "platform.h"

double get_timestamp() 
{
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::system_clock::now().time_since_epoch()
    ).count() / 1000.0;
}

void format_time(double seconds, char* buffer)
{
    int h = static_cast<int>(seconds / 3600);
    int m = static_cast<int>((seconds - h * 3600) / 60);
    int s = static_cast<int>(seconds - h * 3600 - m * 60);
    int ms = static_cast<int>((seconds - floor(seconds)) * 1000);

    if (h > 0) 
    {
        sprintf(buffer, "%02dh%02dm%02ds", h, m, s);
    } 
    else if (m > 0) 
    {
        sprintf(buffer, "%02dm%02ds", m, s);
    }
    else if (s > 0) 
    {
        sprintf(buffer, "%02ds", s);
    }
    else if (ms > 0)
    {
        sprintf(buffer, "%03dms", ms);
    }
    else
    {
        sprintf(buffer, "0s");
    }
}

#define CURSOR_CLEAR_LINE "\033[2K"
#define CURSOR_SHOW "\033[?25h"
#define CURSOR_HIDE "\033[?25l"

class Console
{
public:
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
        fflush(stderr);
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
        this->last_progress = const_cast<char*>(msg);
    }
private:
    void _vprint(const char* fmt, va_list args)
    {
        if (in_progress)
        {
            fprintf(stderr, "\r%s", CURSOR_CLEAR_LINE);
            vfprintf(stderr, fmt, args);
            if (last_progress != nullptr)
            fprintf(stderr, last_progress);
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
            if (last_progress != nullptr)
            fprintf(stderr, last_progress);
        }
        else
        {
            vfwprintf(stderr, fmt, args);
        }
    }

    ncnn::Mutex lock;
    bool in_progress = false;
    char* last_progress = nullptr; //  last progress bar message
    int progress_bar_length = 0;
};

class Progress
{
public:
    Progress()
    {
        start_time = get_timestamp();
    }

    bool enabled = true;
    float interval = 0.5f;

    void set_total(int total)
    {
        if (total == this->total) return;
        lock.lock();
        this->total = total;
        if (total > 0)
        {
            this->total_length = static_cast<int>(log10(total)) + 1;
        } 
        else if (total <= 0)
        {
            this->total = 0;
            this->loaded = 0;
            this->processed = 0;
            this->saved = 0;
            this->total_length = 1;
        }
        lock.unlock();
    }

    void update(int loaded, int processed, int saved, bool no_update_speed = false)
    {
        if (!enabled) return;

        lock.lock();
        if (loaded != 0)
        {
            this->loaded = this->loaded + loaded;
        }
        if (processed != 0)
        {
            this->processed = this->processed + processed;
        }
        if (saved != 0) 
        {
            this->saved = this->saved + saved;
            if (!no_update_speed)
            this->saved_for_speed = this->saved_for_speed + saved;
        };
        
        if (saved > 0)
        { // no update if no save
            const double time_now = get_timestamp();
            const double time_delta = time_now - start_time;
            const double time_delta_last = time_now - last_update_time;
            if ((time_delta_last >= interval && time_delta_last != 0.0 && time_delta != 0.0)
                || saved == total)
                { // update if interval or finished
                    _update(time_now, time_delta, true);
                }
        }
        lock.unlock();
    }

    void update(bool store_extra = false, char* extra = nullptr)
    {
        const double time_now = get_timestamp();
        const double time_delta = time_now - start_time;
        lock.lock();
        _update(time_now, time_delta, store_extra, extra);
        lock.unlock();
    }

    void refresh(bool store_extra = false, char* extra = nullptr)
    {
        _refresh(store_extra, extra);
    }

    Console console;
private:
    void _update(double time_now, double time_delta, bool store_extra = false, char* extra = nullptr)
    {
        this->last_update_time = time_now;
        this->speed = (double)saved_for_speed / time_delta;
        this->time_elapsed = time_delta;
        
        if (speed != 0.0)
        {
            this->time_remaining = double(total - saved) / speed;
        }

        float percent = (float)saved / (float)total * 100.0f;
        if (percent > 100.0f) percent = 100.0f;
        this->percent = percent;

        _refresh(store_extra, extra);
    }

    void _refresh(bool store_extra = false, char* extra = nullptr)
    {
        if (store_extra)
        {
            this->extra_str = extra;
        }
        else if (extra_str != nullptr && extra != extra_str)
        {
            extra = extra_str;
        }
        if (extra == nullptr)
        {
            extra = "";
        }

        char elapsed_str[128], remaining_str[128];
        format_time(time_elapsed, elapsed_str);
        format_time(time_remaining, remaining_str);

        char fixed[128];
        char dynamic[128];
        snprintf(fixed, 256, "%0*.2f%% L %0*d P %0*d S %0*d (%d)",
                        6, percent, total_length, loaded, 
                                    total_length, processed, 
                                    total_length, saved, total);
        snprintf(dynamic, 256, " speed %.2f i/s elapsed %s remaining %s", speed, elapsed_str, remaining_str);
        static thread_local int fixed_length = strlen(fixed);
        static thread_local std::stringstream ss; // thread local to avoid memory allocation
        ss.str(""); // clear previous content
        ss << "\r" << fixed << dynamic << extra;
        int speed_length;
        if (speed < 1.0) speed_length = 1;
        else speed_length = static_cast<int>(log10(speed)) + 1;
        int dynamic_length = speed_length + strlen(elapsed_str) + strlen(remaining_str) + 21 + strlen(extra);
        if (this->dynamic_length != 0 && this->dynamic_length > dynamic_length)
        {
            // clear previous dynamic part
            ss  << std::string(this->dynamic_length - dynamic_length, ' ');
        }
        this->dynamic_length = dynamic_length;

        console.progress_start(fixed_length + dynamic_length);
        console.pprint(ss.str().c_str());
        console.progress_end();
    }

    ncnn::Mutex lock;
    int total = 0;
    int loaded = 0;
    int processed = 0;
    int saved = 0;
    int saved_for_speed = 0; // for speed calculation
    int total_length = 1;
    double speed = 0.0f;
    float percent = 0.0f;
    char* extra_str = nullptr;
    double start_time;
    double time_elapsed = 0;
    double time_remaining = 0;
    double last_update_time = 0;
    int dynamic_length = 0;
};