# 逆向记录 06 — mmm_vp.al 反汇编（视频头解析器，2026-11-22）

## 模块定位

`mmm_vp.al` = ATJ2157 固件里的 **AMV/AVI 视频解析器**（YQHX 镜像，Thumb-2 代码）。
两个样本：

| 样本 | 大小 | code_base | 可静态反汇编? |
|---|---|---|---|
| X02 (US215A) | 16 KB | `0x120000`（低 RAM 基址，**已解析平铺镜像**） | ✅ 直接线性 Thumb 反汇编干净 |
| M7 全 12 变体 | 24 KB | `0xbfc27000`（高 XIP 基址，**可重定位镜像**） | ⚠️ 需 loader 重定位后才干净 |

两者 **fourcc 字典完全一致**（`AVI AMV JUNK LIST movi strh strf vids auds`），证明是**同一解析算法**，M7 只是更新/更大的编译版本。因此以 **X02 为权威参考**提取校验逻辑；Unicorn 预言机（`mmm_vp_walker_emu.py`）也只在 X02 布局上验证通过。

## YQHX 头字段（实测）

```
+0x00 magic0=0x00040041   +0x04 'yqhx'
+0x08 code_off=0x1000     +0x0C code_sz   (X02=0x2478 / M7=0x47ac)
+0x10 code_base           (X02=0x120000   / M7=0xbfc27000)
+0x14 data_sz             (X02=0x3800     / M7=0x5800)
+0x20 data_base           (X02=0x122800   / M7=0x9fc2b800)
+0x2C init/open VA（奇数=Thumb 位）
```
M7 的 init=0xbfc27aa5 指向 XIP 高位，文件↔VA 非线性 → 静态反汇编出乱码；X02 的 init=0x120000=code_base+0，文件 0x1000 即第一条指令。

## 宿主回调接口（模块对外依赖）

解析器通过 ctx 句柄调用宿主 3 个回调（vtable）：
- `read(H, buf, n)`  @ 0x120c64 → vtable[0]
- `seek(H, off, whence)` @ 0x120c68(CUR) / 0x120c7e(SET) → vtable[1]
- `size/tell(H)` @ 0x120c96 → vtable[2]
- `read4(H)` @ 0x120834：读 4 字节返回一个 dword（不足返回 0）

Unicorn 预言机正是用普通文件实现这 3 个回调，从而"跑固件自己的代码"。

## 视频头校验清单（walker @ X02 0x120878，重写版 writer 必须满足）

返回值：`0`=成功到达 LIST movi；`-1`=无句柄；`-2`=form 类型非法；`-3`=主头不足 56 字节。

1. **偏移 8 的 RIFF form 类型**必须是 `"AVI "`(0x20495641) 或 `"AMV "`(0x20564d41)，否则 `-2`。若 `"AMV "` → 置 is_amv 标志（ctx+0x28）。
2. **主头必须恰好 56 字节**（`avih`/`amvh`，cb=0x38）。读不满 → `-3`。
   - AVI 路径：`ctx+0x18 = hdr[0x30]`（dwBytesPerSecond 类字段）。
   - AMV 路径：`ctx+0x18` 由 hdr 字节经缩放公式算出（帧大小积），`ctx+0x1c=hdr[0x30]`、`ctx+0x20=hdr[0x28]`。
   - 公共：`ctx+8=hdr[0x2c]`、`ctx+0x38=hdr[0x40]`、`ctx+0x3c=hdr[0x44]`。
3. **块遍历循环**（主头之后）逐个读 chunk fourcc：
   - `"LIST"`(0x5453494c) → 读子类型：
     - `"movi"`(0x69766f6d) → 记录 movi 数据起点(ctx+4)与大小(ctx+0x14)，**返回 0（成功）**。
     - 其它 LIST 子类型 → 继续。
   - `"JUNK"`(0x4b4e554a) → 按 size 跳过。
   - `"strh"`(0x68727473) → 读流头。
   - `"strf"`(0x66727473) → 读流格式（先 0x38 字节），再按子类型：
     - `"vids"`(0x73646976) → 读 **BITMAPINFOHEADER 40 字节**(0x28) 到 ctx+0x34，取宽/高，算 fps。
     - `"auds"`(0x73647561) → 读 **WAVEFORMATEX 18 字节**(0x12) 到 ctx+0x5c，取平均字节率/采样率，`udiv by 1000` 算 fps。
   - 未知 fourcc → `-2`；短读 → `-3`。
4. **AVI 布局附加测试**（在 open 分派器 0x120000 的某命令分支，tbb 跳转表 0..0xd）：
   前 512 字节内，form=`"AVI "` 且**字节 342..345 == `"AVI1"`**（首帧 JPEG 的 APP0 标识）。首帧 SOI 必须落在 336。

## idx1 / AMV_ 支持（M7 字典独有）

M7 字典出现 `vidsaudsidx1AMV_`（X02 无 `idx1`/`AMV_`）→ **M7 固件解析器识别 `idx1` 索引块与 `AMV_` 标记**，印证"补写 idx1 可改善快进/seek"的假设（待实机 A/B 验证）。

## JPEG 表（仍未在代码段定位到量化/哈夫曼表）

- M7 代码段尾部 0x4595..0x46f5 有可打印串（如 `%&'()*456789:...`），初判像哈夫曼 val 表，但该区域**线性反汇编为乱码**（XIP 重定位未展开），不能据此定论。
- 结论：量化/哈夫曼表固化在固件 JPEG 解码路径，**不在解析器代码里**；对 writer 而言，帧仍是"裸 JPEG 扫描、无 DHT/DQT 段"，与 docs/03 一致。画质映射（VFBS）仍需实机 A/B 标定。

## 预言机验证（已跑通）

```
mmm_vp_walker_emu.py test.amv --al x02/FWIM_mmm_vp.al
  → r0=0x0 ok: reached LIST movi (read 316 header bytes)
  → movi data starts at 316, ends at 312
  → verdict: ACCEPTED by the firmware checks
```
原版工具产出的 test.amv 被固件解析器接受，证明上述校验清单与真实代码一致。

## 对 writer 的直接结论

重写版转换器要让 M7/X02 固件都能播，输出必须：
1. RIFF form@8 = `"AVI "`（或 `"AMV "`）。
2. `avih` 主头 cb=0x38（56 字节）且字段布局对齐（dwBytesPerSecond 在 +0x30）。
3. 含 `"LIST movi"`（解析器到此返回成功）。
4. AVI 模式：首帧 JPEG SOI@336、APP0 标识 `"AVI1"`@342..345。
5. 视频 `strf→vids→BITMAPINFOHEADER(40B)`；音频 `strf→auds→WAVEFORMATEX(18B)`。
6. （M7 增强）追加 `idx1` 索引块以改善 seek。

## 批量预言机校验结果（2026-11-22）

用 X02 固件解析器（`mmm_vp_walker_emu.py`）对 writer 输出矩阵 + 原工具参考样本逐项跑校验：

| 样本 | 结果 | 说明 |
|---|---|---|
| 我们 AMV：96x64/128x96/128x128@10/160x120@15/160x128@15/160x120@25 | **PASS** | walker r0=0，到达 LIST movi |
| 原工具 AMV：lianqu1980-128x128/-160x120、test14_b、test16_b | PASS | movi_start 在文件尾（idx1 之后） |
| 原工具 AVI：lianqu1980.avi | PASS | **AVI1=Y，SOI=336** |

**14/14 PASS**。过程中发现并修复一个真实 writer 缺陷：

- **`-strict -1` 缺失**：amv 编码器默认拒绝非 16 倍数高度（160x120、176x144），但固件实测接受这些尺寸（原工具 160x120 样本 PASS）。已在 [ffmpeg.py](file:///e:/4k/github-projects/mp3-amv-avi-converter/src/amvconv/core/ffmpeg.py) 的 `build_command` 中按 `height % 16 != 0` 补 `-strict -1`，并加回归测试 [test_profiles.py](file:///e:/4k/github-projects/mp3-amv-avi-converter/tests/test_profiles.py)。

**AVI 模式布局发现**：ffmpeg 原生 AVI（mjpeg）**不满足**固件 open() 的 AVI1 测试——`bytes 342..345=0`、首帧 SOI 不在 336，walker 虽过但设备静默拒绝。→ 我们未来的 AVI writer 必须自定义头部，把首帧 JPEG 精确对齐到偏移 336 且 APP0 标识写 `"AVI1"`（与 docs/04 一致）。当前 writer 仅 AMV，此项列为 AVI 模式实现的硬约束。

## 原工具实现架构（安装目录实锤，2026-11-22）

`C:\Program Files (x86)\媒体播放器管理工具 4.46\AMVConverter`：

- **整套 FFmpeg 1.0 时代动态库**（2012）：`avcodec-54.dll`(40MB, Lavc54.23)、`avformat-54.dll`(Lavf54.6)、`avutil-51`、`avfilter-2`、`swscale-2`、`swresample-0`、`SDL.dll`、`xvidcore.dll`、`pthreadGC2.dll`。负责源格式解码、swscale 缩放、mjpeg/amv 编码。
- **定制封装层**（固件特定逻辑全在这里）：
  - `AMV_EncDLL.dll`(94KB)：含 `amvh`/`idx1`/`RIFF`/`strf` 字符串 → 自研 AMV muxer（写 56B amvh、1e6/fps 字段、idx1 索引、AMV_END_ 尾）。
  - `AVI_EncDLL.dll`(94KB)：含 **`AVI1`**/`idx1`/`avih` → 自研 AVI muxer（SOI@336 对齐 + APP0 注入 `AVI1`，即微软 AVI-JPEG 约定）。
  - `FFMpegDll.dll`(120KB)：GUI 调用的薄封装；`amv.ax`：DirectShow 预览解码器；`transdll.dll`/`AmvTransform.dll`/`amvtransform.exe`：转换调度与 UI。
- **结论**：原工具 = **vanilla FFmpeg 1.0 + 定制 muxer DLL**，不是魔改 ffmpeg 源码。JPEG 编码用 ffmpeg mjpeg（DLL 内无 libjpeg/JFIF 串），APP0 替换/表剥离/DHT 去除由定制 DLL 后处理完成。原工具的已知 bug（idx1 size 2x、AAC 源 785 哨兵）都在定制层，与 ffmpeg 无关——我们只需复刻固件在意的布局，不必复刻 bug。

**对我们架构的印证**：现代 ffmpeg 子进程（core 层）+ 自己的 writer 后处理（amvh/AVI1/idx1 布局）正是同构方案，只是把定制 DLL 的职责用 Python 重写。

## AVI writer 落地（2026-11-22，实机七轮 A/B 定案，T24 正常播放+快进）

**先纠正一个曾经的错误结论**：预言机 walker ACCEPTED ≠ 实机可播。第一版"端到端验证"当时预言机、ffprobe、idx1 自洽全过，但 M7 实机一律"格式错误"拒播。原因：walker 覆盖的只是固件**软解封装层**（AVI1/idx1 布局校验），而**硬解 JPEG 路径**与 **movi 4 字节对齐布局**另有预言机看不见的 gate（见下）。

`src/amvconv/core/aviwriter.py`：ffmpeg 子进程（mjpeg `-huffman default` + adpcm_ima_wav 512B 块）产标准 AVI，Python 后处理重封装——**原版 328B 头部模板与 609B 帧头模板逐字节内嵌**（自 T3 提取，动态字段原地 patch），帧头整段替换保留扫描，movi 按原版 4B 对齐约定交织，idx1 写对齐后长度。

### 实机七轮 A/B（M7 U 盘，源 lianqu1980.mp4 → 128x128 / 4278 帧，T3ORIG 恒为基线）

| 轮 | 样本 | 实机结果 | 结论 |
|---|---|---|---|
| 1 | T1=我们输出 / T2=T1 删 idx1 / T3=原版 / T4=ffmpeg 原生 | T1/T2/T4 全部"格式错误"；T3 正常 | AVI1 关卡过了也没用，问题在帧内容 |
| 2 | T6=旧帧+头部 quirk / T8=修帧+干净头 / T9=修帧+quirk 头 | T6 无首帧；T8/T9 首帧乱码后报错；T8≡T9 | 乱码=哈夫曼表退化；"quirk 无关"当时成立但后被推翻（被帧 bug 掩盖） |
| 3 | T10STD = `-huffman default`+段序修复+干净头 | 初报"正常"实为**误诊**（滑到 T3 看错）；复测仍乱码 | 表值标准化不够 |
| 4 | T12DHT = T10 + 多表 DHT 拆成一表一段 | 仍乱码 | 拆段必要但不充分 |
| 5 | T13HDR = **T3 帧头模板注入**+我们的扫描 | 首帧黑（源片黑场淡入，非 bug）但**不再报格式错误**；随后放弃 | 帧头 gate 确认；剩余问题在帧外 |
| 6 | T15/T16 二分（T3 帧+我们音频 / 我们帧+T3 音频）；T17/T18（quirk 头+pad idx1）；T20=T3 帧+T3 音频+我们容器；T21=T3 头部+我们 movi | 全部"首帧后放弃"；T20 挂 → **容器有罪**；T21 挂 → 差异在 movi | "帧/音频各自有罪"是布局错位的假象 |
| 7 | **静态突破**：T3 的 idx1 size=4B 对齐长、块头 size=真实长、payload 零填充到 4B；T23=按此约定重建 → **与 T3 逐字节 0 差异**；T24=我们的帧+我们的音频+全套原版约定 | **T24 实机正常播放，快进成功** | 定案 |

### 固件的完整硬 gate（预言机/ffprobe 均不可见）

帧级（JPEG 硬解）：

1. **哈夫曼表退化**：新版 ffmpeg 默认 `optimal` 按帧内容生成极简表，固件只认标准表。修复：`-huffman default`（保证扫描按标准表编码）。
2. **帧头结构**：APP14 前置、DHT-before-SOF0、多表合并 DHT 段（新版 ffmpeg 把 4 张表串进一个 418B 段，规范允许、ffprobe 全解，固件每段只读第一张表）均致死。修复：`fix_jpeg_layout()` 整段替换为原版帧头模板（APP0 AVI1 + DQT×2 + SOF0 色度 Tq=1 + DHT×4 一表一段），保留我们的扫描数据。

容器级（demux/DMA）：

3. **movi 4 字节对齐**：视频 payload 零填充到 4B；块头 size=真实 JPEG 长（不含 pad）；idx1 size=**对齐后**长度。我们旧的 2B 补位布局在第一个非 4 对齐帧后使后续块全部错位 → 固件按对齐粒度消费即崩（"首帧后放弃"的统一解释）。
4. **328B 头部区逐字节复刻原版**：avih 保留区 [22201,1,0,785]、strh_v.length=2、strh_a dwLength=n×258、wPriority=2 等 quirk **是 gate**（第二轮"quirk 无关"结论系被帧级 bug 掩盖的误判；T20 用健康流复测才暴露）。生产做法：内嵌 T3 头部模板，仅 patch RIFF size / dwTotalFrames / movi LIST size / dwLength。

### 当前验证状态（定案）

- 回归测试 `tests/test_aviwriter.py`：**9 passed**（布局/模板注入/幂等/透传/quirk 在位/4B 对齐/自定义 filter）。
- **端到端逐字节复现**：生产 `remux()` 输出与实机通过的 T24OURS4 仅差 2 字节（dwLength 由 T24 沿用 T3 的 4293×258 修正为 4278×258，属有意修正）。
- 预言机 X02 walker ACCEPTED + ffprobe 全解。
- 开发中修掉的真实缺陷：strf_a 打包串少一个 I（hdrl 302B 破坏 336 对齐）；idx1 首条偏移基准误 +8（应为 +4）；movi LIST 头重复写入；`_patch_app0` 偏移错 2B。

## 纯 Python AMV 原生编码器落地（2026-11-23，P5 实机三项全过）

**实机结果（M7，A128P5.AMV）**：方向正常、画质与原版持平、灰底消失 → 纯 Python
复刻原版比特流路线定案，已落地生产（`src/amvconv/core/amvenc.py` + `ffmpeg.py`
双通道管线，CLI/GUI 的 `ffmpeg.convert` 直接走新编码器，不再用 ffmpeg amv 视频编码）。

### 画质/方向三项标定定案（ORIG=lianqu1980-128x128.amv 真值裁决框架）

1. **灰底根因 = 量化表值**：设备解码模型 `pixel = IDCT(q×T)/8 + 128`，黑帧
   DC=-128 要求 T_dc=8 才解出 0。ffmpeg amv 编码器 Q60 的 DC 表值 13 → 设备解出
   50（灰）。真值表 `T_ORIG_Y/C` 提取自 AMV_EncDLL.dll VA 0xe348（i16）。
2. **表序配对 = 自然序 × ZIG**：`T_ORIG` 列表是**自然序**；熵编码的 zigzag 序系数
   须除以 `T[ZIG]`。证据：AVI 模式原版帧头 DQT0/DQT1 == `T_ORIG_Y[ZIG]`/`T_ORIG_C[ZIG]`
   逐字节（已固化为回归测试 `test_dqt_pairing_natural_order`）。曾误把自然序当
   zigzag 序用，表值错位一格 → 画质劣化。
3. **方向 = 原版比特流是 vflip(显示内容)**：ORIG 帧与 vflip(源 mp4) 相关 0.975+
   （20 帧裁决 19 胜，ORIG#700），设备按比特流直显 → 编码器必须**预 vflip**。
   对应原工具 BMP bottom-up quirk。此前"水平翻转+逆时针180°"即缺这一步。
4. **哈夫曼表 = 设备内置标准表**：与 AVI 模式原版帧头 DHT 段一致；K.3 变体表
   编的内容设备解不出（P5 首测炸因）。生产做法：从 `_ORIG_FRAME_HDR` 解析 DHT，
   单一事实源。
5. **帧格式 = 无头 JPEG**：`FFD8 + 熵数据 + FFD9`，无 DQT/DHT/SOS；4:2:0，8x8
   MCU 光栅序。量化舍入 round-half-away-from-zero 与原版一致。

### 容器与管线（AMV 原生模式）

- **头部**：ffmpeg amv muxer 产物与原版工具头部逐字节一致（316B 区，仅 amvh+12
  标志位与 movi LIST size 两个非 gate 字段差异，实机均不校验）。
- **donor 通道**：amv muxer 硬要求恰好 2 条流 → `-f lavfi color=black:WxH:r=F`
  假视频 + 真音频（`adpcm_ima_amv` 376B 块），只取头部与音频块，开销可忽略。
- **视频通道**：rawvideo 管道（`scale=W:H` 硬拉伸+`fps=F`+yuvj420p）→ Python
  逐帧编码；帧数不足按音频块数重复末帧补齐；奇数块**不填充**（原版行为）。
- **验证**：`tests/test_amvenc.py` 黑帧逐字节 == ORIG 首帧；端到端 10s 冒烟
  301/301 帧与实机验证版 P5 逐字节一致。
- **320x240@25 实机通过（A320P1，4933 帧/35.5MB，CLI 生产管线直接产出）**：
  编码器与容器完全分辨率无关，顶到 ATJ2157 硬解上限的档位同样正常播放；
  原版工具 320x240 档实测可播（用户确认），非 16 倍数档位已终裁并实机定案（见下节）。

### C 核加速编码（2026-11-23）

- 实测编码循环占编码总耗时 **89%**（320x240：211/238ms），numpy 量化段仅 26ms。
- `_entropy.c`（gcc -O2 共享库，ctypes 加载，导入时缺库自动编译、源码更新自动重编）
  复刻同一熵逻辑；**与纯 Python 路径逐字节一致**（随机帧/平滑帧/非 16 倍数补边），
  端到端 301/301 帧仍与实机验证版 P5 一致。提速 ~7x：320x240 全曲 ~20min → ~2.7min。
- 踩坑两枚（已固化为测试）：① `[..., ZIG]` 高级索引结果可能非 C 连续，C 核按连续
  内存读取会静默失败回退（`ret=-1` 被当溢出哨兵吞掉）——`_quant_zig` 必须
  `ascontiguousarray`；② `while (a >>= 1) s++` 位长少计一位，正确写法 `for (t=a; t; t>>=1) s++`。

### 测试脚本教训（防再犯）

- recon 时 `(16,16,8,8)` 块阵列 → 图像必须 `transpose(0,2,1,3).reshape(128,128)`，
  直接 reshape 会打乱子块（曾致多轮 0.4 低相关误判）。
- ORIG↔源 mp4 帧映射 1:1（off=0）；源片头 20 帧全黑，别拿它们当内容帧裁决。

### 160x120（非 16 倍数档）末 MCU 行结构终裁（2026-11-23）

参考真值：原版工具转出 `lianqu1980-160x120.avi`（根目录，4293 视频帧，
usPerFrame=46122 即 21.68fps，AVI 模式**无 vflip**=显示序）。裁决脚本
`chk160u/x`（temp disasm），解码器模型 = 帧头真实 DHT/SOS 段 + T_ORIG 表。

1. **失步根因 = 色度 AC 表非 ITU K.3 标准**：原工具（FFmpeg 1.0 mjpegenc）帧头
   DHT 0x11 bits 末项 **0x77**（162 符号），非标准的 0x7D（168 符号）；与设备
   AMV 模式标定值一致 → 两套表同源，此前用 K.3 表解 AVI 参考流必失步。
2. **末行结构 = B（补齐到 16 倍数，末 MCU 行完整 6 块）**：120 行 → 编码
   128 行/8 MCU 行，480 块/帧。变体扫描（A=partial 4 块 / B=补齐 / C=仅 Y 2 块）
   × 序（YYUV/YYVU）× 表配对共 12 组合：仅 **B 全帧干净消费**
   （tail=0B、badDC=0）且 Y 重建与 ffmpeg 解码 corr=1.0000（frame 2000）；
   复验 500/1234/2500/3900 帧 corr≥0.99993，A/C 全部否决。
3. **MCU 内色度序 = YYUV（Cb 前 Cr 后）**：色度平面重建对比 ffmpeg 解码
   U/V：YYUV corrU/V=0.9995+，YYVU 崩至 0.48（DC 链交叉污染）。
4. **补边填充 = 亮度 0（黑）/ 色度 128（中性），非边缘复制**：原工具流中
   补齐区重建值 padU_mean=128.0、padY_mean≈0–5，与内容行相关 0.32（复制应≈1）。
   → `encode_frame` 补齐分支从 `mode="edge"` 改为 `constant_values`（0/128/128）。
5. **字节级比对原工具不可行**（定论）：原工具用 fdct_ifast 整数 DCT +
   swscale-2，系数与我们精确 DCT 不同，内容帧前缀仅 15–17B 一致；
   黑帧逐字节一致仍成立。裁决只能走解码结构 + 像素相关路线。
6. **自洽验证**：新填充下 CLI 生产管线出 `T-160x120.amv`（15s@30fps，451 帧）；
   设备模型解码全帧 DONE/480 块/tail=0/badDC=0，padU=128，
   与源帧（vflip 后）corr≈0.998。回归 17 passed。
7. **实机定案（M7，2026-11-23）**：T-160x120.amv 播放正常、底部 8 行无偏色 →
   补齐结构 B + 黑/中性填充 + YYUV 全链路验证通过。至此 128x128 / 320x240 /
   160x120（非 16 倍数代表档）三档实机全过，编码器完全通用。

## idx1 实效验证与 AMV 尾区修复（2026-11-24，实机定案）

AVI 三件套实验（生产管线 128x128 短片 326 帧，同 T24 约定）：

| 文件 | 改动 | 实机表现 |
|---|---|---|
| IDX-A0 | 基线（idx1+块头双正确） | 正常 |
| IDX-A1 | 删尾部 idx1（RIFF size 修正） | 顺序播放正常；**快进报格式错误** |
| IDX-A2 | idx1 首尾视频条目 (off,size) 互换 | **进度条异常，快进 1s 内报格式错误** |

**定案**：固件顺序播放走 movi 块头流式解码（不依赖 idx1）；**快进/seek 与时长
计算完全依赖 idx1**，无 idx1 或 idx1 错乱即报错。A1 排除了"块头扫描兜底"假说。

**原版 AMV 尾区实测布局**（lianqu1980-128x128/-160x120.amv）：
`AMV_END_`(8B 裸哨兵，无 size 字段) + `idx1` + size(4B) + 16B 记录×2N
（fourcc+flags 0x10+off+size；off 基准 = 'movi' fourcc 位置，首块 off=4；
音视频 1:1 交织；size 字段=真实长度）。128x128 文件 size=2×bug、160x120 文件
size=正确值，**两者均可播** → 写正确值。原版 RIFF size 字段=0（未回填），
设备同样兼容我们回填的版本。

**生产缺口修复**：此前 AMV 输出无 idx1 尾区 → 快进必报格式错误。
`amvenc.build_file` 已补写尾区（RIFF size 含尾区、movi size 仅数据区），
回归测试校验每条记录指向正确块头。修复后 `T-160x120.amv` 实机
播放+快进全正常 → AMV 模式与原版工具功能完全对齐。

**DLL 侧佐证**（AMV_DecDLL.dll 静态逆向，ImageBase 0x10000000，32 位）：
- `AmvDecoder` @VA 0x16d0：每次 fread 8B 与 `"AMV_END_"`(@0x1000b0bc) 比对，
  命中 → return 2 流结束；未命中 fseek(-8) 回退后解一帧 → **哨兵就是解码器
  的流终止协议**，walker 不覆盖此路径（解释了为何无尾区文件 walker 也 PASS）。
- `AmvInforInit` @0x1630：fopen 路径入参（≥0x104B 调用者缓冲），解析文件头
  填全局 fcd4..fd1e（文件镜像静态全 0，运行时填充）；`AmvClose` @0x17d0 释放。
- @0x17f0 = IMA-ADPCM 音频解码（step 表 @0x1000b108、索引 clamp 0x58、
  nibble 步长表 @0x1000b0c8）——音频路径与视频解耦，佐证 376B 块假设。

**PC 播放器取向差异（VLC 倒置之谜，2026-11-24 定案）**：用户实测本项目输出在 VLC 中
倒置、ffplay 正常。根因链（VLC 3.0.24 源码 + 官方 win64 便携版实测闭环）：

1. ffmpeg `mjpegdec.c` decode_init 对 `AV_CODEC_ID_AMV` 硬编码 `s->flipped = 1`，
   解码末尾用**负 linesize 技巧**翻转返回的 AVFrame（`data += (h-1)*linesize;
   linesize *= -1`，ffmpeg 3.4~7.1 均如此，与容器头部无关——原版与本项目输出的
   strf/strh 均全零，无任何方向字段）。
2. ffplay 直接消费 AVFrame → 正常。VLC 走 avformat demuxer（merge-ffmpeg 构建，
   日志 `demux module "avcodec"`、`adding es: video codec = AMV`、`codec (amv)
   started`）→ 解码器**确实识别为 amv**，但其直接渲染路径在解码前就把
   picture_t 绑定到 buffer 正 pitch，解码后直接使用 picture_t、**丢弃 AVFrame
   上的负 linesize 变更** → 显示未翻转的原始流 = 倒置。VLC 快照实测：渲染帧
   160x128 顶部 8 行黑填充 = 原始比特流取向（corr 0.76 vs vflip(源)）。
   （VLC 3.0 已移除 `--avcodec-dr` 选项，DR 是 amv 唯一路径，无法在 3.0 上
   验证非 DR 分支；`lavc_CopyPicture` 的 size_t 负 stride 因补码回绕理论上
   反而能正确翻转，但未实测，不下结论。）
3. **原版工具文件在 VLC 中同样倒置**（lianqu1980-160x120.amv 实测）——本项目输出
   与原版行为完全一致，非转换器 bug，是 VLC avcodec 模块对负 linesize 帧的已知
   处理缺陷。PC 预览用 ffplay 等直接消费 AVFrame 的播放器，或 VLC 开垂直翻转。
   补齐档（非 16 倍数）VLC 显示 128 行全高（含黑填充），ffplay/设备按 120 裁剪。

**PotPlayer 对 AMV 容器的已知缺陷（2026-11-24，用户实测）**：打开 AMV 容器文件
直接 Access Violation 崩溃（PotPlayer64.dll 弹窗），部分环境不崩则视频全乱码、
音频轨正常；**AVI 容器（原版工具 AVI 模式/本项目 aviwriter 产物）播放正常**。
实证事实：① 上述行为（用户观察）；② 两种容器的结构差异（本项目字节级验证：
AMV 容器无 `avih`、strh/strf 全零，帧为无头 JPEG；AVI 容器头部正常、帧带完整
JPEG 头 APP0/DQT/SOF0/DHT/SOS）。
**机制推测（未验证，PotPlayer 闭源，其内部实现无任何证据）**：崩溃可能源于
其 AVI 解析器对退化头部（无 avih）的空引用；乱码可能源于 MJPEG 解码路径拿不到
固件固化的量化/Huffman 表；音频正常因 ADPCM 简单。总判断"PotPlayer 无 AMV
支持"由行为成立，但分工归因待证——可证伪实验：给 AMV 容器补正常 avih/strh
（帧不动）看崩溃是否消失；AMV 帧内嵌 DQT/DHT/SOS 看乱码是否消失。
结论：AMV 容器面向设备；PC 通用预览请转 AVI 容器或用 ffplay。

## 下阶段

1. ~~idx1 实效验证~~ ✅ 已完成（见上节：快进硬依赖 idx1，AMV 尾区已补写并实机定案）。
2. ~~实现 AVI writer 时落实 SOI@336 / AVI1@342 布局，并用预言机 AVI1 测试卡关。~~ ✅ 已完成（且实机定案）
3. ~~逆向 AVI_EncDLL.dll~~ 可降级为佐证：完整约定集已由 T23≡T3 逐字节复现逆出，DLL 非必需。
4. ~~AVI writer 接入 cli.py / gui.py 管线~~ ✅ 已完成（CLI `--avi/--q`；GUI 批量队列
   +进度/取消+预览，见下节）。核心层为支持 GUI 增加：`ffmpeg.convert` 与
   `aviwriter.convert_avi` 的 `progress(done,total)/cancel(Event)` 参数；
   `core/preview.py` 流式解码预览（ffmpeg 的 AMV 解码器 flipped=1，输出即设备显示
   取向；AMV 容器 probe 时长为 0，用 01wb 块数×1017/22050 容器级兜底）。

## 官方套件安装目录盘点（2026-11-24，用户机 `C:\Program Files (x86)\媒体播放器管理工具 4.46\`）

- `AMVConverter\`：即逆向对象本体——`AMV_EncDLL.dll`(94208B，T_ORIG 出处)、
  `AVI_EncDLL.dll`（与 EncDLL 同尺寸，两模式共用编码器旁证）、`avcodec-54.dll`
  （FFmpeg 1.0 实锤）、`amv.ax`（**DirectShow AMV 解码滤镜**——原厂商的 PC 播放方案，
  注册后 WMP/MPC-HC 等 DShow 播放器可播 AMV，补全 VLC/PotPlayer 缺陷叙事）
- `AMVPlayer\`：官方 PC 播放器 `amvplayer.exe` + `AMV_DecDLL.dll`(73728B，即逆向
  过的 DecDLL 本尊) + SDL.dll；`AmvPlayer.ini` 记录 UI 档位索引持久化
  （VIDEOWH/VIDEOFRAME）与 MTVFILEEXTEND=amv
- 后续调研（未做）：amv.ax 逆向（交叉验证 DecDLL 表结论）；amvplayer.exe 作
  解码 oracle；PotPlayer 根因分析路线（WER LocalDumps → WinDbg → 变异矩阵）

## DecDLL 数据段深扫（2026-11-25）

AMVPlayer\ 与 AMVConverter\ 两份 `AMV_DecDLL.dll` SHA256 一致（F2BDB004…4F56B）。
导出表仅 3 函数：`AmvInforInit`@0x1630 / `AmvDecoder`@0x16d0 / `AmvClose`@0x17d0。
`.data` 段（VA 0x1000b000 起，镜像静态已初始化）宝藏：

1. **标准 JPEG Annex K Huffman 表全套实锤**——DC 亮度 BITS@0xb532
   （`01 05 01 01 01 01 01 01 00…`）、DC 色度 BITS@0xb542（`03 01…01 00…`）、
   DC VALS@0xb559（1–0x0b）、AC 亮度 VALS@0xb659（162B 逐字节=标准表）。
   → **AMV 熵编码用标准固定表，无自定义 VLC**，与本项目编码器比特流复刻互证
   （docs/01 的 Huffman 结论获解码端独立佐证）。
2. **容器解析标签表**@0xb030-0xb0b8：`movi/strf/strh/strl/amvh/hdrl/LIST/AMV /
   RIFF/00dc/01wb` + 错误文案 "Input not a AMV file - starts not with "RIFF""。
   注意 `amvh`/`strl` 也在查找列表——解码器容忍的额外 FourCC（本项目未产出，
   但说明原版解析器比 mmm_vp walker 覆盖面宽）。
3. **IMA-ADPCM 标准表逐字节吻合**：step@0xb108（7,8,9,…,2457）、index 调整表
   两份副本@0xb0c8/0xb0e8（-1,-1,-1,-1,2,4,6,8,…）→ 音频路径 = 标准 IMA，
   无任何厂商魔改（376B 块假设再获旁证）。
4. **未知 64×i16 表**@0xb430：`9,9,12,24,50×4 | 9,11,13,33,50×4 | 12,13,28,50×4 |
   24,33,50×6 | 50×32`（自然序，50 疑似哨兵）。形态像 DCT 频带码率成本表
   （左上角低频代价小、其余封顶），但出现在解码器中语义未定——**不定案**，
   候选解释：① 编码器残留（Enc/Dec 同源代码）② 错误隐藏权重。待交叉
   EncDLL 同偏移比对。
5. **无 YUV→RGB 浮点/双精度常量**（全文件扫描 1.402/0.344136/0.714136/1.772/
   0.299/0.587/0.114/1.164 零命中）→ DecDLL 大概率直接输出 YUV，RGB 转换在
   amvplayer.exe + SDL 层（或整数定点内联）。设备色彩语义（full-range 假设）
   不受影响，但 PC 官方播放链的色彩矩阵归属定了：**不在 DecDLL**。
6. 字符串区无调试符号、无自定义错误文案（除容器报错）——Release 构建，
   静态逆向天花板即此；amv.ax（118784B，独立 DirectShow 滤镜）仍是下一个
   调研对象，可交叉验证 1/3 表结论并可能含色彩矩阵。

## 预览架构调研（2026-11-24，QMediaPlayer 实测）

- Qt 6.9 多媒体默认 FFmpeg 后端：AMV/AVI 两容器均能打开（无 errorOccurred），
  但**时长读数继承原版 quirk**——`strh_v.length=2` 使 demuxer 报 92ms 即
  EndOfMedia（ffprobe 对原版参考文件同样报 0.092244s，佐证 quirk 复刻忠实）。
  **QMediaPlayer 方案不可用，预览保留 ffmpeg rawvideo 管道**（逐帧 RGB，
  方向=设备显示取向）；带声音的完整播放由预览窗"ffplay 播放"按钮外包。
- 关于页内容结构借鉴 Attic 启动器 SettingsAbout（头区/法律声明/引用项目列表，
  许可证逐字实查：FFmpeg LGPL-3.0/GPL-2.0 按构建、PySide6 LGPL-3.0、
  qfluentwidgets GPL-3.0、numpy BSD-3、atj2157-toolkit MIT）。

## C++ 化路线调研（2026-11-24，2026-11-25 定稿开工）

用户意向：核心与壳全 C++（Fluent 设计语言不变），从 PySide6 分支切新分支；
Python 版永久保留作金标准预言机。计划：Phase0 金标准语料 → Phase1 libamvconv
（确定性 float64 DCT 逐位对齐 numpy einsum 收缩序 + _entropy.c 复用 + 容器 +
ffmpeg 管道）→ Phase2 Qt C++ 壳 → Phase3 windeployqt 打包。

C++ 侧 Fluent 组件库候选（实查）：

| 库 | 许可 | 状态 | 备注 |
|---|---|---|---|
| **Fairy-Oracle-Sanctuary/Qt-Fluent-Widgets** | GPL-3.0 | 活跃，Qt5.15.2/6.x | PyQt-Fluent-Widgets 的**原生 C++ 移植**，API 对应现 Python 版可近似 1:1 翻译；静态库、三主题、Gallery 示例、跨平台、MSVC /FS |
| C++ 版 QFluentWidgets（zhiyiYo 官方） | 闭源 | 商业买断 | 组件最全，付费 |
| zhiyiYo/QFluentKit | GPL-3.0 | 停维护 | 早期 C++ 版，需 vendor+修 Qt6 |
| zhuzichu520/FluentUI | MIT | 活跃(2025-03) | QML 栈，UI 层需重写而非翻译 |

首选 Qt-Fluent-Widgets（GPL 兼容 + 翻译成本最低）；动工前需实际 clone 编译验证
Gallery。

## C++ 移植定稿（2026-11-25，Phase0 开工）

分支策略：`C++` 分支（自 PySide6@7b984b3 切出）为主线兼默认分支；PySide6 分支
冻结为金标准预言机（只维护不加功能）。坑：Actions 的 `branches` 过滤器是 glob，
`+` 非合法模式字符——ci.yml 直写 `C++` 会让整个 workflow 解析失败（零 job 秒挂），
已用通配 `"C*"` 匹配。

分层定案：
- **libamvconv（C++ 核心库）**包含：编码内核（amvenc.py 342 行 + `_entropy.c`
  直接复用）、容器写入（amv.py / aviwriter.py）、profiles 参数表（单一事实源，
  CLI/GUI 共用）、**ffmpeg 子进程编排（定案：归 core，不归壳）**、预览流
  （preview.py）。公开 API：`convert(in, out, profile, progress_cb, cancel_flag)`
  ——签名照搬 Python 版现有 `progress(done,total)/cancel(Event)`。
- 壳层薄：C++ CLI（终结 Nuitka 双 exe ~25min 构建与 onefile 冷启动）+ Qt C++ GUI
  （Qt-Fluent-Widgets，自现 PySide6-Fluent-Widgets 代码近似 1:1 翻译）。
- 构建栈：CMake + MSVC + Qt6，C++20。

Qt 环境排雷（2026-11-25 实测，Phase2 前置结论）：
- **conda base 的 Qt 不可用**：qmake 实为 5.15.2（非 6.8.3），全部库带 Anaconda
  私有 `_conda` 后缀（Qt5Core_conda.dll），无开发头、无 Qt6、无 Qt6 CMake config
  ——是 conda 自家 GUI 的私有运行时包，不是给外部 C++ 开发用的。
- Phase2 的 Qt 用 **aqt 解包官方归档**（aqtinstall 3.3.0 已装）：纯 portable 文件夹、
  零安装零注册。候选 `win64_mingw`（配现有 gcc 13.3，内核线已验证）或
  `win64_msvc_2022_64`（本机已有 BuildTools 14.44 / MSVC 14.44.35207）。
- Phase1 零 Qt 依赖（内核 + CLI 纯 MinGW 即可），不受此排雷影响。

验证与对齐标准（三项均已用户确认）：
- Phase0：`scripts/gen_golden.py` 生成金标准语料——lavfi 确定性合成源（lossless
  x264 + PCM，CI 可再生）+ 可选真实素材（`--src`），矩阵=12 档 × fps 候选 ×
  AMV/AVI 模式 + flip/q 变体，产物 SHA256 写入 `golden/manifest.json`（媒体本体
  被 .gitignore 排除，不入库）。
- Phase1：C++ 测试 CLI 产物与语料 SHA256 全等比对，进 CI。
- DCT 分层标准：先逐位对齐（固定收缩序、`/fp:strict`、禁 FMA/向量化重排）；
  对不上的档降级"差异帧占比 < 阈值 + 设备实机抽检"，逐档记录本文。
- Python 版永久保留为金标准预言机；逐位 diff 是最强回归工具，不轻易放弃。

阶段：Phase0 语料 → Phase1 libamvconv + C++ CLI + 比对 CI → Phase2 Qt 壳 →
Phase3 windeployqt 打包 + CI 发版（替换 release.yml 的 Nuitka 路线）。

### Phase1 里程碑1：编码内核逐位达成（2026-11-25）

- **einsum 收缩序定案**（探针实测，20480/20480 逐位）：
  `np.einsum("ab,nbc,dc->nad")` 非优化路径 = 对每个 (n,a,d) 输出，**b 外 c 内**
  逐项 `acc += (M[a,b]*X[b,c])*M[d,c]`（左结合，无 FMA）。两阶段 matmul/BLAS、
  c 外 b 内等候选全部逐位不符——C++ 照抄该嵌套即可。
- 常量策略：DCT_M/TY/TC 以 **float64 位型十六进制**导出（gen_tables_h.py →
  tables.h，`std::bit_cast` 初始化），杜绝 cos/sqrt 跨实现 ULP 差异。
- 熵编码：直接编译 `src/amvconv/core/_entropy.c` 原文件（单一事实源，零改动）。
- 坑：C++ `%` 向零截断，Python 向上取整 `-h%16` 惯用法必须换 `(h+15)&~15`
  （曾致 H 变小、memcpy 越界 fastfail）。
- 基准：`scripts/gen_frame_golden.py` 产帧级金标准（4 档 × 含 pad 路径/双素材），
  `cpp/tools/enc_test.exe` **bit-exact 10/10**。构建需 `-ffp-contract=off`
  （GCC 默认融 FMA）。

### Phase1 里程碑2+3：容器/编排移植，文件级逐位达成（2026-11-25）

- `cpp/src/container.cpp`：amvenc.split_amv/build_file + aviwriter
  _packets/_patch_app0/fix_jpeg_layout/remux 移植（纯字节机械；原版模板经
  `gen_container_h.py` 从 aviwriter 导出 `container_data.h`，单一事实源）。
- `cpp/src/ffproc.cpp`：donor 通道 + rawvideo 管道逐帧（vflip 在管线侧，同
  Python）；子进程 system/popen，取消=置位后停读+关管道。AVI 侧 remux 后
  4B 对齐/idx1/RIFF 回填全对齐。
- `cpp/tools/amvconv_cli.cpp` + `scripts/compare_cpp_golden.py`：读
  golden/manifest.json 全量重跑——**cpp-vs-golden 57/57 逐位一致**
  （AMV/AVI 双通道 × 12 档 × fps/flip/q 变体，一次通过）。
- CI 新增 `cpp-bitexact` job：同 runner 现生成语料（版本漂移由"现生成"消化）
  → 帧级 enc_test → 文件级 57 条比对。

## GUI 环境坑（2026-11-24 实测）

- **PySide6 6.12.0 wheel 在本机（Win11 26100）QtCore DLL load failed 127**：Qt6Core
  动态导入 `icuuc.dll`，解析到 System32 的 MS ICU 72（conda 构建为 73，符号后缀
  `_73` 不匹配；20 个 ucnv_* 符号名虽在但版本组合仍致加载失败）。anaconda base 与
  venv 同现。**解决：钉 PySide6 6.9.3**（pyproject gui extra 已加 `<6.12`），
  配 PySide6-Fluent-Widgets 1.11.3（GPL-3.0 与本项目 license 兼容）
