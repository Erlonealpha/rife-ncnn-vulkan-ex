# RIFE ncnn Vulkan EX
[![Ask DeepWiki](https://deepwiki.com/badge.svg)](https://deepwiki.com/Erlonealpha/rife-ncnn-vulkan-ex) [![zread](https://img.shields.io/badge/Ask_Zread-_.svg?style=flat&color=00b0aa&labelColor=000000&logo=data%3Aimage%2Fsvg%2Bxml%3Bbase64%2CPHN2ZyB3aWR0aD0iMTYiIGhlaWdodD0iMTYiIHZpZXdCb3g9IjAgMCAxNiAxNiIgZmlsbD0ibm9uZSIgeG1sbnM9Imh0dHA6Ly93d3cudzMub3JnLzIwMDAvc3ZnIj4KPHBhdGggZD0iTTQuOTYxNTYgMS42MDAxSDIuMjQxNTZDMS44ODgxIDEuNjAwMSAxLjYwMTU2IDEuODg2NjQgMS42MDE1NiAyLjI0MDFWNC45NjAxQzEuNjAxNTYgNS4zMTM1NiAxLjg4ODEgNS42MDAxIDIuMjQxNTYgNS42MDAxSDQuOTYxNTZDNS4zMTUwMiA1LjYwMDEgNS42MDE1NiA1LjMxMzU2IDUuNjAxNTYgNC45NjAxVjIuMjQwMUM1LjYwMTU2IDEuODg2NjQgNS4zMTUwMiAxLjYwMDEgNC45NjE1NiAxLjYwMDFaIiBmaWxsPSIjZmZmIi8%2BCjxwYXRoIGQ9Ik00Ljk2MTU2IDEwLjM5OTlIMi4yNDE1NkMxLjg4ODEgMTAuMzk5OSAxLjYwMTU2IDEwLjY4NjQgMS42MDE1NiAxMS4wMzk5VjEzLjc1OTlDMS42MDE1NiAxNC4xMTM0IDEuODg4MSAxNC4zOTk5IDIuMjQxNTYgMTQuMzk5OUg0Ljk2MTU2QzUuMzE1MDIgMTQuMzk5OSA1LjYwMTU2IDE0LjExMzQgNS42MDE1NiAxMy43NTk5VjExLjAzOTlDNS42MDE1NiAxMC42ODY0IDUuMzE1MDIgMTAuMzk5OSA0Ljk2MTU2IDEwLjM5OTlaIiBmaWxsPSIjZmZmIi8%2BCjxwYXRoIGQ9Ik0xMy43NTg0IDEuNjAwMUgxMS4wMzg0QzEwLjY4NSAxLjYwMDEgMTAuMzk4NCAxLjg4NjY0IDEwLjM5ODQgMi4yNDAxVjQuOTYwMUMxMC4zOTg0IDUuMzEzNTYgMTAuNjg1IDUuNjAwMSAxMS4wMzg0IDUuNjAwMUgxMy43NTg0QzE0LjExMTkgNS42MDAxIDE0LjM5ODQgNS4zMTM1NiAxNC4zOTg0IDQuOTYwMVYyLjI0MDFDMTQuMzk4NCAxLjg4NjY0IDE0LjExMTkgMS42MDAxIDEzLjc1ODQgMS42MDAxWiIgZmlsbD0iI2ZmZiIvPgo8cGF0aCBkPSJNNCAxMkwxMiA0TDQgMTJaIiBmaWxsPSIjZmZmIi8%2BCjxwYXRoIGQ9Ik00IDEyTDEyIDQiIHN0cm9rZT0iI2ZmZiIgc3Ryb2tlLXdpZHRoPSIxLjUiIHN0cm9rZS1saW5lY2FwPSJyb3VuZCIvPgo8L3N2Zz4K&logoColor=ffffff)](https://zread.ai/Erlonealpha/rife-ncnn-vulkan-ex)

简体中文 | [English](README_EN.md)

这是基于[RIFE ncnn Vulkan](https://github.com/nihui/rife-ncnn-vulkan)的扩展版本，支持了流式输出、进度显示等功能。

ncnn 实现的 RIFE（用于视频帧插值的实时中间流估计）。

rife-ncnn-vulkan-ex 使用 [ncnn project](https://github.com/Tencent/ncnn) 作为通用神经网络推理框架。

## 扩展功能
- 实时进度显示
- 支持流式输出
- 支持暂停、恢复、安全退出
- 输入图像缓存
- 自动断点续传

## [下载](https://github.com/Erlonealpha/rife-ncnn-vulkan-ex/releases)

下载 Windows/Linux/MacOS 多平台预编译包，支持 Intel/AMD/Nvidia 显卡

**https://github.com/Erlonealpha/rife-ncnn-vulkan-ex/releases**

发行包包含所有必需的二进制文件和模型。它是可移植的，因此不需要 CUDA 或 PyTorch 运行时环境。:)

## 关于 RIFE

RIFE（用于视频帧插值的实时中间流估计）

https://github.com/hzwer/arXiv2020-RIFE

Huang, Zhewei and Zhang, Tianyuan and Heng, Wen and Shi, Boxin and Zhou, Shuchang

https://rife-vfi.github.io

https://arxiv.org/abs/2011.06294

## 基础用法

输入两个帧图像，输出一个插值帧图像。

#### 示例命令

```shell
./rife-ncnn-vulkan-ex -0 0.jpg -1 1.jpg -o 01.jpg
./rife-ncnn-vulkan-ex -i input_frames/ -o output_frames/
```

以下命令在 CPU、离散 GPU 和集成 GPU 上同时运行。使用 2 个线程进行图像解码，4 个线程用于一个 CPU 工作线程，4 个线程用于另一个 CPU 工作线程，2 个线程用于离散 GPU，1 个线程用于集成 GPU，4 个线程用于图像编码。
```shell
./rife-ncnn-vulkan-ex -i input_frames/ -o output_frames/ -g -1,-1,0,1 -j 2:4,4,2,1:4
```

### 与FFmpeg结合使用

```shell
mkdir input_frames
mkdir output_frames

# 使用 ffpmeg 输出输入视频的基本格式（例如：24帧，AAC音频）
ffprobe input.mp4

# 解码输入视频为无损 JPEG 图像序列帧
ffmpeg -i input.mp4 -f image2 -qscale:v 1 input_frames/frame_%08d.jpg

# 以下两个步骤可以使用流式输出模式，无需中间帧存储（见下文）
# 插针输入图像序列帧，输出插值帧序列帧，默认输出两倍
./rife-ncnn-vulkan-ex -i input_frames -o output_frames

# 编码输出插值帧序列帧为 H.265/HEVC 视频文件（48帧）
ffmpeg -framerate 48 -i output_frames/%08d.jpg -i input.mp4 -c:v hevc_nvenc -rc constqp -qp 22 -tune hq -pix_fmt yuv420p -c:a copy -map 0:v -map 1:a output.mp4
```

### 流式输出到 FFmpeg (实时视频处理)

支持流式输出，无需中间帧存储。使用`-r`选项：

```shell
# 使用原始输出模式 (输出图像为 BGR24 rawvideo 格式)
# 注意: 替换 1920x1080 为你的输入视频实际的分辨率（这是必须的！）
./rife-ncnn-vulkan-ex -i input_frames -r | ffmpeg -framerate 48 -f rawvideo -pix_fmt bgr24 -s 1920x1080 -i - -i input.mp4 -c:v hevc_nvenc -rc constqp -qp 22 -tune hq -pix_fmt yuv420p -c:a copy -map 0:v -map 1:a output.mp4
```

### 完整用法

```console
Usage: rife-ncnn-vulkan -0 infile -1 infile1 -o outfile [options]...
       rife-ncnn-vulkan -i indir -o outdir [options]...

  -h                   显示此帮助信息
  -v                   显示详细信息
  -0 input0-path       输入图像0路径 (jpg/png/webp)
  -1 input1-path       输入图像1路径 (jpg/png/webp)
  -i input-path        输入图像目录  (jpg/png/webp)
  -o output-path       输出图像路径  (jpg/png/webp) 或文件夹
  -n num-frame         目标帧数 (默认=N*2)
  -s time-step         插值时间步长 (0~1, 默认=0.5)
  -m model-path        模型路径(默认=rife-v2.3)
  -g gpu-id            使用的显卡 ID (-1=cpu, 默认=auto) 支持 0,1,2 用于多显卡
  -j load:proc:save    线程数设置 加载/处理/保存 (默认=1:2:2)
                       (当 '-r' 启用时, 保存线程数被强制设置为 1) 支持 1:2,2,2:2 用于多显卡
  -r                   原始输出模式 (无 jpg/png/webp 输出, 支持如ffmpeg管道输入)
  -x                   启用 TTA 模式 (空间和时间测试时增强选项)
  -z                   启用时序 TTA 模式
  -u                   启用 UHD 模式 （超高清模式）
  -f pattern-format    输出图像文件名模式 (%08d.jpg/png/webp, 默认=ext/%08d.png)
  -p progress          显示进度信息 (0=不显示, 1=启用, 默认=1)
  -t progress-interval 进度信息刷新间隔 (默认=0.5，单位=秒)
  -d                   启用 debug 模式 (输出更多信息)
```

- `input0-path`、 `input1-path`和 `output-path` 支持文件路径
- `input-path` 和 `output-path` 支持文件夹路径
- `num-frame` = 目标帧数，仅rife-v4及以上模型支持
- `time-step` = 插值时间步长，仅rife-v4及以上模型支持
- `load:proc:save` = 线程数设置 加载/处理/保存。较大的值增加了GPU内存使用。推荐：“4:4:4”用于许多小图像，“2:2:2”用于大图像。注意：当使用原始输出模式时，保存线程数被强制设置为 1。
- `raw-output` = 流式处理结果到标准输出的BGR24格式，无需写入图像文件。这使得实时流处理工具FFmpeg可以通过管道操作符“|”进行流式传输。
- `pattern-format` = 输出文件名模式和格式（png/webp/jpg）。PNG支持更好，WebP文件大小更小，两者均为无损编码。

### 性能要点

- **线程调优**: `-j load:proc:save` 参数控制资源分配。如果您的GPU有余量，请使用更高的值；如果内存有限，请使用较低的值。
- **图像缓存**: 输入图像被缓存以避免冗余加载，尤其适用于多帧插值（例如，8x超分）。
- **原始输出模式**: 使用 `-r` 进行流式处理避免中间文件I/O，从而实现更快的实时处理，并通过管道操作符“|”进行流式传输。

### 问题排查

如果出现问题或崩溃, 尝试升级你的显卡驱动:

- Intel: https://downloadcenter.intel.com/product/80939/Graphics-Drivers
- AMD: https://www.amd.com/en/support
- NVIDIA: https://www.nvidia.com/Download/index.aspx

## 从源文件构建

1. 下载并安装 [Vulkan SDK](https://vulkan.lunarg.com/)
  - 对于 Linux 发行版，你可以从包管理器获取必要的构建需求
```shell
dnf install vulkan-headers vulkan-loader-devel
```
```shell
apt-get install libvulkan-dev
```
```shell
pacman -S vulkan-headers vulkan-icd-loader
```

2. 克隆项目和所有子模块

```shell
git clone https://github.com/nihui/rife-ncnn-vulkan.git
cd rife-ncnn-vulkan
git submodule update --init --recursive
```

3. 使用Cmake构建项目
  - 你可以通过 -DUSE_STATIC_MOLTENVK=ON 选项避免在 MacOS 上链接 Vulkan 加载库

```shell
mkdir build
cd build
cmake ../src
cmake --build . -j 4
```

### 模型

| 模型 | 版本 |
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
| rife-v4.25-lite | 4.25-lite |
| rife-v4.26 | 4.26 |
| rife-v4.26-large | 4.26-large |

## 示例图像

### 原始图像

![origin0](images/0.png)
![origin1](images/1.png)

### 使用模型 rife-anime 进行插值

```shell
rife-ncnn-vulkan-ex.exe -m models/rife-anime -0 0.png -1 1.png -o out.png
```

![rife](images/out.png)

### 使用模型 rife-anime 进行插值 + TTA-s

```shell
rife-ncnn-vulkan-ex.exe -m models/rife-anime -x -0 0.png -1 1.png -o out.png
```

![rife](images/outx.png)

## 源项目

- https://github.com/hzwer/arXiv2020-RIFE
- https://github.com/nihui/rife-ncnn-vulkan

## 其他参考项目
- https://github.com/Tencent/ncnn 适用于所有平台的快速神经网络推理库
- https://github.com/webmproject/libwebp 用于在所有平台上对 Webp 图像进行编码和解码
- https://github.com/nothings/stb 用于在 Linux/MacOS 上解码和编码图像
- https://github.com/tronkko/dirent 在 Windows 上列出目录中的文件
- https://github.com/styler00dollar/VapourSynth-RIFE-ncnn-Vulkan 更多模型
- https://github.com/TNTwise/rife-ncnn-vulkan 工作流参考