"""AVI 模式 writer：ffmpeg(mjpeg+IMA) 产标准 AVI，再重封装为 ATJ2157 固件布局。

实机 A/B 七轮定案（docs/06）——固件播放器的完整硬 gate：

帧级（JPEG 解码路径）：
- 只认标准 JPEG 哈夫曼表：新版 ffmpeg 默认 optimal 表（按帧内容生成退化表）→ 首帧乱码
- 帧头必须是原版结构：APP0(AVI1) → DQT×2 → SOF0(色度 Tq=1) → DHT×4(一表一段，
  顺序 00,10,01,11) → SOS；APP14 前置 / DHT 合并段 / DHT 在 SOF0 前 → 乱码或拒播
  → 生产做法：整段替换为原版帧头模板（_ORIG_FRAME_HDR），保留我们的扫描数据

容器级（demux 路径）：
- 首帧 JPEG SOI 必须落在偏移 336（否则"格式错误"拒播）
- 328B 头部区必须逐字节复刻原版（avih 保留区 [22201,1,0,785]、strh_v.length=2、
  strh_a dwLength=n×258 等 quirk；干净头 + 健康流实测仍被拒——quirk 是 gate，
  此前"quirk 无关"结论系被帧级 bug 掩盖的误判）
- 视频块 payload 零填充到 4 字节对齐；块头 size = 真实 JPEG 长（不含 pad）；
  idx1 size = 对齐后长度。固件按 4 字节对齐消费 DMA，只补到偶数的布局会在
  第一个非 4 对齐帧后错位（黑首帧后跳文件）
- 音频恒 512B IMA 块（1017 样本）→ 音频驱动时序 22050/1017≈21.68fps，usPerFrame=46122
- hdrl 定长 296B；movi 数据区 @328；idx1 偏移基准 = 'movi' fourcc + 4
"""

from __future__ import annotations

import base64
import struct
import subprocess
from pathlib import Path

AUDIO_RATE = 22050
AUDIO_BLOCK = 512          # 字节/音频块
AUDIO_SAMPLES = 1017       # (512-4)*2+1
AUDIO_AVG_BPS = 11100      # 22050/2
US_PER_FRAME = 46122       # round(1e6*1017/22050)
HEADER_SIZE = 336          # movi 首块头 @328 + 8
MOVI_BASE = 324            # idx1 偏移基准（'movi' fourcc + 4）

# 原版容器头部区（RIFF..movi LIST 头，328B，逐字节），动态字段由 remux 原地 patch
_ORIG_HDR = base64.b64decode(
    "UklGRrDy1gBBVkkgTElTVCgBAABoZHJsYXZpaDgAAAAqtAAAAAAEAAAAAAAQAAEAxRAAAAAAAAAC"
    "AAAAAAAEAIAAAACAAAAAuVYAAAEAAAAAAAAAEQMAAExJU1R0AAAAc3RybHN0cmg4AAAAdmlkc01K"
    "UEcAAAAAAAAAAAAAAAAqtAAAQEIPAAAAAAACAAAAAAAAAAAAAAAAAAAAAAAAAIAAgABzdHJmKAAA"
    "ACgAAACAAAAAgAAAAAEAGABNSlBHAMAAAAAAAAAAAAAAAAAAAAAAAABMSVNUYAAAAHN0cmxzdHJo"
    "OAAAAGF1ZHMAAAAAAAAAAAAAAAAAAAAAAAIAAFwrAAAAAAAAiuYQAAIBAAAAAAAAAAIAAAAAAAAA"
    "AAAAc3RyZhQAAAARAAEAIlYAAFwrAAAAAgQAAgD5A0xJU1TM2NQAbW92aQ==")

# 原版首帧 JPEG 头（SOI..SOS 前：APP0 AVI1 + DQT×2 + SOF0 + DHT×4，609B）
_ORIG_FRAME_HDR = base64.b64decode(
    "/9j/4AAQQVZJMQAAAAAAAAAAAAD/2wBDAAgGBgcGBQgHBwcJCQgKDBQNDAsLDBkSEw8UHRofHh0aHBwgJC4nICIsIxw"
    "cKDcpLDAxNDQ0Hyc5PTgyPC4zNDL/2wBDAQkJCQwLDBgNDRgyIRwhMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMjIyMj"
    "IyMjIyMjIyMjIyMjIyMjIyMjIyMjL/wAARCACAAIADASIAAhEBAxEB/8QAHwAAAQUBAQEBAQEAAAAAAAAAAAECAwQFBg"
    "cICQoL/8QAtRAAAgEDAwIEAwUFBAQAAAF9AQIDAAQRBRIhMUEGE1FhByJxFDKBkaEII0KxwRVS0fAkM2JyggkKFhcYG"
    "RolJicoKSo0NTY3ODk6Q0RFRkdISUpTVFVWV1hZWmNkZWZnaGlqc3R1dnd4eXqDhIWGh4iJipKTlJWWl5iZmqKjpKWm"
    "p6ipqrKztLW2t7i5usLDxMXGx8jJytLT1NXW19jZ2uHi4+Tl5ufo6erx8vP09fb3+Pn6/8QAHwEAAwEBAQEBAQEBAQAA"
    "AAAAAAECAwQFBgcICQoL/8QAtREAAgECBAQDBAcFBAQAAQJ3AAECAxEEBSExBhJBUQdhcRMiMoEIFEKRobHBCSMzUvAV"
    "YnLRChYkNOEl8RcYGRomJygpKjU2Nzg5OkNERUZHSElKU1RVVldYWVpjZGVmZ2hpanN0dXZ3eHl6goOEhYaHiImKkpOU"
    "lZaXmJmaoqOkpaanqKmqsrO0tba3uLm6wsPExcbHyMnK0tPU1dbX2Nna4uPk5ebn6Onq8vP09fb3+Pn6")

_SOF0_POS = _ORIG_FRAME_HDR.index(b"\xff\xc0")


def build_ffmpeg_command(source: Path, tmp: Path, width: int, height: int,
                         ffmpeg: str = "ffmpeg", extra_video_args=(), extra_audio_args=()) -> list[str]:
    """ffmpeg 侧：mjpeg + adpcm_ima_wav，1:1 交织，帧率对齐音频块周期。"""
    return [
        ffmpeg, "-hide_banner", "-y", "-i", str(source),
        "-r", f"{AUDIO_RATE}/{AUDIO_SAMPLES}",
        "-vf", f"scale={width}:{height},format=yuvj420p",
        # 原版工具硬拉伸填满（实测 T3ORIG 与拉伸源相关性远高于加边），不保比例、不加黑边
        "-c:v", "mjpeg", "-q:v", "4",
        # -huffman default：扫描数据必须按标准表编码（与 _ORIG_FRAME_HDR 注入的表一致）；
        # 新版默认 optimal 会按帧内容生成退化表，固件硬解不认（docs/06 实机 A/B）
        "-huffman", "default", *extra_video_args,
        # AVI muxer 拒绝 adpcm_ima_amv（AMV 专用）；adpcm_ima_wav 即 tag 0x11，块须 2 的幂
        "-c:a", "adpcm_ima_wav", "-ar", str(AUDIO_RATE), "-ac", "1",
        "-block_size", str(AUDIO_BLOCK), *extra_audio_args,
        "-f", "avi", str(tmp),
    ]


def _packets(data: bytes):
    """从 ffmpeg AVI 的 movi 中提取 (fourcc, payload) 序列。"""
    i = data.find(b"movi")
    if i < 0:
        raise ValueError("输入 AVI 无 movi 列表")
    p, end = i + 4, len(data)
    while p + 8 <= end:
        cid, sz = data[p:p + 4], struct.unpack_from("<I", data, p + 4)[0]
        if cid in (b"idx1", b"JUNK"):
            break
        if cid[:2] in (b"00", b"01") and sz <= end - p - 8:
            yield cid, data[p + 8:p + 8 + sz]
        p += 8 + sz + (sz & 1)


_APP0 = b"\xff\xd8\xff\xe0\x00\x10"
_AVI1_PAYLOAD = b"AVI1" + bytes(10)  # APP0 payload 共 14B（len 字段 0x0010 含自身 2B）


def _patch_app0(jpeg: bytes) -> bytes:
    """APP0(JFIF) → APP0(AVI1)，长度不变；非 APP0 开头原样返回；模板帧本就是 AVI1，无损。"""
    if jpeg.startswith(_APP0):
        return _APP0 + _AVI1_PAYLOAD + jpeg[20:]
    return jpeg


def _frame_header(width: int, height: int) -> bytes:
    hdr = bytearray(_ORIG_FRAME_HDR)
    struct.pack_into(">HH", hdr, _SOF0_POS + 5, height, width)
    return bytes(hdr)


def fix_jpeg_layout(jpeg: bytes, width: int = 128, height: int = 128) -> bytes:
    """现代化 ffmpeg 帧 → 原版帧：整段替换 SOI..SOS 前头部为原版模板，保留扫描数据。

    固件硬解只认原版表值/段结构（DQT×2、色度 Tq=1、DHT 一表一段且按 00,10,01,11 排列、
    无 APP14）。扫描数据由 -huffman default 按同一套标准表编码，选择子一致，可直接复用。
    量化表值与 ffmpeg 编码矩阵的细微差异只影响像素值（观感正常），不影响可解码性。
    """
    if jpeg[:2] != b"\xff\xd8":
        return jpeg
    p = 2
    while p + 4 <= len(jpeg) and jpeg[p] == 0xFF:
        m = jpeg[p + 1]
        if m == 0xDA:
            return _frame_header(width, height) + jpeg[p:]
        ln = int.from_bytes(jpeg[p + 2:p + 4], "big")
        p += 2 + ln
    return jpeg


def remux(raw: bytes, width: int, height: int, frame_filter=None) -> bytes:
    """把 ffmpeg 标准 AVI 重封装为固件原版布局（头部/帧头模板 + 4B 对齐 + idx1）。

    frame_filter 默认按输出尺寸注入原版帧头（fix_jpeg_layout）；传自定义单参函数可覆盖。
    """
    if frame_filter is None:
        frame_filter = lambda j: fix_jpeg_layout(j, width, height)
    vids = [_patch_app0(frame_filter(p)) for c, p in _packets(raw) if c == b"00dc"]
    auds = [p for c, p in _packets(raw) if c == b"01wb"]
    if not vids or not auds:
        raise ValueError("ffmpeg 输出缺少 00dc/01wb 包")
    n = min(len(vids), len(auds))  # 严格 1:1；尾部多出的音频块丢弃
    vids, auds = vids[:n], auds[:n]

    # movi：视频 payload 零填充到 4B 对齐，块头 size = 真实 JPEG 长
    movi_parts, movi_sz, idx, off = [], 0, [], 4
    for v, a in zip(vids, auds):
        vp = len(v) + (-len(v)) % 4
        movi_parts.append(b"00dc" + struct.pack("<I", len(v)) + v + bytes(vp - len(v)))
        idx.append(struct.pack("<4sIII", b"00dc", 0x10, off, vp))
        movi_sz += 8 + vp
        off += 8 + vp
        movi_parts.append(b"01wb" + struct.pack("<I", len(a)) + a)
        idx.append(struct.pack("<4sIII", b"01wb", 0x10, off, len(a)))
        movi_sz += 8 + len(a)
        off += 8 + len(a)

    movi = b"".join(movi_parts)  # movi LIST 头（12B）已含于 _ORIG_HDR 尾部，size 在下方 patch
    idx1 = b"idx1" + struct.pack("<I", 16 * len(idx)) + b"".join(idx)

    hdr = bytearray(_ORIG_HDR)
    struct.pack_into("<I", hdr, hdr.index(b"avih") + 8 + 4, n)        # dwTotalFrames
    struct.pack_into("<I", hdr, hdr.index(b"auds") + 32, n * 258)     # 原版 dwLength 约定
    struct.pack_into("<I", hdr, hdr.rindex(b"movi") - 4, 4 + movi_sz)  # movi LIST size
    out = bytes(hdr) + movi + idx1
    return b"RIFF" + struct.pack("<I", len(out) - 8) + out[8:]


def convert_avi(source: Path, target: Path, width: int, height: int,
                ffmpeg: str = "ffmpeg", tmp: Path | None = None) -> subprocess.CompletedProcess:
    """端到端：ffmpeg → 临时 AVI → 固件布局重封装。"""
    tmp = tmp or target.with_suffix(".raw.avi")
    cmd = build_ffmpeg_command(source, tmp, width, height, ffmpeg)
    proc = subprocess.run(cmd, capture_output=True, text=True)
    try:
        if proc.returncode == 0:
            target.write_bytes(remux(tmp.read_bytes(), width, height))
    finally:
        tmp.unlink(missing_ok=True)
    return proc
