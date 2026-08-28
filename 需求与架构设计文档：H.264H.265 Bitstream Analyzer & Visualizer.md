# 需求与架构设计文档：H.264/H.265 Bitstream Analyzer & Visualizer

## 1. 项目概述
本项目旨在开发一款跨平台的流媒体深度分析软件。不仅需要像 H264BSAnalyzer 那样对 H.264/H.265 裸流进行精准的 NALU 语法与协议级解析，还需要集成类似 YUView 的画面播放验证功能。实现底层字节（Hex）、句法元素（Syntax）与宏观画面（Video Frame）的同屏联动。

## 2. 核心架构约束（最高优先级规则）
本项目必须严格遵循**三层强解耦 (Three-Tier Decoupling)** 架构：

*   **Layer 1: 解析引擎层 (Parser Core)** 
    *   **职责**：负责文件 I/O、NALU 搜索、起始码匹配、字节偏移量记录、SPS/PPS 结构按位解析。
    *   **技术要求**：**必须完全使用标准 C++ (Zero-Dependency)**。绝对禁止引入任何第三方音视频库（如 FFmpeg）或 Qt 基础类。保留对最底层 Byte Offset 的绝对控制权。
*   **Layer 2: 播放与解码引擎 (Player Engine)**
    *   **职责**：专门负责将视频流解码为可视化的像素帧 (YUV/RGB)。
    *   **技术要求**：允许且建议封装 **FFmpeg (libavcodec/libavformat)** 或使用 **Qt Multimedia** 模块。该引擎只接受 UI 层传来的文件路径或数据流进行解码播放，绝对不能污染 Layer 1 的解析逻辑。
*   **Layer 3: 展现与交互层 (UI Layer)**
    *   **职责**：界面布局，以及统筹 Layer 1 和 Layer 2 的数据联动。
    *   **技术要求**：使用 Qt6 构建。

## 3. 核心功能需求

### 3.1 协议层解析视图 (类似 H264BSAnalyzer)
*   **NALU 列表**：展示 No., Offset, Length, Start Code, NAL Type, Info。不同类型帧（I/P/B）高亮区分。
*   **Hex 视图**：选中列表某行时，同步显示该 NALU 对应数据段的 16 进制及 ASCII 码。
*   **Info 树状视图**：展示 SPS/PPS 等结构的详细句法元素（如分辨率、Profile 等）。

### 3.2 视觉播放视图 (类似 YUView)
*   **视频播放器**：在界面侧边或顶部集成一个视频播放窗口，支持基础的播放、暂停、逐帧快进/快退。
*   **联动高亮 (核心创新点)**：当在“视频播放器”中播放到某一帧时，“NALU 列表”应自动滚动并高亮当前画面对应的 NALU；反之，当在“NALU 列表”中点击某一个 I 帧或 P 帧时，播放器应精确 Seek 并渲染该帧的画面。

## 4. 技术栈要求
*   **构建系统**：CMake (需严格划分模块，通过 `target_link_libraries` 组合)。
*   **核心语言**：C++17 或更高标准。
*   **UI 与渲染**：Qt6 (Widgets 或配合 QOpenGLWidget 进行画面渲染)。
*   **解码库**：FFmpeg (6.x 版本优先) 或 Qt6 Multimedia 模块。

## 5. 纯 C++ 核心解析实现策略 (Parser Core)
*   **NALU 提取器**：手写状态机扫描 `00 00 00 01`。
*   **BitReader**：实现按位读取机制。
*   **哥伦布编码**：实现 `ue(v)` 和 `se(v)` 的指数哥伦布解码算法。

## 6. 开源项目工程化规范与目录生成指令
请在开始编写逻辑代码前，**首先为我生成完整的项目基础目录结构和 CMake 构建脚本**。

### 6.1 标准目录结构要求
```text
BitstreamVisualizer/
├── CMakeLists.txt              # 全局构建配置
├── README.md
├── LICENSE                     # MIT License
├── .gitignore
├── src/
│   ├── CMakeLists.txt          
│   ├── core_parser/            # Layer 1: 纯 C++ 解析引擎 (静态库)
│   │   ├── CMakeLists.txt      
│   │   ├── include/            
│   │   │   ├── BitReader.h
│   │   │   └── NaluExtractor.h
│   │   └── src/                
│   ├── core_player/            # Layer 2: 解码播放引擎 (静态库/动态库)
│   │   ├── CMakeLists.txt      # 需配置查找并链接 FFmpeg 或 Qt6 Multimedia
│   │   ├── include/            
│   │   │   └── VideoDecoder.h
│   │   └── src/                
│   └── ui/                     # Layer 3: Qt6 界面模块 (可执行文件)
│       ├── CMakeLists.txt      # 链接 core_parser 和 core_player
│       ├── main.cpp            
│       ├── MainWindow.h
│       └── MainWindow.cpp
└── tests/                      
    └── CMakeLists.txt
```

## 7. 开源项目工程化规范与目录生成指令

本项目计划作为标准的开源项目发布。请在开始编写具体的 C++ 逻辑代码之前，**首先为我生成完整的项目基础目录结构和构建脚本**。

