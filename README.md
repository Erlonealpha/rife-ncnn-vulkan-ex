# RIFE ncnn Vulkan EX

This is an extended version of [RIFE ncnn Vulkan](https://github.com/nihui/rife-ncnn-vulkan) with additional options and features.

ncnn implementation of RIFE (Real-Time Intermediate Flow Estimation for Video Frame Interpolation).

rife-ncnn-vulkan uses [ncnn project](https://github.com/Tencent/ncnn) as the universal neural network inference framework.

## Additional Features
- Simple progress display
- Stream output to stdout support
- Graceful shutdown on interrupt signal
- Input image caching for improved performance
- Breakpoint detection with automatic resumption from checkpoint

## [Download](https://github.com/Erlonealpha/rife-ncnn-vulkan-ex/releases)

Download Windows/Linux/MacOS Executable for Intel/AMD/Nvidia GPU

**https://github.com/Erlonealpha/rife-ncnn-vulkan-ex/releases**

This package includes all the binaries and models required. It is portable, so no CUDA or PyTorch runtime environment is needed :)

## About RIFE

RIFE (Real-Time Intermediate Flow Estimation for Video Frame Interpolation)

https://github.com/hzwer/arXiv2020-RIFE

Huang, Zhewei and Zhang, Tianyuan and Heng, Wen and Shi, Boxin and Zhou, Shuchang

https://rife-vfi.github.io

https://arxiv.org/abs/2011.06294

## Usages

Input two frame images, output one interpolated frame image.

### Example Commands

```shell
./rife-ncnn-vulkan -0 0.jpg -1 1.jpg -o 01.jpg
./rife-ncnn-vulkan -i input_frames/ -o output_frames/
```

Example below runs on CPU, Discrete GPU, and Integrated GPU all at the same time. Uses 2 threads for image decoding, 4 threads for one CPU worker, 4 threads for another CPU worker, 2 threads for discrete GPU, 1 thread for integrated GPU, and 4 threads for image encoding.
```shell
./rife-ncnn-vulkan -i input_frames/ -o output_frames/ -g -1,-1,0,1 -j 2:4,4,2,1:4
```

### Video Interpolation with FFmpeg

```shell
mkdir input_frames
mkdir output_frames

# Find the source fps and format with ffprobe (e.g., 24fps, AAC)
ffprobe input.mp4

# Decode all frames
ffmpeg -i input.mp4 input_frames/frame_%08d.png

# Interpolate to 2x frame count
./rife-ncnn-vulkan -i input_frames -o output_frames

# Encode interpolated frames at 48fps with audio
ffmpeg -framerate 48 -i output_frames/%08d.png -i input.mp4 -c:v hevc_nvenc -rc constqp -qp 22 -tune hq -pix_fmt yuv420p -c:a aac -map 0:v -map 1:a output.mp4
```

### Streaming Output to FFmpeg (Real-time Processing)

For direct streaming output without intermediate frame storage, use the `-r` option:

```shell
# Method 1: Using raw output (output is BGR24 rawvideo format)
# Note: replace 1920x1080 with your actual frame resolution
./rife-ncnn-vulkan -i input_frames -r | ffmpeg -framerate 48 -f rawvideo -pix_fmt bgr24 -s 1920x1080 -i - -i input.mp4 -c:v hevc_nvenc -rc constqp -qp 22 -tune hq -pix_fmt yuv420p -c:a aac -map 0:v -map 1:a output.mp4
```

### Full Usages

```console
Usage: rife-ncnn-vulkan -0 infile -1 infile1 -o outfile [options]...
       rife-ncnn-vulkan -i indir -o outdir [options]...

  -h                   show this help
  -v                   verbose output
  -0 input0-path       input image0 path (jpg/png/webp)
  -1 input1-path       input image1 path (jpg/png/webp)
  -i input-path        input image directory (jpg/png/webp)
  -o output-path       output image path (jpg/png/webp) or directory
  -n num-frame         target frame count (default=N*2)
  -s time-step         time step (0~1, default=0.5)
  -m model-path        rife model path (default=rife-v2.3)
  -g gpu-id            gpu device to use (-1=cpu, default=auto) can be 0,1,2 for multi-gpu
  -j load:proc:save    thread count for load/proc/save (default=1:2:2)
                       (when '-r' specified, save is forced to 1) can be 1:2,2,2:2 for multi-gpu
  -r                   raw output to stdout (no jpg/png/webp output, supports streaming to ffmpeg via pipe)
  -x                   enable spatial tta mode
  -z                   enable temporal tta mode
  -u                   enable UHD mode
  -f pattern-format    output image filename pattern format (%08d.jpg/png/webp, default=ext/%08d.png)
  -p progress          show progress (0=off, 1=on, default=1)
  -t progress-interval progress update interval in seconds (default=0.5)
  -d                   enable debug output
```

- `input0-path`, `input1-path` and `output-path` accept file path
- `input-path` and `output-path` accept file directory
- `num-frame` = target frame count
- `time-step` = interpolation time
- `load:proc:save` = thread count for load/processing/save stages. Larger values increase GPU memory usage. Recommended: "4:4:4" for many small images, "2:2:2" for large images. Note: save threads are forced to 1 when using raw output (-r flag).
- `raw-output` = stream processing results to stdout in BGR24 format without writing image files. This enables real-time streaming to video processing tools like FFmpeg via pipe operator "|".
- `pattern-format` = output filename pattern and format (png/webp/jpg). PNG has better support, WebP yields smaller files, both are losslessly encoded.

### Performance Tips

- **Thread Tuning**: The `-j load:proc:save` parameter controls resource allocation. Use higher values if your GPU has spare capacity, lower values if memory is limited.
- **Image Caching**: Input images are cached to avoid redundant loading, especially beneficial for multi-frame interpolation (e.g., 8x upsampling).
- **Raw Output Mode**: Using `-r` for streaming avoids intermediate file I/O, enabling faster real-time processing when piping to FFmpeg.

### Troubleshooting

If you encounter a crash or error, try upgrading your GPU driver:

- Intel: https://downloadcenter.intel.com/product/80939/Graphics-Drivers
- AMD: https://www.amd.com/en/support
- NVIDIA: https://www.nvidia.com/Download/index.aspx

## Build from Source

1. Download and setup the Vulkan SDK from https://vulkan.lunarg.com/
  - For Linux distributions, you can either get the essential build requirements from package manager
```shell
dnf install vulkan-headers vulkan-loader-devel
```
```shell
apt-get install libvulkan-dev
```
```shell
pacman -S vulkan-headers vulkan-icd-loader
```

2. Clone this project with all submodules

```shell
git clone https://github.com/nihui/rife-ncnn-vulkan.git
cd rife-ncnn-vulkan
git submodule update --init --recursive
```

3. Build with CMake
  - You can pass -DUSE_STATIC_MOLTENVK=ON option to avoid linking the vulkan loader library on MacOS

```shell
mkdir build
cd build
cmake ../src
cmake --build . -j 4
```

### Model

| model | upstream version |
|---|---|
| rife | 1.2 |
| rife-HD | 1.5 |
| rife-UHD | 1.6 |
| rife-anime | 1.8 |
| rife-v2 | 2.0 |
| rife-v2.3 | 2.3 |
| rife-v2.4 | 2.4 |
| rife-v3.0 | 3.0 |
| rife-v3.1 | 3.1 |
| rife-v4 | 4.0 |
| rife-v4.1 | 4.1 |
| rife-v4.2 | 4.2 | 
| rife-v4.3 | 4.3 |
| rife-v4.4 | 4.4 |
| rife-v4.5 | 4.5 |
| rife-v4.6 | 4.6 |
| rife-v4.7 | 4.7 |
| rife-v4.8 | 4.8 |
| rife-v4.9 | 4.9 |
| rife-v4.10 | 4.10 |
| rife-v4.11 | 4.11 |
| rife-v4.12 | 4.12 |
| rife-v4.12-lite | 4.12-lite |
| rife-v4.13 | 4.13 |
| rife-v4.13-lite | 4.13-lite |
| rife-v4.14 | 4.14 |
| rife-v4.14-lite | 4.14-lite |
| rife-v4.15 | 4.15 |
| rife-v4.15-lite | 4.15-lite |
| rife-v4.16-lite | 4.16-lite |
| rife-v4.17 | 4.17 |
| rife-v4.17-lite | 4.17-lite |
| rife-v4.18 | 4.18 |
| rife-v4.19 | 4.19 |
| rife-v4.20 | 4.20 |
| rife-v4.21 | 4.21 |
| rife-v4.22 | 4.22 |
| rife-v4.22-lite | 4.22-lite |
| rife-v4.24 | 4.24 |
| rife-v4.25 | 4.25 |

## Sample Images

### Original Image

![origin0](images/0.png)
![origin1](images/1.png)

### Interpolate with rife rife-anime model

```shell
rife-ncnn-vulkan.exe -m models/rife-anime -0 0.png -1 1.png -o out.png
```

![rife](images/out.png)

### Interpolate with rife rife-anime model + TTA-s

```shell
rife-ncnn-vulkan.exe -m models/rife-anime -x -0 0.png -1 1.png -o out.png
```

![rife](images/outx.png)

## Original RIFE Project

- https://github.com/hzwer/arXiv2020-RIFE

## Other Open-Source Code Used

- https://github.com/Tencent/ncnn for fast neural network inference on ALL PLATFORMS
- https://github.com/webmproject/libwebp for encoding and decoding Webp images on ALL PLATFORMS
- https://github.com/nothings/stb for decoding and encoding image on Linux / MacOS
- https://github.com/tronkko/dirent for listing files in directory on Windows
- https://github.com/nihui/rife-ncnn-vulkan for the original implementation
- https://github.com/styler00dollar/VapourSynth-RIFE-ncnn-Vulkan for the added models
- https://github.com/TNTwise/rife-ncnn-vulkan for additional features