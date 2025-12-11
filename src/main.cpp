// rife implemented with ncnn library

#include <io.h>
#include <stdio.h>
#include <map>
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


bool gdebug = false;

std::atomic<bool> g_interrupted(false);   // ev 1
std::atomic<bool> g_stopped(false);       // ev 2
std::vector<std::function<void()>> g_stop_callbacks;

void print(const char* message, ...)
{
    va_list args;
    va_start(args, message);
    if (va_arg(args, void*) == nullptr) {
        fprintf(stderr, "%s\n", message);
    } else {
        va_start(args, message);
        va_list args_copy;
        va_copy(args_copy, args);
        size_t len = vsnprintf(nullptr, 0, message, args_copy) + 1;  // +1 for the null terminator
        va_end(args_copy);
        if (len > 0) {
            char* buffer = new char[len];
            vsnprintf(buffer, len, message, args);
            fprintf(stderr, "%s\n", buffer);
            delete[] buffer;
        }
    }
    va_end(args);
}

void wprint(const wchar_t* message, ...)
{
    va_list args;
    va_start(args, message);
    if (va_arg(args, void*) == nullptr) {
        fwprintf(stderr, L"%s\n", message);
    } else {
        va_start(args, message);
        va_list args_copy;
        va_copy(args_copy, args);
        size_t len = vswprintf(nullptr, 0, message, args_copy) + 1;  // +1 for the null terminator
        va_end(args_copy);
        if (len > 0) {
            wchar_t* buffer = new wchar_t[len];
            vswprintf(buffer, len, message, args);
            fwprintf(stderr, L"%s\n", buffer);
            delete[] buffer;
        }
    }
    va_end(args);
}

void debug_output(const char* message, ...)
{
    if (gdebug)
    {
        va_list args;
        va_start(args, message);
        if (va_arg(args, void*) == nullptr) {
            print("%s\n", message);
        } else {
            va_start(args, message);
            va_list args_copy;
            va_copy(args_copy, args);
            size_t len = vsnprintf(nullptr, 0, message, args_copy) + 1;  // +1 for the null terminator
            va_end(args_copy);
            if (len > 0) {
                char* buffer = new char[len];
                vsnprintf(buffer, len, message, args);
                print("%s\n", buffer);
                delete[] buffer;
            }
        }
        va_end(args);
    }
}

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
#include "platform.h"
#include "benchmark.h"

#include "rife.h"

#include "filesystem_utils.h"
#include "progress.h"

static void print_usage()
{
    print("Usage: rife-ncnn-vulkan -0 infile -1 infile1 -o outfile [options]...\n");
    print("       rife-ncnn-vulkan -i indir -o outdir [options]...\n\n");
    print("  -h                   show this help\n");
    print("  -v                   verbose output\n");
    print("  -0 input0-path       input image0 path (jpg/png/webp)\n");
    print("  -1 input1-path       input image1 path (jpg/png/webp)\n");
    print("  -i input-path        input image directory (jpg/png/webp)\n");
    print("  -o output-path       output image path (jpg/png/webp) or directory\n");
    print("  -n num-frame         target frame count (default=N*2)\n");
    print("  -s time-step         time step (0~1, default=0.5)\n");
    print("  -m model-path        rife model path (default=rife-v2.3)\n");
    print("  -g gpu-id            gpu device to use (-1=cpu, default=auto) can be 0,1,2 for multi-gpu\n");
    print("  -j load:proc:save    thread count for load/proc/save (default=1:2:2)\n");
    print("                       (when '-r' specified, save is forced to 1) can be 1:2,2,2:2 for multi-gpu\n");
    print("  -r                   raw output to stdout (no jpg/png/webp output)\n");
    print("  -x                   enable spatial tta mode\n");
    print("  -z                   enable temporal tta mode\n");
    print("  -u                   enable UHD mode\n");
    print("  -f pattern-format    output image filename pattern format (%%08d.jpg/png/webp, default=ext/%%08d.png)\n");
    print("  -p progress          show progress (0=off, 1=on, default=1)\n");
    print("  -t progress-interval progress update interval in seconds (default=0.5)\n");
    print("  -d                   enable debug output\n");
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

void call_stop_callbacks()
{
    if (!g_stop_callbacks.empty())
    {
        for (auto callback : g_stop_callbacks)
        {
            callback();
        }
    }
}

void stop_with_error(const char* message,...)
{
    va_list args;
    va_start(args, message);
    if (va_arg(args, void*) == nullptr) {
        print("%s\n", message);
    } else {
        va_start(args, message);
        va_list args_copy;
        va_copy(args_copy, args);
        size_t len = vsnprintf(nullptr, 0, message, args_copy) + 1;  // +1 for the null terminator
        va_end(args_copy);
        if (len > 0) {
            char* buffer = new char[len];
            vsnprintf(buffer, len, message, args);
            print("%s\n", buffer);
            delete[] buffer;
        }
    }
    va_end(args);

    g_stopped.store(true, std::memory_order_release);
    call_stop_callbacks();
}

struct CacheEntry {
    ncnn::Mat image;
    int request;
    std::atomic<int> complete;
    int released;
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

    void register_request_nolock(const path_t& path, int count, int webp)
    {
        if (cache.find(path) == cache.end())
        {
            cache[path].request = count;
            cache[path].complete = 0;
            cache[path].webp = webp;
        }
        else
        {
            cache[path].request += count;
        }
    }

    void register_request(const path_t& path, int count, int webp)
    {
        pool_lock.lock();
        cache[path].request = count;
        cache[path].complete = 0;
        cache[path].webp = webp;
        pool_lock.unlock();
    }

    int acquire(const path_t& path, ncnn::Mat& image)
    {
        pool_lock.lock();
        auto& entry = cache[path];

        if (entry.released)
        {
            stop_with_error("Image pool: image %s acquired but it has been released", path.c_str());
            pool_lock.unlock();
            return entry.ret;
        }

        if (entry.image.empty())
        {
            int ret = decode_image(path, entry.image, &entry.webp);
            entry.ret = ret;
        }
        
        image = entry.image;
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
            entry.released = 1;
            if (!entry.image.empty()) // release if not empty
            {                         // empty if errors or use exists output
                entry.image.release();
            }
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
                item.second.image.release();
            }
        }
    }
private:
    std::map<path_t, CacheEntry> cache;
    ncnn::Mutex pool_lock;
};

ImageCachePool imagepool;

class Task
{
public:
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
        max_size = size;
    }

    void put(const T& v)
    {
        lock.lock();

        while (tasks.size() >= 8 && max_size != 0)
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
        if (max_size == 0) return false;
        lock.lock();
        bool ret = tasks.size() >= max_size;
        lock.unlock();
        return ret;
    }

private:
    int max_size;
    ncnn::Mutex lock;
    ncnn::ConditionVariable condition;
    std::queue<T> tasks;
};

TaskQueue<int> toload;
TaskQueue<Task> toproc;
TaskQueue<Task> tosave;
TaskQueue<Task> torawsave;

Progress progress;

// return 0: null, 1: interrupted, 2: stopped
int check_event()
{
    if (g_interrupted.load(std::memory_order_relaxed))
        return 1;
    if (g_stopped.load(std::memory_order_relaxed))
        return 2;
    return 0;
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
    debug_output("load thread started");

    const LoadThreadParams* ltp = (const LoadThreadParams*)args;

    for (;;)
    {
        {
            int r = check_event();
            if (r == 1)      goto load_interrupted;
            else if (r == 2) goto load_stopped;
        }

        if (toload.empty()) break;

        int i;
        toload.get(i);
        {
            int r = check_event();
            if (r == 1)      goto load_interrupted;
            else if (r == 2) goto load_stopped;
        }

        if (ltp->raw_output && ltp->bitmap[i])
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
    
            int ret0 = imagepool.acquire(v.in0path, v.in0image);
            int ret1 = imagepool.acquire(v.in1path, v.in1image);
            {
                int r = check_event();
                if (r == 1)      goto load_interrupted;
                else if (r == 2) goto load_stopped;
            }
    
            if (ret0 == 0 && ret1 == 0)
            {
                v.outimage = ncnn::Mat(v.in0image.w, v.in0image.h, (size_t)3, 3);
                toproc.put(v);
            }
            else
            {
                // 失败了也需要标记
                imagepool.release(image0path);
                imagepool.release(image1path);
            }
        }
        progress.update(1, 0, 0);
    }

    debug_output("load thread finished");
    return 0;
load_interrupted:
    debug_output("load thread interrupted");
    return 0;
load_stopped:
    debug_output("load thread stopped");
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
    debug_output("proc thread started");

    const ProcThreadParams* ptp = (const ProcThreadParams*)args;
    const RIFE* rife = ptp->rife;

    for (;;)
    {
        {
            int r = check_event();
            if (r == 1)      goto proc_interrupted;
            else if (r == 2) goto proc_stopped;
        }

        Task v;

        toproc.get(v);
        {
            int r = check_event();
            if (r == 1)      goto proc_interrupted;
            else if (r == 2) goto proc_stopped;
        }

        if (v.id == -233)
            break;

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
    }

    debug_output("proc thread finished");
    return 0;
proc_interrupted:
    debug_output("proc thread interrupted");
    return 0;
proc_stopped:
    debug_output("proc thread stopped");
    return 0;
}

class SaveThreadParams
{
public:
    int verbose;
};

void* save(void* args)
{
    debug_output("save thread started");

    const SaveThreadParams* stp = (const SaveThreadParams*)args;
    const int verbose = stp->verbose;

    for (;;)
    {
        Task v;

        tosave.get(v);

        if (v.id == -233)
            break;

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
        int r = check_event();
        if (r == 1)      goto save_interrupted;
        else if (r == 2) goto save_stopped;
    }

    debug_output("save thread finished");
    return 0;
save_interrupted:
    debug_output("save thread interrupted");
    return 0;
save_stopped:
    debug_output("save thread stopped");
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
            return;

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
    debug_output("raw save thread started");

    const RawSaveThreadParams* rstp = (const RawSaveThreadParams*)args;
    const int start_id = rstp->start_id;
    const int total = rstp->total;
    const int verbose = rstp->verbose;
    int last_id = start_id - 1;
    std::vector<int> pending_ids;
    std::vector<ncnn::Mat> pending_images;
    pending_images.resize(total + start_id);
    
    for (;;)
    {
        Task v;

        torawsave.get(v);

        if (v.id == -233)
            break;

        pending_images[v.id] = v.outimage;
        pending_ids.push_back(v.id);
        if (v.id == last_id + 1)
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
                pending_images[_id].release();
                i--;
                if (verbose)
                {
                    print("raw saved to stdout: %d\n", _id);
                }
                int r = check_event();
                if (r == 1)      goto raw_save_interrupted;
                else if (r == 2) goto raw_save_stopped;
            }
        }
        
        imagepool.release(v.in0path);
        imagepool.release(v.in1path);
        progress.update(0, 0, 1);
    }
    debug_output("raw save thread finished");
raw_save_interrupted:
    debug_output("raw save thread interrupted");
    return 0;
raw_save_stopped:
    debug_output("raw save thread stopped");
    return 0;
}

int get_num(const path_t& path)
{
    std::wregex num_regex(L"\\d+");
    std::wsmatch match;
    if (std::regex_search(path, match, num_regex)) {
        return std::stoi(match.str());
    }
    return 0;
}

void existence_bitmap(const std::vector<path_t>& files, std::vector<bool>& bitmap) {
    if (files.empty()) return;
    for (int i = 0; i < files.size(); i++) {
        int num = get_num(files[i]);
        int offset = num - 1;
        if (offset < 0 || offset >= bitmap.size()) {
            #if _WIN32
            wprint(L"file %ls has no corresponding output file\n", files[i].c_str());
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
        // show cursor
        if (progress.enabled)
            print("\033[25h");
        if (g_interrupted.load(std::memory_order_relaxed)) 
        {
            // second interrupt, terminate immediately.
            print("terminate immediately\n");
            exit(1);
        }
        else
        {
            g_interrupted.store(true, std::memory_order_release);
            print("\ninterrupt signal received, cancel all tasks...\n");
            call_stop_callbacks();
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
            numframe = atoi(optarg);
            break;
        case 's':
            timestep = atof(optarg);
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
            enable_progress = _wtoi(optarg);
            break;
        case 't':
            progress_interval = _wtof(optarg);
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
            debug = _wtoi(optarg);
            break;
        case 'h':
        default:
            print_usage();
            return -1;
        }
    }
#endif // _WIN32

    gdebug = debug;

    debug_output("rife-ncnn-vulkan start");

    if (((input0path.empty() || input1path.empty()) && inputpath.empty()) || (!raw_output && outputpath.empty()))
    {
        print_usage();
        return -1;
    }

    if (inputpath.empty() && (timestep <= 0.f || timestep >= 1.f))
    {
        print("invalid timestep argument, must be 0~1\n");
        return -1;
    }

    if (!inputpath.empty() && numframe < 0)
    {
        print("invalid numframe argument, must not be negative\n");
        return -1;
    }

    if (jobs_load < 1 || jobs_save < 1)
    {
        print("invalid thread count argument\n");
        return -1;
    }

    if (jobs_proc.size() != (gpuid.empty() ? 1 : gpuid.size()) && !jobs_proc.empty())
    {
        print("invalid jobs_proc thread count argument\n");
        return -1;
    }

    for (int i=0; i<(int)jobs_proc.size(); i++)
    {
        if (jobs_proc[i] < 1)
        {
            print("invalid jobs_proc thread count argument\n");
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
            print("invalid outputpath extension type\n");
            return -1;
        }
    }

    if (!raw_output && (format != PATHSTR("png") && format != PATHSTR("webp") && format != PATHSTR("jpg")))
    {
        print("invalid format argument\n");
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
        print("unknown model dir type\n");
        return -1;
    }

    if (!rife_v4 && (numframe != 0 || timestep != 0.5))
    {
        print("only rife-v4 model support custom numframe and timestep\n");
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
            int lr_o = list_directory(outputpath, exists_output_files);
            if (lr_o != 0 && !raw_output)
                return -1;
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

//                 print("%d %f %d\n", i, fx, sx);

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
                imagepool.register_request_nolock(input0_files[i], 1, get_file_extension(filename0) == PATHSTR("webp") ? 1 : 0);
                imagepool.register_request_nolock(input1_files[i], 1, get_file_extension(filename1) == PATHSTR("webp") ? 1 : 0);
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
            imagepool.register_request_nolock(input0path, 1, get_file_extension(input0path) == PATHSTR("webp") ? 1 : 0);
            imagepool.register_request_nolock(input1path, 1, get_file_extension(input1path) == PATHSTR("webp") ? 1 : 0);
            first_task_id = 0;
            total++;
        }
        else
        {
            print("input0path, input1path and outputpath must be file at the same time\n");
            print("inputpath and outputpath must be directory at the same time\n");
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
        print("\033[?25l");
    }
    progress.interval = progress_interval;
    progress.set_total(total);
    if (skipped > 0) 
    {
        progress.update(skipped, skipped, skipped, true);
        print("skipped %d frames\n", skipped);
    }

    path_t modeldir = sanitize_dirpath(model);

#if _WIN32
    CoInitializeEx(NULL, COINIT_MULTITHREADED);
#endif

    if (g_interrupted.load(std::memory_order_relaxed)) 
        return -1;

    #ifdef _WIN32 // windows use _setmode to set binary mode for stdout
    if (raw_output)
    {
        #include <fcntl.h>
        _setmode(_fileno(stdout), _O_BINARY);
    }
    #endif

    g_stop_callbacks.push_back([]() { imagepool.cleanup(); });

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
    if (raw_output)
        jobs_save = 1;

    int gpu_count = ncnn::get_gpu_count();
    for (int i=0; i<use_gpu_count; i++)
    {
        if (gpuid[i] < -1 || gpuid[i] >= gpu_count)
        {
            print("invalid gpu device\n");

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

    {
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

            debug_output("create load thread");
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

            g_stop_callbacks.push_back([
                    end, jobs_save, total_jobs_proc,
                    jobs_load, raw_output
                ](){
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

            bool interrupted = g_interrupted.load(std::memory_order_seq_cst);

            if (!interrupted)
            {
                for (int i=0; i<total_jobs_proc; i++)
                {
                    toproc.put(end);
                }
            }
            for (int i=0; i<total_jobs_proc; i++)
            {
                proc_threads[i]->join();
                delete proc_threads[i];
            }
            debug_output("proc thread joined\n");

            if (!interrupted)
            {
                for (int i=0; i<jobs_save; i++)
                {
                    if (raw_output)
                    {
                        torawsave.put(end);
                    }
                    else
                    {
                        tosave.put(end);
                    }
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
    }

    ncnn::destroy_gpu_instance();

    // show cursor
    if (progress.enabled)
        print("\033[?25h");
    return 0;
}
