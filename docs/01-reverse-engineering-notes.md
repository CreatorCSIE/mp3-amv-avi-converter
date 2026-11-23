# 逆向记录 01 — 静态摸底与 ffmpeg 实测（2026-11-22）

目标程序：`C:\Program Files (x86)\媒体播放器管理工具 4.46\AMVConverter`

## 1. 程序栈识别

| 文件 | 结论 |
|---|---|
| amvtransform.exe | MFC42 / Win32 32 位，BMP 贴图皮肤 UI |
| transdll.dll | 导出 `CreateAmvInterface` / `AmvClose` / `GetDeviceIdList`；字符串含 `AmvHeaderEncoder`、`hdrlamvh`、`:\idx1.dat`、`temp0.dat`、`AMV_END_`、`CAmvTransform` |
| AmvTransform.dll | COM 组件，ProgID `AmvTransforms` / `AmvTransform.1`，CLSID `{C8CA0A66-AF32-4D5E-879E-F0809ACEDC55}`，**本机已注册**（可无头调用） |
| AMV_EncDLL / AMV_DecDLL | 自研 AMV 编解码器 |
| 捆绑 FFmpeg 0.11 | avcodec-54 + xvid + SDL（老版本，仅内部使用） |

## 2. AmvTransform.ini（GBK）关键参数

```
VIDEOSIZE=96X64(16;25;30),128X96(16;25;30),128X128(16;25;30),160X120(16;25;30),
160X128(16;25;30),176X128(16;25;30),176X144(16;25;30),192X160(16;25;30),
208X120(16;25;30),208X160(16;25;30),208X176(16;25;30),320X240(16;20;25)
AVISIZE=（同 12 档，全部为 (14;22;22)）
CHIPTYPE=1, BMPADJUST=0, MEMSIZE=30000000, DISKSIZE=1600000000
```

**发现**：UI 帧率表(16/25/30) 与 AVISIZE(14/22) 不一致。22050 的整除因子含 14/15/18/21/25/30，
不含 16 与 22 —— 两个表都不能完全满足整除约束，说明原工具对非整除帧率有填充/截断策略，待深挖。

## 3. ffmpeg AMV 输出实测（gyan 2023-02，96x64@15，15 帧样本）

命令核心：`-c:v amv -c:a adpcm_ima_amv -ar 22050 -ac 1`，`-block_size = 22050/fps`（如 15→1470）。

结构结论（`amv-conv inspect` 可复现）：
- **所有容器块 size 字段写 0**（RIFF 与全部 LIST 都不回填）；LIST 类型字段除首个 `hdrl` 外也写 0
  → 解析必须平铺扫描，不能按 size 递归
- **奇数大小的叶子块不写填充字节**（00dc=1155 后紧跟 01wb 头），与标准 RIFF 相悖
- 块序列：`LIST(hdrl)>amvh(56)` `LIST>strh(56)+strf(36)` `LIST>strh(48)+strf(20)` `LIST>{00dc,01wb}×N`，**无 avih、无 idx1**
- `amvh` 56 字节：`+0 = 66667 (0x0001046B，疑似签名/版本)`，`+32=宽` `+36=高` `+40=fps`，尾部三 dword `[1,0,1]`（含义待与原版对比）
- 视频帧 `00dc`：JPEG 只有 `FFD8…FFD9`，无 DQT/DHT/SOS —— 裸扫描数据 + 固定量化表
- 音频块 `01wb`（15fps 时恒 743 字节）：`4B predictor(0) + 4B 样本数(0x5BE=1470) + 1470×4bit ADPCM 数据`
  → **每帧音频样本数 = 22050/fps 得到直接证实**
- 音频与视频 **1:1 交织**；文件尾有字面量 **`AMV_END_`**（8 字节，ffmpeg 也写！）
- `16fps` 被 ffmpeg 直接拒绝（22050 % 16 ≠ 0）

## 3.5 ffmpeg amvenc 高度硬编码限制（实测枚举）

对 64/96/112/120/128/144/160/176/192/208/224/240 全部实测：**仅 height=120 被拒**
（amvenc.c 硬编码，其余全过）。而原程序 12 档中 160X120、208X120 两档都是 height=120，
且 settings.ini 显示用户上次用的正是 VWH=3 → 160X120。
→ **结论：ffmpeg 的 amv 编码器不能直接做原程序 1/6 的档位**。
重写方案倾向：ffmpeg 只做解码/缩放 → mjpeg 编码 JPEG → 自研 muxer 拼 AMV 容器
（amvh 高度字段自写，不受 ffmpeg 白名单限制）。

## 3.6 settings.ini 运行参数（用户实机）

`VWH=3`(160X120) `Insertpic=TRUE`(logo 默认开!) `Imagequality=0` `Imagezoom=1` `VFBS=high`
`BMPADJUST=0` `OUTPUTPATH=`(默认) `Overwrite=TRUE`

## 4. 「支持现代格式」的真相

帮助/格式列表是 **2012 年语境**的"现代"。RM/RMVB/VOB/FLV/ASF 等输入依赖**系统 DirectShow 解码器**，
非自带；在 Win11 上这些解码器大多缺失 → 原工具输入能力实际严重退化。重写版改用 FFmpeg 解码，
输入范围反而更广更稳。

## 5. 待验证清单（A/B 对比测试驱动）

- [ ] 原工具产出的 .amv vs ffmpeg 产出的逐字节差异
      （COM 反射已失败：AmvTransforms 运行时 ITypeInfo 指向未注册 typelib，且内嵌 typelib 仅含
      ICountLines/GetFileLines 等遗留接口，疑似非转换入口 → 需用户 GUI 实跑产出，或反汇编 transdll CreateAmvInterface）
- [ ] amvh 56 字节各字段含义（尺寸/fps/码率/芯片型号？）→ 反汇编 `AmvHeaderEncoder`
- [ ] idx1 是否写入、`AMV_END_` 尾部标记、RIFF 长度回填行为
- [ ] 16/22 等非整除帧率下原工具的音频块策略
- [ ] BMPADJUST 0~5 与翻转/旋转的精确映射
- [ ] Insertpic（logo）/ Insertwave（片头音）注入点

## 6. 测试环境

- 本机 ffmpeg：2023-02-27 git 891ed24f77 essentials
- COM：`AmvTransforms`、`AmvTransform.1` 均已注册；`AmvTransform.AmvTransform` 不存在
- Python 3.13.5 + pywin32（COM 反射用）
- 对比工具：`amv-conv inspect`（本仓库 core/amv.py）
