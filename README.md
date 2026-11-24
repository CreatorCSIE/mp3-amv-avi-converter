# amvconv — Actions ATJ 芯片 AMV 视频转换器（现代重写）

`amvconv` 是一个**面向 Actions ATJ21xx 系列芯片播放器生态（RUIZU 等品牌"带视频功能的 MP4/MP3"）的开源 AMV/AVI 转换器**——逆向老程序「媒体播放器管理工具 4.46」中的 AMVConverter 并重写其转换逻辑，让这类老设备能用现代素材。AMV 是该芯片平台使用的私有视频格式，官方工具早已停更、只跑在老 Windows 上。

> **免责声明**：本项目仅用于**互操作性研究与数据抢救**，不随附原程序（专有软件）的任何代码与资源。
> *Interoperability research and data rescue only; no code or assets from the proprietary original tool are redistributed.*

核心理念是**比特流复刻，而非编码器包装**——ffmpeg 自带的 `amv` 编码器量化表与设备真值不符（会产生灰底），所以视频走**自研纯 Python AMV 原生编码器**，逐字节复刻原版输出；ffmpeg 只负责源解码与容器/音频 donor。定案靠三层参照系交叉：

```text
静态逆向 exe/DLL                      固件 MMM_VP.AL（Unicorn 预言机）
├── AMV_EncDLL：量化表/Huffman/DCT/扫描序   ├── 视频解析器逐分支验收行为
├── ini 参数表：VIDEOSIZE / AVISIZE          └── amvh 私有头 / idx1 布局
            ↓                                     ↓
        RUIZU M7（ATJ2157）实机 A/B 标定——播放 + 快进全档验证通过
            ↓
   amvconv：ffmpeg 源解码 → 自研原生编码 → 原版比特流（设备直显）
```

> **戒律**：任何比特流结论必须有 DLL 逆向、固件预言机或实机 A/B 三层中至少两层的证据背书，写进 [docs/](docs/) 才进代码——"看起来能播"不算定案。

完整逆向与标定记录见 [docs/01](docs/01-reverse-engineering-notes.md)–[docs/06](docs/06-mmm-vp-disasm.md)。

---

## 🌟 核心功能特性

### 转换核心（已可用）

- **AMV 原生编码器**：量化表 / Huffman 表 / DCT / 扫描序 / 容器布局全部从 DLL 与实机标定逆向提取；无头 JPEG 帧（FFD8+熵数据+FFD9）、4:2:0、非 16 倍数尺寸按补齐 MCU 完整编码（填充=亮度 0/色度 128）；原版方向 quirk（vflip + BMP bottom-up）逐字节复刻，设备直显不翻转。
- **熵编码 C 核**：`_entropy.c` 经 ctypes 现场编译自动加速，缺 gcc 时无感回退纯 Python，输出比特流一致。
- **容器复刻**：`RIFF('AMV ')` + `hdrl` 内 56 字节私有头 `amvh` + `AMV_END_` 哨兵 + **`idx1` 索引**——快进/seek 是固件硬依赖，缺失设备直接报格式错误；已按原版布局写入并实机快进验证。
- **音频链路**：IMA-ADPCM 变体固定 22050 Hz 单声道，与视频帧 1:1 交织（块样本数 = 22050/fps）；帧率从 UI 参数表候选取最大、低帧率源复制帧补齐，22050 需被 fps 整除。
- **AVI 模式 writer**：原版头部 / AVI1 / 4B 对齐 / idx1 复刻，产出 PC 播放器友好的 AVI（PotPlayer 对 AMV 容器支持差，AVI 正常），CLI `--avi` 与 GUI 模式切换均已接入。
- **实机全档验证**：RUIZU M7 上 128×128@30 / 160×120@30 / 320×240@25 播放 + 快进全过，画质与原版持平（灰底消除 / 方向 / 拉伸）；ATJ2157 硬件解码上限 320×240。

### GUI（PySide6 + Fluent-Widgets，已可用）

- **转换页**：批量队列 + 拖拽添加 + 逐行进度条 + 点击取消 + AMV/AVI 双模式 + 分辨率/帧率联动（自动改帧率提示）；双击已完成行弹出结果预览。
- **预览页（侧边栏独立页）**：打开/拖入任意媒体文件逐帧预览——画面 = ffmpeg 解码输出 = 设备显示取向，**所见即所得**；播放/暂停、进度条拖动定位、ffplay 含声播放按钮（Qt 多媒体后端受原版 quirk 影响不可用，见 FAQ）。
- **关于页**：运行环境自检（Python/PySide6/ffmpeg/C 核加载状态）、设备兼容实机结论、第三方库许可证逐字实查列表、致谢与免责声明折叠分组。

### CLI（已可用）

- `amv-conv profiles` 打印逆向得到的 12 档参数表；`convert` 支持 `-s/-f` 与 `--avi [--q]`；`inspect` 输出 AMV 容器结构摘要（与原版输出字节级对比用）。

### 老素材兼容

- 源解码走 FFmpeg，mp4/mkv/mov/webm 等现代格式直接吃——不再依赖原版工具的系统 DirectShow 解码器（RM/VOB 等老格式在现代 Windows 上大概率失败，这正是重写改用 FFmpeg 解码源的原因）。

---

## 🧭 开发状态

| 阶段 | 内容 | 状态 |
|---|---|---|
| Phase 0 | 格式逆向（ini / DLL / MMM_VP.AL 解析器 / 量化表 / 灰阶探针） | ✅ 完成，证据链见 docs/01–06 |
| Phase 1 | AMV 原生编码器（纯 Python + C 核）+ 容器 idx1 尾区 | ✅ 完成，实机全档播放 + 快进验证通过 |
| Phase 2 | AVI 模式 writer（原版头部 / AVI1 / 4B 对齐 / idx1） | ✅ 完成，实机验证 + CLI `--avi` 接入 |
| Phase 3 | 画质对齐（灰底消除 / 方向 / 拉伸） | ✅ 实机与原版持平 |
| Phase 4 | GUI（批量队列 / 进度 / 取消 / 侧边栏预览页 / 关于页） | ✅ 完成，pytest 21 passed |
| Phase 5 | 发布流水线（GitHub Actions CI + Nuitka 单文件打包 + GitHub Release） | 🚧 进行中（首发 v0.1.0） |
| Phase 6 | 核心 C++ 化 + Qt 壳迁移路线 | 📋 调研完成（docs/06），暂不动工 |

---

## 💻 运行环境要求

### 1. 使用者

- **Windows x64**（实机标定平台；核心层跨平台，GUI 未在其他平台验证）。
- **FFmpeg**：`ffmpeg`/`ffplay` 需在 PATH 或随包目录（源解码、donor 容器、含声预览都靠它）。
- 源码运行时需 **Python ≥ 3.10**；GUI 依赖 PySide6 + PySide6-Fluent-Widgets——**注意 PySide6 6.12 wheel 在 Windows 上因系统 ICU 版本冲突无法加载，钉 6.9.x**。
- gcc 可选：有则熵编码 C 核现场编译加速，无则自动回退纯 Python（结果比特流一致，只是慢）。

### 2. 开发者

- venv + `pip install -e .[gui,dev]`；回归测试 `pytest`（编码器比特流 / AVI 布局 / 参数表 / 进度取消预览）。
- 实机验证（可选，改动比特流行为时强烈建议）：RUIZU M7 或同芯片设备 + 读卡器。

---

## 🚀 如何编译与运行

### 1. 快速开始

```bash
pip install -e .[gui,dev]          # 或仅 pip install -e .（只装核心+CLI）

amv-conv profiles                   # 打印逆向得到的 12 档参数表
amv-conv convert in.mp4 out.amv -s 160x120 -f 30
amv-conv convert in.mp4 out.avi -s 160x120 --avi [--q 4]   # AVI 模式（PC 播放器友好）
amv-conv inspect out.amv            # 容器结构摘要（与原版输出对比用）
amv-conv-gui                        # GUI：转换队列 + 侧边栏预览页 + 关于页
pytest                              # 回归测试
```

### 2. 仓库结构 (Repository Layout)

```text
mp3-amv-avi-converter
├── docs/                   逆向与标定记录 01–06（含 C++ 化路线调研）
├── src/amvconv/
│   ├── cli.py              命令行入口（amv-conv）
│   ├── core/               壳无关转换核心（纯 Python + 可选 C 核）
│   │   ├── amvenc.py       AMV 原生编码器（复刻原版比特流）
│   │   ├── _entropy.c      熵编码 C 核（ctypes 自动编译，缺则回退）
│   │   ├── aviwriter.py    AVI 模式 writer（原版布局复刻）
│   │   ├── ffmpeg.py       FFmpeg 子进程（源解码 / donor / 音频）
│   │   ├── profiles.py     原程序参数表（VIDEOSIZE / AVISIZE）
│   │   ├── amv.py          AMV 容器解析器（字节级对比工具）
│   │   ├── preview.py      流式解码预览（seek + 逐帧 RGB）
│   │   └── models.py       任务 / 结果数据结构
│   └── gui/                PySide6 + Fluent-Widgets 壳
│       ├── main_window.py          FluentWindow 主窗 + 转换页
│       ├── preview_interface.py    侧边栏预览页
│       ├── media_player.py         共享播放组件
│       ├── preview_dialog.py       转换结果对话框（复用组件）
│       └── about_interface.py      关于页（折叠分组）
└── tests/                  pytest 回归
```

### 3. 设计原则

**core 层与界面壳完全解耦**——先用 PySide6 快速迭代逆向实验，日后换壳（Tauri / Qt C++）core 不动。

---

## ❓ 常见问题（FAQ）

### Q1：为什么不直接 `ffmpeg -c:v amv`？

ffmpeg 自带 amv 编码器的量化表与设备真值不符——实机 A/B 对比会产生灰底，画质明显劣于原版输出。本项目从 AMV_EncDLL 提取真值量化表/Huffman 表并逐字节复刻比特流，实机画质与原版持平。

### Q2：VLC 里画面上下颠倒？

不是转换器的问题。原版比特流 = vflip(显示内容)（BMP bottom-up quirk），设备直显不翻转；ffplay 正常（ffmpeg 解码器对 AMV 硬编码翻转补偿），**VLC 的 avcodec 模块直接渲染路径丢弃了 ffmpeg 的负 linesize 翻转，属 VLC 缺陷**——原版工具的文件在 VLC 中同样倒置。VLC 请开垂直翻转，PC 预览推荐 ffplay 或本工具预览页。

### Q3：PotPlayer 打不开 / 崩溃 / 乱码？

PotPlayer 不支持 AMV 容器（可致 Access Violation 崩溃，或视频乱码而音频正常）；**AVI 模式产物在 PotPlayer 播放正常**。机制推测见 docs/06（闭源未验证）。设备用 AMV，PC 通用预览请转 AVI 或用 ffplay。

### Q4：设备上快进/拖动进度报"格式错误"？

ATJ 固件的快进/seek **硬依赖 idx1 索引区**（movi 后 `AMV_END_` 哨兵 + idx1），缺失即报错。本项目已按原版布局写入并实机快进验证通过；若你用其他工具产出 AMV，请检查 idx1。

### Q5：支持哪些设备？

实机验证平台为 RUIZU M7（ATJ2157）：128×128@30 / 160×120@30 / 320×240@25 播放 + 快进全过，ATJ2157 硬件解码上限 320×240。对其他 ATJ 型号/固件版本的兼容性未作承诺（格式同源，大概率可用，见 docs/04–05 生态调研）。

### Q6：需要装原版「媒体播放器管理工具」吗？

不需要，也不应该。本重写不依赖原程序、不随附其任何代码与资源；唯一的外部依赖是 FFmpeg（源解码 / donor 容器 / 含声预览）。

### Q7：GUI 预览为什么没有声音？

预览页是 ffmpeg 管道逐帧解码（无音频通道），画面即设备显示取向；**要带声完整播放请点「ffplay 播放（含声音）」按钮**——Qt 多媒体后端受原版 quirk 影响不可用（见 docs/06）。

---

## 📄 许可证与版权声明 (License & Copyright)

- 本项目源码采用 **[GNU General Public License v3 (GPL v3) 许可证](LICENSE)** 进行强制开源，按"现状"提供，不提供任何明示或暗示的保证。
  - **Copyright (c) 2026 CreatorCSIE. All rights reserved.**
- **逆向与第三方免责声明**：
  - 逆向对象《媒体播放器管理工具 4.46》及其内置 AMV_EncDLL/AMV_DecDLL 为**专有软件**，本仓库不含、不分发其任何代码与资源；逆向记录仅作互操作性研究文档。
  - FFmpeg（LGPL-3.0/GPL-2.0，外部进程调用）、PySide6（LGPL-3.0）、PySide6-Fluent-Widgets（GPL-3.0）、NumPy（BSD-3-Clause）等第三方组件版权归各自作者所有。
  - Actions ATJ 芯片与 AMV 格式相关商标、专利版权归炬力（Actions）及其继任者所有。
- 转换与分发内容的版权责任由使用者承担。
