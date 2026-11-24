# 逆向记录 03 — 原版 AVI 模式解剖（2026-11-22）

样本：`lianqu1980.avi`（原工具 128X128 档导出，源 lianqu1980.mp4 197.3s AAC+H264）
14,086,840B。

## 结论速览

**AMV 与 AVI 共用同一条编解码管线**（裸 JPEG 帧 + IMA ADPCM 22050Hz 单声道），
差异只在容器与帧率策略。"MP3 都能播放"的原因：AVI 模式就是标准 AVI 骨架 +
MJPEG/IMA 这两个 90 年代起就通用的编码。

## 结构（实测）

```
RIFF 'AVI '
  LIST 'hdrl' (296B)
    avih: usPerFrame=46122, totalFrames=4293, streams=2, 128x128, maxBps=262144
    LIST 'strl': strh(vids/MJPG, scale=46122 rate=1e6) + strf(BITMAPINFOHEADER 40B, MJPG)
    LIST 'strl': strh(auds, handler=0, scale=512 rate=11100) + strf(WAVEFORMATEX: 0x11 IMA, 1ch, 22050Hz, 11100B/s)
  LIST 'movi' (13,949,132B)
    00dc(883..3877B JPEG) + 01wb(恒512B) 严格 1:1 交织 ×4293
  idx1 (137,376B = 8586 记录 × 16B，size 字段正确，无 AMV 的 2× bug)
```

## 22fps 之谜 → 21.68fps 音频驱动

AVISIZE 表候选 `14;22;22`，实际产出 **46122µs/帧 = 21.6816fps**：

- 音频块**固定 512B**（IMA 单声道）= `(512-4)×2+1 = 1017 样本`
- 22050 / 1017 = **21.6816** —— 不是 22！
- avih/strh 的帧时长直接取音频块周期：`1e6×512/11100 = 46122`
- 4293 块 × 1017 样本 = 198.00s 音频 ≡ 4293 帧 × 46122µs 视频，零漂移

即 **AVI 模式让音频时钟驱动视频时序**，用固定块长回避非整除——
与 AMV 模式"恒 30fps 保整除"是两种截然不同的对策。
（14 档推测 = 22050/1575 样本块 = 14.001fps，待测）

## AVI1 魔数（固件硬校验点，与 zanzipanzi 逆向笔记互证）

首个 JPEG 帧 @336 = `FF D8 FF E0 00 10 'AVI1' 00×6`——APP0 段写 **AVI1** 而非 JFIF。
ATJ2157 固件校验**文件字节 342..345 == 'AVI1'**，否则静默拒播（ffmpeg 普通 AVI 首帧在 ~10000 处带 JFIF，故拒）。
AMV 模式帧则是裸 `FF D8` + 扫描数据（无 APP0）。重写版 AVI writer 必须复刻此布局。

## 头部 quirk（重写版兼容层需要）

1. **00dc 块头 size 比 idx1/真实 JPEG 少 1~2 字节**（883 vs idx1 的 884；1050 vs 1052）。
   多数播放器靠 JPEG EOI 重同步所以无感，但 ffprobe 直接废（只报 2 帧）。
   → 重写版应写**正确 size**；读原版产物时以 idx1 为准。
2. idx1 偏移基准 = movi 数据区首块头（与 AMV 相同约定）。
3. 无 `AMV_END_`、无 `amvh`——纯标准 AVI，ffmpeg `-c copy` 修 size 即可救活。

## 对重写版的映射规则

| 模式 | 容器 | fps | 音频 | 时序源 |
|---|---|---|---|---|
| AMV | AMV 头 + 裸交织 + 尾部 idx1(2×size bug 兼容读) | 恒最大候选(30) | 22050/fps 样本/块(735) | 视频驱动 |
| AVI | 标准 RIFF AVI | 1e6×512/11100=21.68 | 恒 512B/块(1017 样本) | 音频驱动 |

待补测：AVI 14fps 档（验证 1575 样本块假说）——未测。~~AMV 320X240 档~~
✅ 已实机定案（320x240@25，docs/06；AVI 候选 22 与 AMV 候选 25 不同，印证两套独立表）。
另：AVI 模式比特流为显示序（无 vflip），与 AMV 模式相反（docs/06 160x120 节实锤）。
