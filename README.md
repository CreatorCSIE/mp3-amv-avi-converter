# amvconv — Actions ATJ 芯片 AMV 视频转换器（现代重写）

面向 **Actions ATJ21xx 系列芯片播放器生态**（RUIZU 等品牌的"带视频功能的 MP4/MP3"）
的开源工具链：逆向老程序「媒体播放器管理工具 4.46」中的 **AMVConverter** 并重写其
转换逻辑，让这类老设备能用现代素材。AMV 是该芯片平台使用的私有视频格式。

> ⚠️ 本项目仅用于**互操作性研究与数据抢救**。逆向过程与证据记录在 [docs/](docs/)。

## 项目性质

项目范围已超出转换器本身，沉淀的是整个 ATJ 芯片生态的互操作知识：

- **格式逆向**：原程序 exe/DLL 静态逆向 + 固件 `MMM_VP.AL` 视频解析器 Unicorn
  预言机 + RUIZU M7（ATJ2157）实机 A/B 标定，三层参照系交叉定案（docs/01–06）
- **固件生态**：ATJ 固件解包（LFI/FWIM/BCH）、固件内视频/音频解码路径分析、
  刷写与救砖工具链调研（docs/04–05）
- **转换核心**：原程序 = 旧版 FFmpeg + 定制封装 DLL。本重写**不依赖原程序、也不
  依赖 ffmpeg 的 amv 编码器**（其量化表与设备真值不符，会产生灰底）：视频走
  **自研纯 Python AMV 原生编码器**，逐字节复刻原版比特流（量化表/Huffman 表/
  DCT/扫描序/容器布局均从 DLL 与实机标定逆向提取），ffmpeg 仅负责源解码与
  容器/音频 donor。输出在 RUIZU M7 实机全档验证通过（播放+快进）。

## 架构

设计原则：**core 层与界面壳完全解耦**——先用 PySide6 快速迭代逆向实验，日后若换 Tauri 壳，core 不动。

```
┌────────────────────────────────────────────┐
│  壳层（可替换）                                │
│   cli.py（当前主力）            gui/ (PySide6 骨架) │
├────────────────────────────────────────────┤
│  core/（壳无关，纯 Python）                     │
│   amvenc.py     AMV 原生编码器（复刻原版比特流）    │
│   _entropy.c    熵编码 C 核（ctypes 自动编译，可回退）│
│   aviwriter.py  AVI 模式 writer（原版布局复刻）     │
│   ffmpeg.py     FFmpeg 子进程（源解码/donor/音频）  │
│   profiles.py   原程序参数表（VIDEOSIZE/AVISIZE）  │
│   amv.py        AMV 容器解析器（字节级对比工具）     │
│   models.py     任务/结果数据结构                   │
└────────────────────────────────────────────┘
```

- 语言：Python ≥ 3.10（numpy）；GUI 依赖 PySide6（可选）；C 核需 gcc（缺则自动回退纯 Python）

## 快速开始

```bash
pip install -e .[gui,dev]          # 或仅 pip install -e .

amv-conv profiles                   # 打印逆向得到的 12 档参数表
amv-conv convert in.mp4 out.amv -s 160x120 -f 30
amv-conv inspect out.amv            # 容器结构摘要（与原版输出对比用）
python -m amvconv.gui.main_window   # GUI 骨架
pytest                              # 回归测试
```

## 当前状态

| 模块 | 状态 |
|---|---|
| 逆向记录（ini/DLL/MMM_VP.AL 解析器/量化表/灰阶探针） | ✅ 完成，见 docs/01–06 |
| AMV 原生编码器（纯 Python + C 核） | ✅ 完成，128x128/320x240/160x120 实机全过 |
| 容器 idx1 尾区（快进/seek 固件硬依赖） | ✅ 完成，实机快进验证通过 |
| AVI 模式 writer（原版头部/AVI1/4B 对齐/idx1） | ✅ 完成，实机验证通过（暂未接入 CLI） |
| 画质对齐（灰底消除/方向/拉伸） | ✅ 实机与原版持平 |
| GUI | 🌱 骨架 |

## 已知格式要点（速览，证据见 docs/06）

- 容器：`RIFF('AMV ')` + `LIST 'hdrl'` 内 56 字节私有头 `amvh`；movi 后
  `AMV_END_` 哨兵 + `idx1` 索引——**快进/seek 硬依赖 idx1**，缺失则报格式错误
- 视频帧：无头 JPEG（FFD8 + 熵数据 + FFD9），4:2:0，量化表/标准 Huffman 表由
  设备固件固化；非 16 倍数尺寸按补齐到 16 倍数的完整 MCU 编码（填充=亮度 0/色度 128）
- 方向：原版比特流 = vflip(显示内容)（BMP bottom-up quirk），设备直显不翻转。
  **PC 预览注意**：ffplay 正常（ffmpeg 解码器对 AMV 硬编码翻转补偿），VLC 显示
  倒置（无补偿）——原版工具的文件在 VLC 中同样倒置，非转换器问题；VLC 请开垂直翻转
- 音频：IMA-ADPCM 变体，**固定 22050 Hz 单声道**，与视频帧 1:1 交织，块样本数 = 22050/fps
- 帧率：UI 表候选（16/25/30 等）中取最大，低帧率源复制帧补齐；22050 需被 fps 整除
- ATJ2157 硬件解码上限 320×240
- 老格式支持（RM/VOB/FLV…）依赖系统 DirectShow 解码器，现代 Windows 上大概率失败——这也是重写时改用 FFmpeg 解码源的原因

## 许可

GNU GPL v3，见 [LICENSE](LICENSE)。
