# amvconv — AMV 视频转换器（现代重写）

逆向老程序「媒体播放器管理工具 4.46」中的 **AMVConverter**，用现代架构重写其转换逻辑。
AMV 是 2000 年代 AMV 芯片MP4/MP3 播放器（"带视频功能的 U 盘"）使用的私有视频格式。

> ⚠️ 本项目仅用于**互操作性研究与数据抢救**（让老设备还能用新素材）。逆向过程与证据记录在 [docs/](docs/)。

## 架构

设计原则：**core 层与界面壳完全解耦**——先用 PySide6 快速迭代逆向实验，日后若换 Tauri 壳，core 不动。

```
┌────────────────────────────────────────────┐
│  壳层（可替换）                                │
│   cli.py（当前主力，对比测试驱动）  gui/ (PySide6 骨架) │
├────────────────────────────────────────────┤
│  core/（壳无关，纯 Python）                     │
│   profiles.py   原程序参数表（VIDEOSIZE/AVISIZE）  │
│   ffmpeg.py     FFmpeg 子进程后端（CLI 调用，已确认）  │
│   amv.py        AMV 容器解析器（字节级对比工具）       │
│   models.py     任务/结果数据结构                   │
└────────────────────────────────────────────┘
```

- 编码引擎：系统 FFmpeg（`-c:v amv -c:a adpcm_ima_amv`），子进程方式
- 原程序对照：`AmvTransforms` COM 组件已注册于本机，可无头调用做 A/B 对比
- 语言：Python ≥ 3.10；GUI 依赖 PySide6（可选）

## 快速开始

```bash
pip install -e .[gui,dev]          # 或仅 pip install -e .

amv-conv profiles                   # 打印逆向得到的 12 档参数表
amv-conv convert in.mp4 out.amv -s 160x120 -f 15
amv-conv inspect out.amv            # 容器结构摘要（与原版输出对比用）
python -m amvconv.gui.main_window   # GUI 骨架
pytest                              # 参数表单测
```

## 当前状态

| 模块 | 状态 |
|---|---|
| 静态逆向（ini/字符串/DLL 清单/COM 注册） | ✅ 完成，见 docs |
| ffmpeg AMV 输出结构实测 | ✅ 完成，见 docs |
| A/B 对比测试（原版 vs ffmpeg） | 🔄 进行中（测试素材准备中） |
| AMV_EncDLL 深挖（amvh 字段/16fps 之谜） | ⏳ 待对比结果驱动 |
| GUI | 🌱 骨架 |

## 已知格式要点（速览）

- 容器：`RIFF('AMV ')` + `LIST 'hdrl'` 内 56 字节私有头 `amvh`（无标准 avih/idx1）
- 视频帧：JPEG 裸扫描数据（无 DQT/DHT/SOS 段，量化表固定）
- 音频：IMA-ADPCM 变体，**固定 22050 Hz 单声道**，音频块与视频帧 1:1 交织，块样本数 = 22050/fps
- 帧率约束：22050 必须被 fps 整除；原程序 UI 表（16/25/30）与内部 AVISIZE 表（14/22）矛盾，待解
- 老格式支持（RM/VOB/FLV…）依赖系统 DirectShow 解码器，现代 Windows 上大概率失败——这也是重写时改用 FFmpeg 的原因

## 许可

GNU GPL v3，见 [LICENSE](LICENSE)。
