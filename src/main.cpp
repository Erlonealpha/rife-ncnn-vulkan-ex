// rife implemented with ncnn library

#include <stdio.h>
#include <unordered_map>
#include <algorithm>
#include <queue>
#include <vector>
#include <clocale>
#include <regex>
#include <csignal>
#include <atomic>
#include <cstdarg>
#include <functional>


#if _WIN32
#include <io.h>
// image decoder and encoder with wic
#include "wic_image.h"
#else // _WIN32
// image decoder and encoder with stb
#define STB_IMAGE_IMPLEMENTATION
#define STBI_NO_PSD
#define STBI_NO_TGA
#define STBI_NO_GIF
#define STBI_NO_HDR
#define STBI_NO_PIC
#define STBI_NO_STDIO
#include "stb_image.h"
#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"
#endif // _WIN32
#include "webp_image.h"

#include "progress.h"

bool gdebug = false;

Progress progress;

#define rprint(fmt, ...)    progress.console.rprint(fmt, ##__VA_ARGS__)
#define rwprint(fmt, ...)   progress.console.rwprint(fmt, ##__VA_ARGS__)
#define print(fmt, ...)     progress.console.print(fmt, ##__VA_ARGS__)
#define wprint(fmt, ...)    progress.console.wprint(fmt, ##__VA_ARGS__)

#define debug_output(fmt, ...)  (gdebug? print(fmt, ##__VA_ARGS__) : (void)0)
#define debug_outputw(fmt, ...) (gdebug? wprint(fmt, ##__VA_ARGS__) : (void)0)

#if _WIN32
#include <wchar.h>
static wchar_t* optarg = NULL;
static int optind = 1;
static wchar_t getopt(int argc, wchar_t* const argv[], const wchar_t* optstring)
{
    if (optind >= argc || argv[optind][0] != L'-')
        return -1;

    wchar_t opt = argv[optind][1];
    const wchar_t* p = wcschr(optstring, opt);
    if (p == NULL)
        return L'?';

    optarg = NULL;

    if (p[1] == L':')
    {
        optind++;
        if (optind >= argc)
            return L'?';

        optarg = argv[optind];
    }

    optind++;

    return opt;
}

static std::vector<int> parse_optarg_int_array(const wchar_t* optarg)
{
    std::vector<int> array;
    array.push_back(_wtoi(optarg));

    const wchar_t* p = wcschr(optarg, L',');
    while (p)
    {
        p++;
        array.push_back(_wtoi(p));
        p = wcschr(p, L',');
    }

    return array;
}
#else // _WIN32
#include <unistd.h> // getopt()

static std::vector<int> parse_optarg_int_array(const char* optarg)
{
    std::vector<int> array;
    array.push_back(atoi(optarg));

    const char* p = strchr(optarg, ',');
    while (p)
    {
        p++;
        array.push_back(atoi(p));
        p = strchr(p, ',');
    }

    return array;
}
#endif // _WIN32

// ncnn
#include "cpu.h"
#include "gpu.h"
#include "benchmark.h"

#include "plat.h"

#include "rife.h"

#include "filesystem_utils.h"
#include "input.h"

static void print_usage()
{
    rprint("Usage: rife-ncnn-vulkan-ex -0 infile -1 infile1 -o outfile [options]...\n");
    rprint("       rife-ncnn-vulkan-ex -i indir -o outdir [options]...\n\n");
    rprint("  -h                   show this help\n");
    rprint("  -v                   verbose output\n");
    rprint("  -0 input0-path       input image0 path (jpg/png/webp)\n");
    rprint("  -1 input1-path       input image1 path (jpg/png/webp)\n");
    rprint("  -i input-path        input image directory (jpg/png/webp)\n");
    rprint("  -o output-path       output image path (jpg/png/webp) or directory\n");
    rprint("  -n num-frame         target frame count (default=N*2)\n");
    rprint("  -s time-step         time step (0~1, default=0.5)\n");
    rprint("  -m model-path        rife model path (default=rife-v2.3)\n");
    rprint("  -g gpu-id            gpu device to use (-1=cpu, default=auto) can be 0,1,2 for multi-gpu\n");
    rprint("  -j load:proc:save    thread count for load/proc/save (default=1:2:2)\n");
    rprint("                       (when '-r' specified, save is forced to 1) can be 1:2,2,2:2 for multi-gpu\n");
    rprint("  -r                   raw output to stdout (no jpg/png/webp output)\n");
    rprint("  -x                   enable spatial tta mode\n");
    rprint("  -z                   enable temporal tta mode\n");
    rprint("  -u                   enable UHD mode\n");
    rprint("  -f pattern-format    output image filename pattern format (%%08d.jpg/png/webp, default=ext/%%08d.png)\n");
    rprint("  -p progress          show progress (0=off, 1=on, default=1)\n");
    rprint("  -t progress-interval progress update interval in seconds (default=0.5)\n");
    rprint("  -d                   enable debug output\n\n");
}

static int decode_image(const path_t& imagepath, ncnn::Mat& image, int* webp)
{
    *webp = 0;

    unsigned char* pixeldata = 0;
    int w;
    int h;
    int c;

#if _WIN32
    FILE* fp = _wfopen(imagepath.c_str(), L"rb");
#else
    FILE* fp = fopen(imagepath.c_str(), "rb");
#endif
    if (fp)
    {
        // read whole file
        unsigned char* filedata = 0;
        int length = 0;
        {
            fseek(fp, 0, SEEK_END);
            length = ftell(fp);
            rewind(fp);
            filedata = (unsigned char*)malloc(length);
            if (filedata)
            {
                fread(filedata, 1, length, fp);
            }
            fclose(fp);
        }

        if (filedata)
        {
            pixeldata = webp_load(filedata, length, &w, &h, &c);
            if (pixeldata)
            {
                *webp = 1;
            }
            else
            {
                // not webp, try jpg png etc.
#if _WIN32
                pixeldata = wic_decode_image(imagepath.c_str(), &w, &h, &c);
#else // _WIN32
                pixeldata = stbi_load_from_memory(filedata, length, &w, &h, &c, 3);
                c = 3;
#endif // _WIN32
            }

            free(filedata);
        }
    }

    if (!pixeldata)
    {
#if _WIN32
        wprint(L"decode image %ls failed\n", imagepath.c_str());
#else // _WIN32
        print("decode image %s failed\n", imagepath.c_str());
#endif // _WIN32

        return -1;
    }

    image = ncnn::Mat(w, h, (void*)pixeldata, (size_t)3, 3);

    return 0;
}

static int encode_image(const path_t& imagepath, const ncnn::Mat& image)
{
    int success = 0;

    path_t ext = get_file_extension(imagepath);

    if (ext == PATHSTR("webp") || ext == PATHSTR("WEBP"))
    {
        success = webp_save(imagepath.c_str(), image.w, image.h, image.elempack, (const unsigned char*)image.data);
    }
    else if (ext == PATHSTR("png") || ext == PATHSTR("PNG"))
    {
#if _WIN32
        success = wic_encode_image(imagepath.c_str(), image.w, image.h, image.elempack, image.data);
#else
        success = stbi_write_png(imagepath.c_str(), image.w, image.h, image.elempack, image.data, 0);
#endif
    }
    else if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") || ext == PATHSTR("JPEG"))
    {
#if _WIN32
        success = wic_encode_jpeg_image(imagepath.c_str(), image.w, image.h, image.elempack, image.data);
#else
        success = stbi_write_jpg(imagepath.c_str(), image.w, image.h, image.elempack, image.data, 100);
#endif
    }

    if (!success)
    {
#if _WIN32
        wprint(L"encode image %ls failed\n", imagepath.c_str());
#else
        print("encode image %s failed\n", imagepath.c_str());
#endif
    }

    return success ? 0 : -1;
}


#define STATE_EMPTY         0x0001
#define STATE_IDLE          0x0002
#define STATE_RUNNING       0x0004
#define STATE_PAUSED        0x0008
#define STATE_STOPPED       0x0010
#define STATE_INTERRUPTED   0x0020

#define TRANSIT_NONE        0x0040
#define TRANSIT_PAUSING     0x0080
#define TRANSIT_RESUMING    0x0100

struct CallbackContext{
    int state;
    std::function<void(int)> callback;
    const char* name;
};

class ProcessController
{
public:
    int state()
    {
        lock.lock();
        int ret = _state;
        lock.unlock();
        return ret;
    }

    int state_transit()
    {
        lock.lock();
        int ret = _transit;
        lock.unlock();
        return ret;
    }

    /*
        @param state_id: STATE_RUNNING | STATE_STOPPED | STATE_INTERRUPTED
    */
    void set_state(int state_id)
    {
        switch (state_id)
        {
        case STATE_RUNNING:
            _set_state(STATE_RUNNING);
            break;
        case STATE_INTERRUPTED:
            set_interrupted();
            break;
        case STATE_STOPPED:
            set_stopped();
            break;
        }
    }

    /*
        @param state_id: STATE_PAUSED
        @param value: true | false
    */
    void set_state(int state_id, bool value)
    {
        switch (state_id)
        {
        case STATE_PAUSED:
            set_paused(value);
            break;
        }
    }

    inline bool is_stopped_fast()
    {
        return stop_flag.load(std::memory_order_acquire);
    }

    int wait_resume() // wait until resuming and return the new state
    {
        lock.lock();
        action_done_count++;
        _check_pause_state();
        condition.wait(lock);
        action_done_count++;
        _check_pause_state();
        int ret = _state;
        lock.unlock();
        return ret;
    }

    void request_increase()
    {
        lock.lock();
        request_size++;
        _check_pause_state();
        lock.unlock();
    }

    void request_decrease()
    {
        lock.lock();
        request_size--;
        _check_pause_state();
        lock.unlock();
    }

    /*
        @attention: 不要在回调函数中获取ProcessController的锁，否则会导致死锁。
        @attention: Do not get the lock of ProcessController in the callback function, 
                    otherwise it will cause deadlock.
    */
    void register_callback(const char* name, int state, std::function<void(int)> callback)
    {
        lock.lock();
        callbacks.push_back({ state, callback, name });
        lock.unlock();
    }

    void unregister_callback(const char* name)
    {
        lock.lock();
        for (auto it = callbacks.begin(); it!= callbacks.end(); it++)
        {
            if (it->name == name)
            {
                callbacks.erase(it);
                break;
            }
        }
        lock.unlock();
    }

    
    /*
        等待指定状态
        返回 -1 表示超时，其他值表示目标状态
        return -1 means timeout, other values means target state
        @attention: 如果state和transit同时被修改则优先返回state
                    state 参数混合transit时可能会等待超过预期的时间
        @attention: If both state and transit are modified, state is preferred to be returned
                    when state and transit are mixed, it may wait for more than expected time
    */
    int wait_state(int state, unsigned long timeout_ms = WAIT_INFINITE)
    {
        lock.lock();

        bool wait_state_flag =
            state & (STATE_RUNNING | STATE_PAUSED | STATE_STOPPED | STATE_INTERRUPTED);
        bool wait_transit_flag =
            state & (TRANSIT_NONE | TRANSIT_PAUSING | TRANSIT_RESUMING);

        if (!wait_state_flag && !wait_transit_flag)
        {
            lock.unlock();
            return -1; // Invalid state
        }

        if (wait_state_flag && (_state & state))
        {
            int ret = _state;
            lock.unlock();
            return ret;
        }
        if (wait_transit_flag && (_transit & state))
        {
            int ret = _transit;
            lock.unlock();
            return ret;
        }

        int now_state = _state;
        int now_transit = _transit;

        bool timeout = false;
        while (_state == now_state && _transit == now_transit)
        {
            // may wait multiple times, sleep time may > timeout_ms
            if (!state_changed_cond.wait(lock, timeout_ms))
            {
                timeout = true;
                break;
            }
        }

        int ret = -1;
        if (!timeout)
        {
            if (wait_state_flag && _state != now_state)
                ret = _state;
            else if (wait_transit_flag && _transit != now_transit)
                ret = _transit;
        }

        lock.unlock();
        return ret;
    }
private:
    void set_paused(bool paused)
    {
        lock.lock();

        if (_transit != TRANSIT_NONE)
        {
            debug_output("set paused during transit\n");
            lock.unlock();
            return;
        }

        if (paused)
        {
            if (_state != STATE_RUNNING)
            {
                debug_output("set pause but not running\n");
                lock.unlock();
                return;
            }

            _set_transit(TRANSIT_PAUSING);
            action_done_count = 0;
            debug_output("set pause\n");
        }
        else
        {
            if (_state != STATE_PAUSED)
            {
                debug_output("set resume but not paused\n");
                lock.unlock();
                return;
            }

            _set_transit(TRANSIT_RESUMING);
            action_done_count = 0;
            condition.broadcast();
            debug_output("set resume\n");
        }

        lock.unlock();
    }

    void set_stopped()
    {
        lock.lock();

        // 正在 pausing：等 pause 完成 → resume → stop
        if (_transit == TRANSIT_PAUSING)
        {
            debug_output("set pending stop during pausing\n");
            _pending_state = STATE_STOPPED;
            lock.unlock();
            return;
        }

        // 已 paused：先 resume，再 stop
        if (_state == STATE_PAUSED)
        {
            debug_output("set pending stop during paused\n");
            _set_transit(TRANSIT_RESUMING);
            _pending_state = STATE_STOPPED;
            action_done_count = 0;
            condition.broadcast();
            lock.unlock();
            return;
        }

        // running / idle：直接 stop
        if (_state == STATE_RUNNING || _state == STATE_IDLE)
        {
            debug_output("set stop\n");
            _do_stop();
        }

        lock.unlock();
    }

    void set_interrupted()
    {
        lock.lock();

        if (_transit == TRANSIT_PAUSING)
        {
            debug_output("set pending interrupt during pausing\n");
            _pending_state = STATE_INTERRUPTED;
            lock.unlock();
            return;
        }

        if (_state == STATE_PAUSED)
        {
            debug_output("set pending interrupt during paused\n");
            _set_transit(TRANSIT_RESUMING);
            _pending_state = STATE_INTERRUPTED;
            action_done_count = 0;
            condition.broadcast();
            lock.unlock();
            return;
        }

        if (_state == STATE_RUNNING || _state == STATE_IDLE)
        {
            debug_output("set interrupt\n");
            _do_interrupt();
        }

        lock.unlock();
    }

    void _check_pause_state()
    {
        if (_transit == TRANSIT_NONE)
            return;

        if (request_size != 0 && action_done_count != request_size)
            return;

        if (_transit == TRANSIT_PAUSING)
        {
            debug_output("ProcessController: pausing done\n");
            _set_state(STATE_PAUSED);
        }
        else if (_transit == TRANSIT_RESUMING)
        {
            debug_output("ProcessController: resuming done\n");
            _set_state(STATE_RUNNING);
        }

        _set_transit(TRANSIT_NONE);
        action_done_count = 0;

        if (_pending_state == STATE_STOPPED)
        {
            _pending_state = STATE_EMPTY;
            _do_stop();
        }
        else if (_pending_state == STATE_INTERRUPTED)
        {
            _pending_state = STATE_EMPTY;
            _do_interrupt();
        }
    }

    void _do_stop()
    {
        debug_output("ProcessController: do_stop\n");
        _set_state(STATE_STOPPED);
        stop_flag.store(true, std::memory_order_release);
    }

    void _do_interrupt()
    {
        debug_output("ProcessController: do_interrupt\n");
        _set_state(STATE_INTERRUPTED);
        stop_flag.store(true, std::memory_order_release);
    }

    void _set_state(int state)
    {
        _state = state;
        state_changed_cond.broadcast();
        _handle_callbacks(state);
    }

    void _set_transit(int transit)
    {
        _transit = transit;
        state_changed_cond.broadcast();
        _handle_callbacks(transit);
    }

    void _handle_callbacks(int state)
    {
        for (auto& callback : callbacks)
        {
            if (state & callback.state)
            {
                callback.callback(state);
            }
        }
    }

    int _state = STATE_IDLE;
    int _transit = TRANSIT_NONE;
    int _pending_state = STATE_EMPTY;  // 延迟合并的目标（STOP / INTERRUPT）

    int request_size = 0;              // 等待完成的请求数
    int action_done_count = 0;         // 已完成的请求数

    std::atomic<bool> stop_flag{false};
    Mutex lock;
    ConditionVariable condition;
    ConditionVariable state_changed_cond;
    std::vector<CallbackContext> callbacks;
};

ProcessController process_controller;

#define stop_with_error(message,...)    {print(message, ##__VA_ARGS__);  process_controller.set_state(STATE_STOPPED);}
#define stop_with_errorw(message,...)   {wprint(message, ##__VA_ARGS__); process_controller.set_state(STATE_STOPPED);}

void image_release(ncnn::Mat& image, int webp = 0)
{
    unsigned char* pixeldata = (unsigned char*)image.data;
    if (webp == 1)
    {
        free(pixeldata);
    }
    else
    {
        #ifdef _WIN32
        free(pixeldata);
        #else
        stbi_image_free(pixeldata);
        #endif
    }
    image.release();
}

struct CacheEntry {
    int id;
    ncnn::Mat image;
    int request;
    std::atomic<int> complete;
    int acquired = 0;
    int released = 0;
    int webp;
    int ret = -1;
};

class ImageCachePool
{
public:
    ~ImageCachePool()
    {
        cleanup();
    }

    void register_request_nolock(int id, const path_t& path, int count, int webp)
    {
        if (cache.find(path) == cache.end())
        {
            cache[path].id = id;
            cache[path].request = count;
            cache[path].complete = 0;
            cache[path].webp = webp;
        }
        else
        {
            cache[path].request += count;
        }
    }

    void register_request(int id, const path_t& path, int count, int webp)
    {
        pool_lock.lock();
        cache[path].id = id;
        cache[path].request = count;
        cache[path].complete = 0;
        cache[path].webp = webp;
        pool_lock.unlock();
    }

    int acquire(const path_t& path, ncnn::Mat& image, int* webp)
    {
        pool_lock.lock();
        auto& entry = cache[path];

        if (entry.released)
        {
            #ifdef _WIN32
            stop_with_errorw(L"Image pool: image %s acquired but it has been released", path.c_str());
            #else
            stop_with_error("Image pool: image %s acquired but it has been released", path.c_str());
            #endif
            pool_lock.unlock();
            return entry.ret;
        }

        if (!entry.acquired)
        {
            // #ifdef _WIN32
            // debug_outputw(L"Image pool: decode image %s\n", path.c_str());
            // #else
            // debug_output("Image pool: decode image %s\n", path.c_str());
            // #endif
            int ret = decode_image(path, entry.image, &entry.webp);
            entry.ret = ret;
            entry.acquired = 1;
        }
        
        image = entry.image;
        *webp = entry.webp;
        pool_lock.unlock();
        return entry.ret;
    }

    void release(const path_t& path) 
    {
        pool_lock.lock();
        auto& entry = cache[path];
        entry.complete.fetch_add(1, std::memory_order_relaxed);

        if (entry.released)
        {
            stop_with_error("Image pool: image %s markup but it has been released", path.c_str());
            pool_lock.unlock();
            return;
        }

        if (entry.complete.load(std::memory_order_relaxed) == entry.request)
        {
            // #ifdef _WIN32
            // debug_outputw(L"Image pool: releasing image %s\n", path.c_str());
            // #else
            // debug_output("Image pool: releasing image %s\n", path.c_str());
            // #endif
            entry.released = 1;
            image_release(entry.image, entry.webp);
        }

        pool_lock.unlock();
    }

    void cleanup()
    {
        // released all if any stop event caused
        for (auto& item : cache)
        {
            if (!item.second.image.empty())
            {
                image_release(item.second.image, item.second.webp);
            }
        }
    }
private:
    std::unordered_map<path_t, CacheEntry> cache;
    Mutex pool_lock;
};

ImageCachePool imagepool;

class Task
{
public:
    Task() {}
    Task(const Task& other) : 
        id(other.id),
        webp0(other.webp0),
        webp1(other.webp1),
        in0path(other.in0path),
        in1path(other.in1path),
        outpath(other.outpath),
        timestep(other.timestep),
        in0image(other.in0image),
        in1image(other.in1image),
        outimage(other.outimage){}
    Task& operator=(const Task& other)
    {
        if (this == &other)
            return *this;
        id = other.id;
        webp0 = other.webp0;
        webp1 = other.webp1;
        in0path = other.in0path;
        in1path = other.in1path;
        outpath = other.outpath;
        timestep = other.timestep;
        in0image = other.in0image;
        in1image = other.in1image;
        outimage = other.outimage;
        return *this;
    }

    ~Task()
    {
        in0image.release();
        in1image.release();
        outimage.release();
    }

    int id;
    int webp0;
    int webp1;

    path_t in0path;
    path_t in1path;
    path_t outpath;
    float timestep;

    ncnn::Mat in0image;
    ncnn::Mat in1image;
    ncnn::Mat outimage;
};

template<typename T>
class TaskQueue
{
public:
    TaskQueue(int size = 8)
    {
        if (size < 0) size = 8;
        _max_size = size;
    }

    void resize(int size)
    {
        lock.lock();
        _max_size = size;
        lock.unlock();
    }

    int size()
    {
        lock.lock();
        int ret = (int)tasks.size();
        lock.unlock();
        return ret;
    }

    int max_size()
    {
        lock.lock();
        int ret = _max_size;
        lock.unlock();
        return ret;
    }

    void put(const T& v)
    {
        lock.lock();

        while (tasks.size() >= _max_size && _max_size != 0)
        {
            condition.wait(lock);
        }

        tasks.push(v);

        lock.unlock();

        condition.signal();
    }

    void put_nowait(const T& v)
    {
        lock.lock();
        tasks.push(v);
        lock.unlock();
        condition.signal();
    }

    void get(T& v)
    {
        lock.lock();

        while (tasks.size() == 0)
        {
            condition.wait(lock);
        }

        v = tasks.front();
        tasks.pop();

        lock.unlock();

        condition.signal();
    }

    void pop()
    {
        lock.lock();
        while (tasks.size() == 0)
        {
            condition.wait(lock);
        }
        tasks.pop();
        lock.unlock();
        condition.signal();
    }

    bool empty()
    {
        lock.lock();
        bool ret = tasks.empty();
        lock.unlock();
        return ret;
    }

    bool full()
    {
        if (_max_size == 0) return false;
        lock.lock();
        bool ret = tasks.size() >= _max_size;
        lock.unlock();
        return ret;
    }

private:
    int _max_size;
    Mutex lock;
    ConditionVariable condition;
    std::queue<T> tasks;
};

TaskQueue<int> toload;
TaskQueue<Task> toproc;
TaskQueue<Task> tosave;
TaskQueue<Task> torawsave;

enum CheckStateResult {
    CHECK_OK,
    CHECK_INTERRUPTED,
    CHECK_STOPPED,
    CHECK_NEED_PAUSE
};

CheckStateResult check_state(bool pause_later = false)
{
    if (pause_later) return CHECK_NEED_PAUSE;
    if (process_controller.is_stopped_fast())
    {
        int state = process_controller.state();
        if (state == STATE_INTERRUPTED) return CHECK_INTERRUPTED;
        else if (state == STATE_STOPPED) return CHECK_STOPPED;
        else stop_with_error("check_state: unexpected state, state = %d", state); // should never happen
        return CHECK_STOPPED;
    }
    int state_transit = process_controller.state_transit();
    if (state_transit == TRANSIT_PAUSING) return CHECK_NEED_PAUSE;
    else if (state_transit == TRANSIT_RESUMING) return CHECK_OK;
    return CHECK_OK;
}

CheckStateResult check_state_stop(bool only_stop_flag = false)
{
    if (process_controller.is_stopped_fast())
    {
        int state = process_controller.state();
        if (state == STATE_INTERRUPTED) return CHECK_INTERRUPTED;
        else if (state == STATE_STOPPED) return CHECK_STOPPED;
        else stop_with_error("check_state: unexpected state, state = %d", state); // should never happen
        return CHECK_STOPPED;
    }
    return CHECK_OK;
}

CheckStateResult check_state_transit(bool only_stop_flag = false)
{
    int state_transit = process_controller.state_transit();
    if (state_transit == TRANSIT_PAUSING) return CHECK_NEED_PAUSE;
    return CHECK_OK;
}

class LoadThreadParams
{
public:
    int raw_output;
    // session data
    std::vector<path_t> input0_files;
    std::vector<path_t> input1_files;
    std::vector<path_t> output_files;
    std::vector<float> timesteps;
    std::vector<bool> bitmap;
};

void* load(void* args)
{
    debug_output("load thread started\n");
    process_controller.request_increase();

    const LoadThreadParams* ltp = (const LoadThreadParams*)args;

    for (;;)
    {
        switch (check_state())
        {
        case CHECK_STOPPED:
            goto load_stopped;
        case CHECK_INTERRUPTED:
            goto load_interrupted;
        case CHECK_NEED_PAUSE:
            debug_output("load thread: into pause\n");
            process_controller.wait_resume();
            debug_output("load thread: wait resumed");
            break;
        case CHECK_OK:
            break;
        }

        if (toload.empty()) break;

        int i;
        toload.get(i);

        if (ltp->raw_output && ltp->bitmap[i]) // FIXME
        {
            // 加载已经存在的输出图像输出到stdout
            Task v;
            v.id = i;

            int ret = decode_image(ltp->output_files[i], v.outimage, nullptr);
            if (ret == 0)
            {
                torawsave.put(v);
            }
        }
        else
        {
            const path_t& image0path = ltp->input0_files[i];
            const path_t& image1path = ltp->input1_files[i];
    
            Task v;
            v.id = i;
            v.in0path = image0path;
            v.in1path = image1path;
            v.outpath = ltp->output_files[i];
            v.timestep = ltp->timesteps[i];
    
            // debug_output("load thread: load %d\n", v.id);
            int ret0 = imagepool.acquire(v.in0path, v.in0image, &v.webp0);
            int ret1 = imagepool.acquire(v.in1path, v.in1image, &v.webp1);
    
            if (ret0 == 0 && ret1 == 0)
            {
                v.outimage = ncnn::Mat(v.in0image.w, v.in0image.h, (size_t)3, 3);
                toproc.put(v);
            }
            else
            {
                imagepool.release(image0path);
                imagepool.release(image1path);
            }
        }
        progress.update(1, 0, 0);
    }

    debug_output("load thread finished\n");
    goto load_cleanup;
load_interrupted:
    debug_output("load thread interrupted\n");
    goto load_cleanup;
load_stopped:
    debug_output("load thread stopped\n");
load_cleanup:
    process_controller.request_decrease();
    return 0;
}

class ProcThreadParams
{
public:
    const RIFE* rife;
    int raw_output;
};

void* proc(void* args)
{
    debug_output("proc thread started\n");
    process_controller.request_increase();

    const ProcThreadParams* ptp = (const ProcThreadParams*)args;
    const RIFE* rife = ptp->rife;

    for (;;)
    {
        bool pause_later = false;
        switch (check_state())
        {
        case CHECK_STOPPED:
            goto proc_stopped;
        case CHECK_INTERRUPTED:
            goto proc_interrupted;
        case CHECK_NEED_PAUSE:
            pause_later = true;
            break;
        case CHECK_OK:
            break;
        }

        // toproc empty       原地暂停
        // toproc full|other  释放可能的put阻塞后暂停
        if (pause_later && toproc.empty())
        {
            debug_output("proc thread: into pause 1\n");
            process_controller.wait_resume();
            debug_output("proc thread: wait resumed 1");
            pause_later = false;
        }

        Task v;
        toproc.get(v);

        if (v.id == -233)
            break;

        switch (check_state(pause_later))
        {
        case CHECK_STOPPED:
            goto proc_stopped;
        case CHECK_INTERRUPTED:
            goto proc_interrupted;
        case CHECK_NEED_PAUSE:
            debug_output("proc thread: into pause 2\n");
            process_controller.wait_resume();
            debug_output("proc thread: wait resumed 2");
            break;
        case CHECK_OK:
            break;
        }

        rife->process(v.in0image, v.in1image, v.timestep, v.outimage);

        if (ptp->raw_output)
        {
            torawsave.put(v);
        }
        else
        {
            tosave.put(v);
        }
        progress.update(0, 1, 0);
        // std::this_thread::sleep_for(std::chrono::seconds(1)); // dbg slow down load thread for debug
    }

    debug_output("proc thread finished\n");
    goto proc_cleanup;
proc_interrupted:
    debug_output("proc thread interrupted\n");
    goto proc_cleanup;
proc_stopped:
    debug_output("proc thread stopped\n");
proc_cleanup:
    process_controller.request_decrease();
    return 0;
}

class SaveThreadParams
{
public:
    int verbose;
};

void* save(void* args)
{
    debug_output("save thread started\n");
    process_controller.request_increase();

    const SaveThreadParams* stp = (const SaveThreadParams*)args;
    const int verbose = stp->verbose;
    for (;;)
    {
        bool pause_later = false;
        switch (check_state())
        {
        case CHECK_STOPPED:
            goto save_stopped;
        case CHECK_INTERRUPTED:
            goto save_interrupted;
        case CHECK_NEED_PAUSE:
            pause_later = true;
            break;
        case CHECK_OK:
        break;
    }
    
        if (pause_later && tosave.empty())
        {
            debug_output("save thread: into pause 1\n");
            process_controller.wait_resume();
            debug_output("save thread: wait resumed 1");
            pause_later = false;
        }

        Task v;
        tosave.get(v);

        if (v.id == -233)
            break;

        switch (check_state_transit(pause_later)) // 对于save线程保证哪怕接受到中断信号也要将当前任务完成
        {
        case CHECK_NEED_PAUSE:
            debug_output("save thread: into pause 2\n");
            process_controller.wait_resume();
            debug_output("save thread: wait resumed 2");
            break;
        default:
            break;
        }

        int ret = encode_image(v.outpath, v.outimage);
        imagepool.release(v.in0path);
        imagepool.release(v.in1path);
        if (ret == 0)
        {
            if (verbose)
            {
                #if _WIN32
                wprint(L"%ls %ls %f -> %ls done\n", v.in0path.c_str(), v.in1path.c_str(), v.timestep, v.outpath.c_str());
                #else
                print("%s %s %f -> %s done\n", v.in0path.c_str(), v.in1path.c_str(), v.timestep, v.outpath.c_str());
                #endif
            }
        }
        progress.update(0, 0, 1);
    }

    debug_output("save thread finished\n");
    goto save_cleanup;
save_interrupted:
    debug_output("save thread interrupted\n");
    goto save_cleanup;
save_stopped:
    debug_output("save thread stopped\n");
save_cleanup:
    process_controller.request_decrease();
    return 0;
}

class RawSaveThreadParams
{
public:
    int verbose;
    int start_id;
    int total;
};

static void write_bgr24_to_stdout(int w, int h, int c, void* bgrdata)
    {
        int stride = (w * c * 8 + 7) / 8;
        unsigned char* data = 0;
        data = (unsigned char*)malloc(h * stride);
        if (!data)
        {
            stop_with_error("\nError: Failed to allocate memory for image data.");
            return;
        }

        for (int y = 0; y < h; y++)
        {
            const unsigned char* bgrptr = (const unsigned char*)bgrdata + y * w * c;
            unsigned char* ptr = data + y * stride;
            memcpy(ptr, bgrptr, w * c);
        }
        
        // write bgr buffer to stdout with broken pipe error handling
        size_t bytes_to_write = (size_t)h * stride;
        size_t written = fwrite(data, 1, bytes_to_write, stdout);
        if (written != bytes_to_write)
        {
            // Write failed - likely ffmpeg closed the pipe
            stop_with_error("\nError: Write to stdout failed (written %zu of %zu bytes).", 
                    written, bytes_to_write);
            fflush(stderr);
            free(data);
            return;
        }
        
        int flush_ret = fflush(stdout);
        if (flush_ret != 0)
        {
            stop_with_error("\nError: fflush(stdout) failed (errno: %d).", errno);
            fflush(stderr);
            free(data);
            return;
        }

        free(data);
    }

void* raw_save(void* args)
{
    debug_output("raw save thread started\n");
    process_controller.request_increase();

    const RawSaveThreadParams* rstp = (const RawSaveThreadParams*)args;
    const int start_id = rstp->start_id;
    const int total = rstp->total;
    const int verbose = rstp->verbose;
    int offset = start_id;
    int last_id = -1;
    std::vector<int> pending_ids;
    std::vector<ncnn::Mat> pending_images;
    pending_images.resize(total-start_id);
    for (;;)
    {
        bool pause_later = false;
        switch (check_state())
        {
        case CHECK_STOPPED:
            goto raw_save_stopped;
        case CHECK_INTERRUPTED:
            goto raw_save_interrupted;
        case CHECK_NEED_PAUSE:
            pause_later = true;
            break;
        case CHECK_OK:
            break;
        }

        if (pause_later && torawsave.empty())
        {
            debug_output("raw save thread: into pause 1\n");
            process_controller.wait_resume();
            debug_output("raw save thread: wait resumed 1");
            pause_later = false;
        }

        Task v;
        torawsave.get(v);

        if (v.id == -233)
            break;

        switch (check_state_transit(pause_later))
        {
        case CHECK_NEED_PAUSE:
            debug_output("raw save thread: into pause 2\n");
            process_controller.wait_resume();
            debug_output("raw save thread: wait resumed 2");
            break;
        default:
            break;
        }

        int now_id = v.id - offset;

        pending_images[now_id] = v.outimage;
        pending_ids.push_back(now_id);
        if (now_id == last_id + 1)
        {
            std::sort(pending_ids.begin(), pending_ids.end(), std::greater<int>());
            int i = pending_ids.size() - 1;
            while (i >= 0)
            {
                int _id = pending_ids[i];
                if (_id != last_id + 1) break;
                pending_ids.pop_back();
                last_id = _id;
                #ifndef NO_STDOUT_OUTPUT
                write_bgr24_to_stdout(
                    pending_images[_id].w,
                    pending_images[_id].h,
                    pending_images[_id].elempack,
                    pending_images[_id].data
                );
                #endif
                // image_release(pending_images[_id]);
                pending_images[_id].release();
                i--;
                if (verbose)
                {
                    print("raw saved to stdout: %d\n", _id);
                }
            }
        }

        imagepool.release(v.in0path);
        imagepool.release(v.in1path);
        progress.update(0, 0, 1);
    }
    debug_output("raw save thread finished\n");
    goto raw_save_cleanup;
raw_save_interrupted:
    debug_output("raw save thread interrupted\n");
    goto raw_save_cleanup;
raw_save_stopped:
    debug_output("raw save thread stopped\n");
raw_save_cleanup:
    for (int i = 0; i < pending_images.size(); i++)
    {
        image_release(pending_images[i]);
    }
    process_controller.request_decrease();
    return 0;
}


#ifdef _WIN32
class InputThreadParams
{
public:
    HANDLE hStdIn;
};
#endif

void* input_loop(void* args)
{
    debug_output("input loop started\n");

    #ifdef _WIN32
    const InputThreadParams* itp = (const InputThreadParams*)args;
    console_init(itp->hStdIn);
    #else
    console_init();
    #endif

    int q_pressed = 0;
    int p_pressed = 0;
    double q_pressed_last = 0;
    double p_pressed_last = 0;
    for (;;)
    {
        switch (check_state_stop())
        {
        case CHECK_STOPPED:
            goto input_loop_stopped;
        case CHECK_INTERRUPTED:
            goto input_loop_interrupted;
        default:
            break;
        }
        #ifdef _WIN32
        int key = key_press(itp->hStdIn);
        #else
        int key = key_press();
        #endif
        debug_output("key pressed: %d\n", key);
        if (key == KEY_Q) // quit
        {
            if (q_pressed && get_timestamp() - q_pressed_last < 0.5)
            {
                process_controller.set_state(STATE_STOPPED);
                debug_output("stop signal received, stop all tasks...\n");
                q_pressed = 0;
                break;
            }
            else
            {
                q_pressed = 1;
            }
            q_pressed_last = get_timestamp();
        }
        else if (key == KEY_P) // pause/resume
        {
            if (p_pressed && get_timestamp() - p_pressed_last < 0.5)
            {
                if (process_controller.state_transit() == TRANSIT_NONE)
                {
                    int state = process_controller.state();
                    if (state == STATE_PAUSED)
                    {
                        debug_output("resume signal received, resume all tasks...\n");
                        process_controller.set_state(STATE_PAUSED, false);
                    }
                    else
                    {
                        debug_output("pause signal received, pause all tasks...\n");
                        process_controller.set_state(STATE_PAUSED, true);
                    }
                }
                else
                {
                    debug_output("pause signal received, but state is changing, ignore...\n");
                }
                p_pressed = 0;
            }
            else
            {
                p_pressed = 1;
            }
            p_pressed_last = get_timestamp();
        }
    }

    debug_output("input loop finished\n");
    return 0;
input_loop_interrupted:
    debug_output("input loop interrupted\n");
    return 0;
input_loop_stopped:
    debug_output("input loop stopped\n");
    return 0;
}

void* progress_auto_fresh(void* args)
{
    if (!progress.enabled) return 0;

    debug_output("progress auto fresh thread started\n");

    // TODO: modify progress interval dynamically
    float auto_fresh_interval = 1.0f; 

    for (;;)
    {
        switch (check_state_stop())
        {
        case CHECK_STOPPED:
            goto progress_auto_fresh_stopped;
        case CHECK_INTERRUPTED:
            goto progress_auto_fresh_interrupted;
        default:
            break;
        }

        double elapsed_time = progress.get_time_elapsed();
        int ms = static_cast<int>((elapsed_time - floor(elapsed_time)) * 1000);
        int next = auto_fresh_interval * 1000 - ms;

        if (next > 0)
        {
            int ret = process_controller.wait_state(
                STATE_STOPPED | STATE_INTERRUPTED,
                static_cast<unsigned long>(next));
            if (ret == STATE_STOPPED)     goto progress_auto_fresh_stopped;
            if (ret == STATE_INTERRUPTED) goto progress_auto_fresh_interrupted;
        }

        progress.update();
    }

    debug_output("progress auto fresh thread finished\n");
    return 0;
progress_auto_fresh_interrupted:
    debug_output("progress auto fresh thread interrupted\n");
    return 0;
progress_auto_fresh_stopped:
    debug_output("progress auto fresh thread stopped\n");
    return 0;
}

int get_num(const path_t& path)
{
#if _WIN32
    std::wregex num_regex(L"\\d+");
    std::wsmatch match;
    if (std::regex_search(path, match, num_regex)) {
        return std::stoi(match.str());
    }
#else // _WIN32
    std::regex num_regex("\\d+");
    std::smatch match;
    if (std::regex_search(path, match, num_regex)) {
        return std::stoi(match.str());
    }
#endif // _WIN32
    return 0;
}

void existence_bitmap(const std::vector<path_t>& files, std::vector<bool>& bitmap) {
    if (files.empty()) return;
    for (int i = 0; i < files.size(); i++) {
        int num = get_num(files[i]);
        int offset = num - 1;
        if (offset < 0 || offset >= bitmap.size()) {
            #if _WIN32
            rwprint(L"file %ls has no corresponding output file\n", files[i].c_str());
            #else // _WIN32
            print("file %s has no corresponding output file\n", files[i].c_str());
            #endif // _WIN32
        } else {
            bitmap[offset] = true;
        }
    }
}

void sigint_handler(int signal)
{
    if (signal == SIGINT)
    {
        int state = check_state();
        if (state == STATE_INTERRUPTED) 
        {
            // second interrupt, terminate immediately.
            print("terminate immediately\n");
            exit(1);
        }
        else if (state != STATE_INTERRUPTED)
        {
            process_controller.set_state(STATE_INTERRUPTED);
            print("\ninterrupt signal received, cancel all tasks...\n");
        }
    }
}


#if _WIN32
int wmain(int argc, wchar_t** argv)
#else
int main(int argc, char** argv)
#endif
{
    std::signal(SIGINT, sigint_handler);

    path_t input0path;
    path_t input1path;
    path_t inputpath;
    path_t outputpath;
    int numframe = 0;
    float timestep = 0.5f;
    path_t model = PATHSTR("rife-v2.3");
    std::vector<int> gpuid;
    int jobs_load = 1;
    std::vector<int> jobs_proc;
    int jobs_save = 2;
    int raw_output = 0;
    int verbose = 0;
    int tta_mode = 0;
    int tta_temporal_mode = 0;
    int uhd_mode = 0;
    path_t pattern_format = PATHSTR("%08d.png");
    int enable_progress = 1;
    float progress_interval = 0.5f;
    int debug = 0;

#if _WIN32
    setlocale(LC_ALL, "");
    wchar_t opt;
    while ((opt = getopt(argc, argv, L"0:1:i:o:n:s:m:g:j:f:p:t:rvxzudh")) != (wchar_t)-1)
    {
        switch (opt)
        {
        case L'0':
            input0path = optarg;
            break;
        case L'1':
            input1path = optarg;
            break;
        case L'i':
            inputpath = optarg;
            break;
        case L'o':
            outputpath = optarg;
            break;
        case L'n':
            numframe = _wtoi(optarg);
            break;
        case L's':
            timestep = _wtof(optarg);
            break;
        case L'm':
            model = optarg;
            break;
        case L'g':
            gpuid = parse_optarg_int_array(optarg);
            break;
        case L'j':
            swscanf(optarg, L"%d:%*[^:]:%d", &jobs_load, &jobs_save);
            jobs_proc = parse_optarg_int_array(wcschr(optarg, L':') + 1);
            break;
        case L'r':
            raw_output = 1;
            break;
        case L'f':
            pattern_format = optarg;
            break;
        case L'p':
            enable_progress = _wtoi(optarg);
            break;
        case L't':
            progress_interval = _wtof(optarg);
            break;
        case L'v':
            verbose = 1;
            break;
        case L'x':
            tta_mode = 1;
            break;
        case L'z':
            tta_temporal_mode = 1;
            break;
        case L'u':
            uhd_mode = 1;
            break;
        case L'd':
            debug = 1;
            break;
        case L'h':
        default:
            print_usage();
            return -1;
        }
    }
#else // _WIN32
    int opt;
    while ((opt = getopt(argc, argv, "0:1:i:o:n:s:m:g:j:f:vxzuh")) != -1)
    {
        switch (opt)
        {
        case '0':
            input0path = optarg;
            break;
        case '1':
            input1path = optarg;
            break;
        case 'i':
            inputpath = optarg;
            break;
        case 'o':
            outputpath = optarg;
            break;
        case 'n':
            numframe = std::atoi(optarg);
            break;
        case 's':
            timestep = std::atof(optarg);
            break;
        case 'm':
            model = optarg;
            break;
        case 'g':
            gpuid = parse_optarg_int_array(optarg);
            break;
        case 'j':
            sscanf(optarg, "%d:%*[^:]:%d", &jobs_load, &jobs_save);
            jobs_proc = parse_optarg_int_array(strchr(optarg, ':') + 1);
            break;
        case 'r':
            raw_output = 1;
            break;
        case 'f':
            pattern_format = optarg;
            break;
        case 'p':
            enable_progress = std::stoi(optarg);
            break;
        case 't':
            progress_interval = std::stof(optarg);
            break;
        case 'v':
            verbose = 1;
            break;
        case 'x':
            tta_mode = 1;
            break;
        case 'z':
            tta_temporal_mode = 1;
            break;
        case 'u':
            uhd_mode = 1;
            break;
        case 'd':
            debug = std::stoi(optarg);
            break;
        case 'h':
        default:
            print_usage();
            return -1;
        }
    }
#endif // _WIN32

    gdebug = debug;

    debug_output("rife-ncnn-vulkan-ex start\n");

    if (((input0path.empty() || input1path.empty()) && inputpath.empty()) || (!raw_output && outputpath.empty()))
    {
        print_usage();
        return -1;
    }

    if (inputpath.empty() && (timestep <= 0.f || timestep >= 1.f))
    {
        rprint("invalid timestep argument, must be 0~1\n");
        return -1;
    }

    if (!inputpath.empty() && numframe < 0)
    {
        rprint("invalid numframe argument, must not be negative\n");
        return -1;
    }

    if (jobs_load < 1 || jobs_save < 1)
    {
        rprint("invalid thread count argument\n");
        return -1;
    }

    if (jobs_proc.size() != (gpuid.empty() ? 1 : gpuid.size()) && !jobs_proc.empty())
    {
        rprint("invalid jobs_proc thread count argument\n");
        return -1;
    }

    for (int i=0; i<(int)jobs_proc.size(); i++)
    {
        if (jobs_proc[i] < 1)
        {
            rprint("invalid jobs_proc thread count argument\n");
            return -1;
        }
    }

    path_t pattern = get_file_name_without_extension(pattern_format);
    path_t format = get_file_extension(pattern_format);

    if (format.empty())
    {
        pattern = PATHSTR("%08d");
        format = pattern_format;
    }

    if (pattern.empty())
    {
        pattern = PATHSTR("%08d");
    }

    if (!outputpath.empty() && !path_is_directory(outputpath))
    {
        // guess format from outputpath no matter what format argument specified
        path_t ext = get_file_extension(outputpath);

        if (ext == PATHSTR("png") || ext == PATHSTR("PNG"))
        {
            format = PATHSTR("png");
        }
        else if (ext == PATHSTR("webp") || ext == PATHSTR("WEBP"))
        {
            format = PATHSTR("webp");
        }
        else if (ext == PATHSTR("jpg") || ext == PATHSTR("JPG") || ext == PATHSTR("jpeg") || ext == PATHSTR("JPEG"))
        {
            format = PATHSTR("jpg");
        }
        else if (raw_output)
        {
            format = PATHSTR("");
        }
        else
        {
            rprint("invalid outputpath extension type\n");
            return -1;
        }
    }

    if (!raw_output && (format != PATHSTR("png") && format != PATHSTR("webp") && format != PATHSTR("jpg")))
    {
        rprint("invalid format argument\n");
        return -1;
    }

    bool rife_v2 = false;
    bool rife_v4 = false;
    int padding = 32;
    if (model.find(PATHSTR("rife-v2")) != path_t::npos)
    {
        // fine
        rife_v2 = true;
    }
    else if (model.find(PATHSTR("rife-v3.6")) != path_t::npos)
    {
        rife_v4 = true;
    }
    else if (model.find(PATHSTR("rife-v3.9")) != path_t::npos)
    {
        rife_v4 = true;
    }
    else if (model.find(PATHSTR("rife-v3")) != path_t::npos)
    {
        // fine
        rife_v2 = true;
    }
    else if (model.find(PATHSTR("rife-v4")) != path_t::npos)
    {
        // fine
        rife_v4 = true;
        if (model.find(PATHSTR("rife-v4.25")) != path_t::npos)
        {
            padding = 64;
        }
        if (model.find(PATHSTR("rife-v4.25-lite")) != path_t::npos)
        {
            padding = 128;
        }
        if (model.find(PATHSTR("rife-v4.26")) != path_t::npos)
        {
            padding = 64;
        }
    }
    else if (model.find(PATHSTR("rife-v5")) != path_t::npos)
    {
        // fine
        rife_v4 = true;
    }
    else if (model.find(PATHSTR("rife")) != path_t::npos)
    {
        // fine
    }
    else
    {
        rprint("unknown model dir type\n");
        return -1;
    }

    if (!rife_v4 && (numframe != 0 || timestep != 0.5))
    {
        rprint("only rife-v4 model support custom numframe and timestep\n");
        return -1;
    }

    // collect input and output filepath
    std::vector<path_t> input0_files;
    std::vector<path_t> input1_files;
    std::vector<path_t> output_files;
    std::vector<float> timesteps;
    std::vector<bool> bitmap; // 记录输出文件是否存在
    size_t total = 0;
    size_t skipped = 0;
    int first_task_id = -1;
    {
        if (!inputpath.empty() && path_is_directory(inputpath) && (raw_output || path_is_directory(outputpath)))
        {
            std::vector<path_t> exists_output_files;
            if (!outputpath.empty())
            {
                int lr_o = list_directory(outputpath, exists_output_files);
                if (lr_o != 0 && !raw_output)
                    return -1;
            }
            std::vector<path_t> filenames;
            int lr = list_directory(inputpath, filenames);
            if (lr != 0)
                return -1;

            const int count = filenames.size();
            if (numframe == 0)
                numframe = count * 2;
            bitmap.resize(numframe);
            existence_bitmap(exists_output_files, bitmap);
            input0_files.resize(numframe);
            input1_files.resize(numframe);
            output_files.resize(numframe);
            timesteps.resize(numframe);
            total = numframe;

            double scale = (double)count / numframe;
            for (int i=0; i<numframe; i++)
            {
                // 原始输出模式下，需要将已经存在的文件加载并写入stdout中
                if (bitmap[i] && !raw_output) {
                    skipped++;
                    continue;
                }
                if (first_task_id == -1) first_task_id = i;
                toload.put_nowait(i);
                // TODO provide option to control timestep interpolate method
                // float fx = (float)((i + 0.5) * scale - 0.5);
                float fx = i * scale;
                int sx = static_cast<int>(floor(fx));
                fx -= sx;

                if (sx < 0)
                {
                    sx = 0;
                    fx = 0.f;
                }
                if (sx >= count - 1)
                {
                    sx = count - 2;
                    fx = 1.f;
                }

//                 rprint("%d %f %d\n", i, fx, sx);

                path_t filename0 = filenames[sx];
                path_t filename1 = filenames[sx + 1];

#if _WIN32
                wchar_t tmp[256];
                swprintf(tmp, pattern.c_str(), i+1);
#else
                char tmp[256];
                sprintf(tmp, pattern.c_str(), i+1); // ffmpeg start from 1
#endif
                path_t output_filename = path_t(tmp) + PATHSTR('.') + format;

                input0_files[i] = inputpath + PATHSTR('/') + filename0;
                input1_files[i] = inputpath + PATHSTR('/') + filename1;
                output_files[i] = outputpath + PATHSTR('/') + output_filename;
                imagepool.register_request_nolock(i, input0_files[i], 1, get_file_extension(filename0) == PATHSTR("webp") ? 1 : 0);
                imagepool.register_request_nolock(i+numframe, input1_files[i], 1, get_file_extension(filename1) == PATHSTR("webp") ? 1 : 0);
                timesteps[i] = fx;
            }
        }
        else if (inputpath.empty() && !path_is_directory(input0path) && !path_is_directory(input1path) && (!path_is_directory(outputpath) || raw_output))
        {
            input0_files.push_back(input0path);
            input1_files.push_back(input1path);
            output_files.push_back(outputpath);
            timesteps.push_back(timestep);
            toload.put_nowait(0);
            imagepool.register_request_nolock(0, input0path, 1, get_file_extension(input0path) == PATHSTR("webp") ? 1 : 0);
            imagepool.register_request_nolock(1, input1path, 1, get_file_extension(input1path) == PATHSTR("webp") ? 1 : 0);
            first_task_id = 0;
            total++;
        }
        else
        {
            rprint("input0path, input1path and outputpath must be file at the same time\n");
            rprint("inputpath and outputpath must be directory at the same time\n");
            return -1;
        }
    }

    if (!enable_progress) 
    {
        progress.enabled = false;
    }
    else
    {
        // hide cursor
        progress.console.show_cursor(false);
        process_controller.register_callback("ProgressInfoUpdate", 
            STATE_PAUSED | TRANSIT_PAUSING | TRANSIT_RESUMING | STATE_STOPPED | STATE_INTERRUPTED | STATE_RUNNING,
            [](int state)
            {
                switch(state)
                {
                case STATE_PAUSED:
                    progress.on_paused();
                    progress.refresh(true, " (paused)");
                    break;
                case STATE_RUNNING:
                    progress.on_resumed();
                    break;
                case TRANSIT_PAUSING:
                    progress.refresh(true, " (pausing)");
                    break;
                case TRANSIT_RESUMING:
                    progress.refresh(true, " (resuming)");
                    break;
                case STATE_STOPPED:
                    progress.refresh(true, " (stopped)");
                    break;
                case STATE_INTERRUPTED:
                    progress.refresh(true, " (interrupted)");
                    break;
                default:
                    break; // never reach
                }
            });
        process_controller.register_callback("ProgressShowCursor",
            STATE_STOPPED | STATE_INTERRUPTED,
            [](int state)
            {
                progress.console.show_cursor(true);
            });
    }
    progress.interval = progress_interval;
    progress.set_total(total);
    if (skipped > 0) 
    {
        progress.update(skipped, skipped, skipped, true);
        rprint("skipped %d frames\n", skipped);
    }

    path_t modeldir = sanitize_dirpath(model);

#if _WIN32
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

    #ifdef _WIN32 // windows use _setmode to set binary mode for stdout
    if (raw_output)
    {
        #include <fcntl.h>
        _setmode(_fileno(stdout), _O_BINARY);
    }
    #endif

    process_controller.register_callback("ImagePoolCleanUp", STATE_STOPPED | STATE_INTERRUPTED,
                                        [](int state) { imagepool.cleanup(); });

    ncnn::create_gpu_instance();

    if (gpuid.empty())
    {
        gpuid.push_back(ncnn::get_default_gpu_index());
    }

    const int use_gpu_count = (int)gpuid.size();

    if (jobs_proc.empty())
    {
        jobs_proc.resize(use_gpu_count, 2);
    }

    int cpu_count = std::max(1, ncnn::get_cpu_count());
    jobs_load = std::min(jobs_load, cpu_count);
    jobs_save = std::min(jobs_save, cpu_count);

    int gpu_count = ncnn::get_gpu_count();
    for (int i=0; i<use_gpu_count; i++)
    {
        if (gpuid[i] < -1 || gpuid[i] >= gpu_count)
        {
            rprint("invalid gpu device\n");

            ncnn::destroy_gpu_instance();
            return -1;
        }
    }

    int total_jobs_proc = 0;
    for (int i=0; i<use_gpu_count; i++)
    {
        if (gpuid[i] == -1)
        {
            jobs_proc[i] = std::min(jobs_proc[i], cpu_count);
            total_jobs_proc += 1;
        }
        else
        {
            total_jobs_proc += jobs_proc[i];
        }
    }

    if (raw_output)
    {
        jobs_save = 1;
        if (total_jobs_proc > torawsave.max_size())
        {
            torawsave.resize(total_jobs_proc);
        }
    }
    else if (jobs_save > tosave.max_size())
    {
        tosave.resize(jobs_save);
    }
    if (total_jobs_proc > toproc.max_size())
    {
        toproc.resize(total_jobs_proc);
    }
    if (jobs_load > toload.max_size())
    {
        toload.resize(jobs_load);
    }

    // 当使用 '|' 输出到 stdout 时，CTRL+C 会直接中断程序组，导致无法安全退出
    // 使用 [q] 安全退出，这样ffmpeg才会完成视频的封装，而不是被强制中断
    rprint("\n");
    rprint("Press [q] twice to safe quit (ffmpeg may not finish the video when [ctrl+c])\n");
    rprint("      [p] twice to pause/resume\n\n"); // test now

    {
        process_controller.set_state(STATE_RUNNING);

        ncnn::Thread progress_auto_fresh_thread = ncnn::Thread(progress_auto_fresh, 0);

        #ifdef _WIN32
        HANDLE hStdIn = GetStdHandle(STD_INPUT_HANDLE);
        InputThreadParams itp;
        itp.hStdIn = hStdIn;
        ncnn::Thread input_loop_thread = ncnn::Thread(input_loop, (void*)&itp);
        #else
        ncnn::Thread input_loop_thread = ncnn::Thread(input_loop, 0);
        #endif

        std::vector<RIFE*> rife(use_gpu_count);

        for (int i=0; i<use_gpu_count; i++)
        {
            int num_threads = gpuid[i] == -1 ? jobs_proc[i] : 1;

            rife[i] = new RIFE(gpuid[i], tta_mode, tta_temporal_mode, uhd_mode, num_threads, rife_v2, rife_v4, padding);

            rife[i]->load(modeldir);
        }

        // main routine
        {
            // load image
            LoadThreadParams ltp;
            ltp.input0_files = input0_files;
            ltp.input1_files = input1_files;
            ltp.output_files = output_files;
            ltp.timesteps = timesteps;
            ltp.bitmap = bitmap;

            std::vector<ncnn::Thread*> load_threads(jobs_load);
            {
                for (int i=0; i<jobs_load; i++)
                {
                    load_threads[i] = new ncnn::Thread(load, (void*)&ltp);
                }
            }

            // rife proc
            std::vector<ProcThreadParams> ptp(use_gpu_count);
            for (int i=0; i<use_gpu_count; i++)
            {
                ptp[i].rife = rife[i];
                ptp[i].raw_output = raw_output;
            }

            std::vector<ncnn::Thread*> proc_threads(total_jobs_proc);
            {
                int total_jobs_proc_id = 0;
                for (int i=0; i<use_gpu_count; i++)
                {
                    if (gpuid[i] == -1)
                    {
                        proc_threads[total_jobs_proc_id++] = new ncnn::Thread(proc, (void*)&ptp[i]);
                    }
                    else
                    {
                        for (int j=0; j<jobs_proc[i]; j++)
                        {
                            proc_threads[total_jobs_proc_id++] = new ncnn::Thread(proc, (void*)&ptp[i]);
                        }
                    }
                }
            }

            // raw save
            std::vector<ncnn::Thread*> save_threads(jobs_save);
            if (raw_output)
            {
                RawSaveThreadParams rstp;
                rstp.verbose = verbose;
                rstp.start_id = first_task_id;
                rstp.total = total;

                save_threads[0] = new ncnn::Thread(raw_save, (void*)&rstp);
            }
            else
            {
                // save image
                SaveThreadParams stp;
                stp.verbose = verbose;
    
                for (int i=0; i<jobs_save; i++)
                {
                    save_threads[i] = new ncnn::Thread(save, (void*)&stp);
                }
            }

            Task end;
            end.id = -233;

            process_controller.register_callback("MainRoutineEnd", 
                STATE_STOPPED | STATE_INTERRUPTED, 
                [end, jobs_save, total_jobs_proc, jobs_load, raw_output]
                (int state)
                {
                    if (raw_output)
                    {
                        if (torawsave.empty()) // release rawsave->torawsave.get
                        {
                            for (int i = 0; i < jobs_save; i++) 
                            {
                                torawsave.put_nowait(end);
                            }
                        }
                        if (torawsave.full()) // release load->torawsave.put
                        {
                            for (;;)
                            {
                                if (torawsave.empty()) break;
                                torawsave.pop();
                            }
                        }
                    }
                    else
                    {
                        if (tosave.empty()) // release save->tosave.get
                        {
                            for (int i = 0; i < jobs_save; i++) 
                            {
                                tosave.put_nowait(end);
                            }
                        }
                        if (tosave.full()) // release load->tosave.put
                        {
                            for (;;)
                            {
                                if (tosave.empty()) break;
                                tosave.pop();
                            }
                        }
                    }
                    if (toproc.empty()) // release proc->toproc.get
                    {
                        for (int i = 0; i < total_jobs_proc; i++)
                        {
                            toproc.put_nowait(end);
                        }
                    }
                    if (toproc.full()) // release load->toproc.put
                    {
                        for (;;) {
                            if (toproc.empty()) break;
                            toproc.pop();
                        }
                    }
                });

            // end
            for (int i=0; i<jobs_load; i++)
            {
                load_threads[i]->join();
                delete load_threads[i];
            }
            debug_output("load thread joined\n");

            for (int i=0; i<total_jobs_proc; i++)
            {
                toproc.put_nowait(end);
            }
            for (int i=0; i<total_jobs_proc; i++)
            {
                proc_threads[i]->join();
                delete proc_threads[i];
            }
            debug_output("proc thread joined\n");

            for (int i=0; i<jobs_save; i++)
            {
                if (raw_output)
                {
                    torawsave.put_nowait(end);
                }
                else
                {
                    tosave.put_nowait(end);
                }
            }
            for (int i=0; i<jobs_save; i++)
            {
                save_threads[i]->join();
                delete save_threads[i];
            }
            debug_output("save thread joined\n");
        }

        for (int i=0; i<use_gpu_count; i++)
        {
            delete rife[i];
        }
        rife.clear();

        process_controller.set_state(STATE_STOPPED);

        progress_auto_fresh_thread.join();

        #ifdef _WIN32
        CancelIoEx(hStdIn, NULL); // cancel input thread blocking read
        input_loop_thread.join();
        #else
        close(fileno(stdin)); // close stdin to unblock input thread !! force
        input_loop_thread.join();
        #endif
    }

    ncnn::destroy_gpu_instance();

    return 0;
}
