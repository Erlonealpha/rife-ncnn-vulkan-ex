#include <chrono>
#include <sstream>
#include "platform.h"

double get_timestamp() {
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

    if (h > 0) {
        sprintf(buffer, "%02dh%02dm%02ds", h, m, s);
    } 
    else if (m > 0) {
        sprintf(buffer, "%02dm%02ds", m, s);
    }
    else if (s > 0) {
        sprintf(buffer, "%02ds%03dms", s, ms);
    }
    else {
        sprintf(buffer, "%03dms", ms);
    }
}

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

    void update(int _loaded, int _processed, int _saved, bool no_update_speed = false)
    {
        if (!enabled) return;

        lock.lock();

        if (_loaded != 0) this->loaded = this->loaded + _loaded;
        if (_processed != 0) this->processed = this->processed + _processed;
        if (_saved != 0) 
        {
            this->saved = this->saved + _saved;
            if (!no_update_speed)
            this->saved_for_speed = this->saved_for_speed + _saved;
        };
        
        if (_saved > 0) 
        { // no update if no save
            const double time_now = get_timestamp();
            const double time_delta = time_now - start_time;
            const double time_delta_last = time_now - last_update_time;

            if ((time_delta_last >= interval && time_delta_last != 0.0 && time_delta != 0.0)
                || saved == total) 
            { // update if interval or finished
                this->last_update_time = time_now;
                this->speed = (double)saved_for_speed / time_delta;
                this->time_elapsed = time_delta;
                
                if (speed != 0.0) 
                {
                    double time_remaining_ = double(total - saved) / speed;
                    this->time_remaining = time_remaining_;
                }

                float percent = (float)saved / (float)total * 100.0f;
                if (percent > 100.0f) percent = 100.0f;

                refresh_console(percent, speed, time_elapsed, time_remaining);
            }
        }
        lock.unlock();
    }
private:
    void refresh_console(float percent, double speed, double elapsed, double remaining) 
    {
        char elapsed_str[128], remaining_str[128];
        format_time(elapsed, elapsed_str);
        format_time(remaining, remaining_str);

        char fixed[256];
        char dynamic[256];
        snprintf(fixed, 256, "%0*.2f%% L %0*d P %0*d S %0*d (%d)",
                        6, percent, total_length, loaded, 
                                    total_length, processed, 
                                    total_length, saved, total);
        snprintf(dynamic, 256, " speed %.2f i/s elapsed %s remaining %s", speed, elapsed_str, remaining_str);
        static thread_local std::stringstream ss; // thread local to avoid memory allocation
        ss.str(""); // clear previous content
        ss << "\r" << fixed << dynamic;
        int speed_length;
        if (speed < 1.0) speed_length = 1;
        else speed_length = static_cast<int>(log10(speed)) + 1;
        int dynamic_length = speed_length + strlen(elapsed_str) + strlen(remaining_str) + 21;
        if (this->dynamic_length != 0 && this->dynamic_length > dynamic_length)
        {
            // clear previous dynamic part
            ss  << std::string(this->dynamic_length - dynamic_length, ' ');
        }
        this->dynamic_length = dynamic_length;
        fprintf(stderr, "%s", ss.str().c_str());
    }
    ncnn::Mutex lock;
    int total = 0;
    int loaded = 0;
    int processed = 0;
    int saved = 0;
    int total_length = 1;
    double speed = 0.0f;
    double start_time;
    double time_elapsed = 0;
    double time_remaining = 0;
    double last_update_time = 0;
    int saved_for_speed = 0; // for speed calculation
    int dynamic_length = 0;
};