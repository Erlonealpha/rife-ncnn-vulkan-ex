#ifndef RIFE_NCNN_PROGRESS_H
#define RIFE_NCNN_PROGRESS_H

#include <chrono>
#include <sstream>

#include "console.h"


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


class Progress
{
public:
    Progress(Console& console) : console(console)
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

    // update with loaded, processed, saved
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
            double time_now, time_delta, time_delta_last;
            _get_effective_time(time_now, time_delta, time_delta_last);
            if ((time_delta_last >= interval && time_delta_last != 0.0 && time_delta != 0.0)
                || saved == total)
                { // update if interval or finished
                    _update(time_now, time_delta);
                }
        }
        lock.unlock();
    }

    // update with extra info
    void update(bool store_extra = false, const char* extra = nullptr)
    {
        if (!enabled) return;
        lock.lock();
        double time_now, time_delta;
        _get_effective_time(time_now, time_delta);
        _update(time_now, time_delta, store_extra, extra);
        lock.unlock();
    }

    void refresh(bool store_extra = false, const char* extra = nullptr)
    {
        if (!enabled) return;
        _refresh(store_extra, extra);
    }

    void on_paused()
    {
        if (!enabled) return;
        lock.lock();
        paused_start_time = get_timestamp();
        lock.unlock();
    }

    void on_resumed()
    {
        if (!enabled) return;
        // paused_start_time may already be consumed by _get_delta_time()
        // when auto_fresh is running during PAUSED
        if (paused_start_time == 0) return;
        lock.lock();
        const double time_now = get_timestamp();
        _update_paused_time(time_now);
        paused_start_time = 0;
        lock.unlock();
    }

    double get_last_update_time()   { lock.lock(); double r = last_update_time; lock.unlock(); return r; }
    double get_speed()              { lock.lock(); double r = speed; lock.unlock(); return r; }
    float  get_percent()            { lock.lock(); float  r = percent; lock.unlock(); return r; }
    double get_time_elapsed()       { lock.lock(); double r = time_elapsed; lock.unlock(); return r; }
    double get_time_remaining()     { lock.lock(); double r = time_remaining; lock.unlock(); return r; }

private:
    void _get_effective_time(double& time_now, double& time_delta)
    {
        time_now = get_timestamp();
        if (paused_start_time != 0) _update_paused_time(time_now);
        time_delta = time_now - start_time - paused_time_total;
    }

    void _get_effective_time(double& time_now, double& time_delta, double& time_delta_last)
    {
        time_now = get_timestamp();
        if (paused_start_time != 0) _update_paused_time(time_now);
        time_delta_last = time_now - last_update_time;
        time_delta = time_now - start_time - paused_time_total;
    }

    void _update_paused_time(const double time_now)
    {
        const double time_delta = time_now - paused_start_time;
        paused_time_total += time_delta;
        paused_start_time = time_now;
    }

    void _update(double time_now, double time_delta, bool store_extra = false, const char* extra = nullptr)
    {
        this->last_update_time = time_now;
        this->speed = (double)saved_for_speed / time_delta;
        this->time_elapsed = time_delta + paused_time_total;
        
        if (speed != 0.0)
        {
            this->time_remaining = double(total - saved) / speed;
        }

        float percent = (float)saved / (float)total * 100.0f;
        if (percent > 100.0f) percent = 100.0f;
        this->percent = percent;

        _refresh(store_extra, extra);
    }

    void _refresh(bool store_extra = false, const char* extra = nullptr)
    {
        if (store_extra && extra != nullptr)
            extra_str = extra;
        const char* used_extra = extra ? extra : extra_str.c_str();
        if (!used_extra) used_extra = "";

        char elapsed_str[32], remaining_str[32];
        format_time(time_elapsed, elapsed_str);
        format_time(time_remaining, remaining_str);

        char fixed[MAX_PROGRESS_FIXED_SIZE];
        char dynamic[MAX_PROGRESS_DYNAMIC_SIZE];
        snprintf(fixed, MAX_PROGRESS_FIXED_SIZE, 
                    "%0*.2f%% L %0*d P %0*d S %0*d (%d)",
                    6, percent, total_length, loaded, 
                                total_length, processed, 
                                total_length, saved, total);
        snprintf(dynamic, MAX_PROGRESS_DYNAMIC_SIZE, 
                    " speed %.2f i/s elapsed %s remaining %s", 
                    speed, elapsed_str, remaining_str);
        static thread_local int fixed_length = strlen(fixed);
        static thread_local std::stringstream ss; // thread local to avoid memory allocation
        ss.str(""); // clear previous content
        ss << "\r" << fixed << dynamic << used_extra;
        int speed_length;
        if (speed < 1.0) speed_length = 1;
        else speed_length = static_cast<int>(log10(speed)) + 1;
        int dynamic_length = speed_length + strlen(elapsed_str) + strlen(remaining_str) + 21 + strlen(used_extra);
        if (this->dynamic_length != 0 && this->dynamic_length > dynamic_length)
        {
            // clear previous dynamic part
            ss  << std::string(this->dynamic_length - dynamic_length, ' ');
        }
        this->dynamic_length = dynamic_length;

        Console::ProgressGuard guard(console, fixed_length + dynamic_length);
        console.pprint(ss.str().c_str());
    }

    Mutex lock;
    Console& console;
    int total = 0;
    int loaded = 0;
    int processed = 0;
    int saved = 0;
    int saved_for_speed = 0; // for speed calculation
    int total_length = 1;
    double speed = 0.0f;
    float percent = 0.0f;
    std::string extra_str;
    double start_time;
    double time_elapsed = 0;
    double time_remaining = 0;
    double last_update_time = 0;
    int dynamic_length = 0;
    double paused_start_time = 0;
    double paused_time_total = 0;
};

#endif // RIFE_NCNN_PROGRESS_H