"""AMV 容器解析器 —— 用于字节级对比「原工具输出」与「ffmpeg 输出」。

实测结构（ffmpeg 2023-02 输出，%TEMP%\\amvtest\\test.amv 96x64@15）：
  - RIFF('AMV ') 及所有 LIST 的 size 字段都写 0（不回填），LIST 类型字段除首个
    'hdrl' 外也写 0 —— 因此只能**平铺扫描**，不能按 size 递归。
  - 叶子块（amvh/strh/strf/00dc/01wb）的 size 是真实的。
  - 奇数大小的块**不写填充字节**，下一块头紧跟 8+size 处（与标准 RIFF 不同！）。
  - 文件尾有字面量 'AMV_END_' 8 字节。
  - amvh 56 字节：+32=宽, +36=高, +40=fps（其余字段待与原版对比确认）。
  - 视频帧 00dc：JPEG 裸扫描数据（FFD8 起、FFD9 止，无 DQT/DHT/SOS）。
  - 音频块 01wb 与视频帧 1:1 交织。
"""

from __future__ import annotations

from collections import Counter
from pathlib import Path

CONTAINERS = {"RIFF", "LIST"}


def _le32(d: bytes, o: int) -> int:
    return int.from_bytes(d[o:o + 4], "little")


def walk_flat(data: bytes):
    """平铺扫描：产出 (fourcc, payload_off, size)；容器产出 (fourcc, None, None)。"""
    off = 12  # 跳过 RIFF + len + 'AMV '
    while off + 8 <= len(data):
        fcc = data[off:off + 4]
        if not all(32 <= b <= 126 for b in fcc):
            break  # 进入非结构区
        size = _le32(data, off + 4)
        name = fcc.decode("latin1")
        if name in CONTAINERS:
            yield name, None, None
            off += 12  # 容器 size 不可信，跳过 4cc+len+type
            continue
        if size > len(data) - off:
            break
        yield name, off + 8, size
        off += 8 + size  # 实测：无奇数填充


def parse(data: bytes) -> dict:
    rep: dict = {
        "size": len(data),
        "form": data[8:12].decode("latin1", errors="replace"),
        "riff_len_field": _le32(data, 4),
        "chunks": [],
        "amvh_hex": "",
        "amvh_fields": {},
        "audio_sizes": [],
        "video_sizes": [],
        "has_idx1": False,
        "has_amv_end": data.endswith(b"AMV_END_"),
        "first_audio_header": "",
        "jpeg_ffd8_count": data.count(b"\xff\xd8"),
        "jpeg_ffd9_count": data.count(b"\xff\xd9"),
    }
    if data[:4] != b"RIFF":
        rep["error"] = "不是 RIFF 文件"
        return rep

    leaves: list[str] = []
    for name, poff, size in walk_flat(data):
        rep["chunks"].append(name)
        if name in CONTAINERS:
            continue
        leaves.append(name)
        if name == "amvh" and size >= 56:
            rep["amvh_hex"] = data[poff:poff + size].hex()
            rep["amvh_fields"] = {
                "dword0": _le32(data, poff),
                "width": _le32(data, poff + 32),
                "height": _le32(data, poff + 36),
                "fps": _le32(data, poff + 40),
                "tail_dwords": [_le32(data, poff + k) for k in (44, 48, 52)],
            }
        elif name == "01wb":
            rep["audio_sizes"].append(size)
            if len(rep["audio_sizes"]) == 1:
                rep["first_audio_header"] = data[poff:poff + 24].hex()
        elif name == "00dc":
            rep["video_sizes"].append(size)
        elif name == "idx1":
            rep["has_idx1"] = True

    rep["video_frames"] = len(rep["video_sizes"])
    rep["audio_blocks"] = len(rep["audio_sizes"])
    rep["audio_block_sizes_hist"] = dict(Counter(rep["audio_sizes"]))
    # 交织节奏：v=00dc a=01wb，其余用方括号标出
    rep["interleave_head"] = "".join(
        "v" if c == "00dc" else ("a" if c == "01wb" else f"[{c}]") for c in leaves
    )[:48]
    return rep


def inspect_file(path: Path) -> dict:
    return parse(Path(path).read_bytes())
