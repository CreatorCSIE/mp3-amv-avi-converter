"""纯 Python AMV 原生编码器 —— 复刻原版工具比特流（M7 实机 P5 验证通过）。

实机标定结论（P5：方向正常 / 画质与原版持平 / 灰底消失）：

- 量化表：T_ORIG_Y/C 提取自 AMV_EncDLL.dll VA 0xe348（**自然序**）。设备解码模型
  pixel = IDCT(q × T)/8 + 128，zigzag 序系数须除以 T[ZIG]。
  证据：AVI 模式原版帧头 DQT0/DQT1 == T_ORIG_Y[ZIG]/T_ORIG_C[ZIG] 逐字节。
- 灰底根因：ffmpeg amv 编码器 DC 表值 13 ≠ 设备真值 8，黑帧 DC=-128 被设备
  解成 50（灰）。用 T_ORIG 真值表从根上消除。
- 哈夫曼表：设备内置标准表（与 AVI 模式原版帧头 DHT 段一致）；用 K.3 之外的
  变体表编码设备解不出。本模块直接从 _ORIG_FRAME_HDR 解析 DHT，单一事实源。
- 帧格式：无头 JPEG —— FFD8 + 熵数据 + FFD9（无 DQT/DHT/SOS），4:2:0，
  8x8 MCU，光栅序，MCU 内 Y0 Y1 Y2 Y3 Cb Cr。
- 方向：原版工具比特流 = vflip(显示内容)（BMP bottom-up quirk），设备按比特流
  直显 → 管线侧必须先对源帧做 vflip 再送入 encode_frame（见 ffmpeg.convert）。
- 容器：RIFF('AMV ')，奇数块**不填充**，尾部 'AMV_END_' 哨兵 + idx1 索引区
  （快进/seek 固件硬依赖，实机验证见 build_file docstring）；头部/音频块由
  ffmpeg amv muxer 产出（与原版工具头部逐字节一致，仅 2 个非 gate 字段差异），
  本模块 build_file 只替换视频负载并回填 RIFF/movi size。
"""

from __future__ import annotations

import ctypes
import shutil
import struct
import subprocess
from collections.abc import Iterable, Sequence
from pathlib import Path

import numpy as np

from .aviwriter import _ORIG_FRAME_HDR

# zigzag 序：自然序下标 -> 熵编码顺序
ZIG = np.array([
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5, 12, 19, 26, 33, 40, 48, 41, 34,
    27, 20, 13, 6, 7, 14, 21, 28, 35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44,
    51, 58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63])

# 设备真实量化表（自然序，EncDLL VA 0xe348 / DecDLL VA 0xb3b0 提取，i16）
T_ORIG_Y = [
    8, 6, 5, 8, 12, 20, 26, 31,
    6, 6, 7, 10, 13, 29, 30, 28,
    7, 7, 8, 12, 20, 29, 35, 28,
    7, 9, 11, 15, 26, 44, 40, 31,
    9, 11, 19, 28, 34, 55, 52, 39,
    12, 18, 28, 32, 41, 52, 57, 46,
    25, 32, 39, 44, 52, 61, 60, 51,
    36, 46, 48, 49, 56, 50, 52, 50,
]
T_ORIG_C = [
    9, 9, 12, 24, 50, 50, 50, 50,
    9, 11, 13, 33, 50, 50, 50, 50,
    12, 13, 28, 50, 50, 50, 50, 50,
    24, 33, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50,
    50, 50, 50, 50, 50, 50, 50, 50,
]

# zigzag 序量化表（熵编码系数按此序除以表值）
TY = np.array(T_ORIG_Y, dtype=np.float64)[ZIG]
TC = np.array(T_ORIG_C, dtype=np.float64)[ZIG]

# 8x8 正交 DCT 矩阵（JPEG 约定，输入已减 128）
_n = np.arange(8)
_DCT_M = np.sqrt(2.0 / 8) * np.cos(np.pi * (_n[None, :] + 0.5) * _n[:, None] / 8)
_DCT_M[0] /= np.sqrt(2)


def build_huff(bits: Sequence[int], vals: Sequence[int]) -> dict[tuple[int, int], int]:
    """J.3 流程：按每长度码数生成 (长度, 码值) -> 符号 表。"""
    code, k, t = 0, 0, {}
    for l in range(1, 17):
        for _ in range(bits[l - 1]):
            t[(l, code)] = vals[k]
            code, k = code + 1, k + 1
        code <<= 1
    return t


def _parse_dht(hdr: bytes) -> dict[int, tuple[list[int], list[int]]]:
    """从原版帧头模板解析 DHT 段：Tc/Td -> (bits[16], vals)。"""
    tabs: dict[int, tuple[list[int], list[int]]] = {}
    i = 2
    while i + 4 <= len(hdr) and hdr[i] == 0xFF:
        m = hdr[i + 1]
        ln = int.from_bytes(hdr[i + 2:i + 4], "big")
        if m == 0xC4:
            body = hdr[i + 4:i + 2 + ln]
            j = 0
            while j < len(body):
                tc = body[j]
                bits = list(body[j + 1:j + 17])
                tot = sum(bits)
                tabs[tc] = (bits, list(body[j + 17:j + 17 + tot]))
                j += 17 + tot
        if m == 0xDA:
            break
        i += 2 + ln
    return tabs


_dht = _parse_dht(_ORIG_FRAME_HDR)


def _codes(tc: int) -> dict[int, tuple[int, int]]:
    t = build_huff(*_dht[tc])
    return {sym: (l, c) for (l, c), sym in t.items()}


# 符号 -> (码长, 码值)
DCY = _codes(0x00)
ACY = _codes(0x10)
DCC = _codes(0x01)
ACC = _codes(0x11)


def _tab(d: dict[int, tuple[int, int]], n: int) -> tuple[np.ndarray, np.ndarray]:
    c = np.zeros(n, np.int32)
    l = np.zeros(n, np.int32)
    for s, (ln, code) in d.items():
        c[s], l[s] = code, ln
    return c, l


_DCY_C, _DCY_L = _tab(DCY, 16)
_ACY_C, _ACY_L = _tab(ACY, 256)
_DCC_C, _DCC_L = _tab(DCC, 16)
_ACC_C, _ACC_L = _tab(ACC, 256)


def _load_fast():
    """加载 _entropy.c 编译的 C 核（缺失时尝试 gcc 现场编译；再失败回退纯 Python）。

    熵编码循环占编码总耗时 ~89%（320x240 实测），C 化后全曲外推从 ~20min 降到秒级。
    """
    import os

    here = Path(__file__).resolve().parent
    src = here / "_entropy.c"
    if not src.is_file():
        return None
    lib = here / ("_entropy.dll" if os.name == "nt" else "_entropy.so")
    if not lib.exists() or lib.stat().st_mtime < src.stat().st_mtime:
        gcc = shutil.which("gcc")
        if not gcc:
            return None
        try:
            subprocess.run([gcc, "-O2", "-shared", "-fPIC", "-o", str(lib), str(src)],
                           check=True, capture_output=True, timeout=120)
        except (OSError, subprocess.SubprocessError):
            return None
    try:
        fn = ctypes.CDLL(str(lib)).amv_entropy
    except OSError:
        return None
    fn.restype = ctypes.c_int
    fn.argtypes = ([ctypes.c_void_p] * 3 + [ctypes.c_int] * 2
                   + [ctypes.c_void_p] * 8 + [ctypes.c_void_p, ctypes.c_int])
    return fn


_FAST = _load_fast()


class _BitWriter:
    __slots__ = ("out", "cur", "n")

    def __init__(self):
        self.out = bytearray()
        self.cur = 0
        self.n = 0

    def put(self, code: int, length: int) -> None:
        for i in range(length - 1, -1, -1):
            self.cur = (self.cur << 1) | ((code >> i) & 1)
            self.n += 1
            if self.n == 8:
                self.out.append(self.cur)
                if self.cur == 0xFF:
                    self.out.append(0x00)  # 字节 stuffing
                self.cur = self.n = 0

    def flush(self) -> None:
        if self.n:
            self.cur = (self.cur << (8 - self.n)) | ((1 << (8 - self.n)) - 1)  # 尾部补 1
            self.out.append(self.cur)
            if self.cur == 0xFF:
                self.out.append(0x00)
            self.cur = self.n = 0


def _bitlen(v: int) -> int:
    return abs(v).bit_length()


def _enc_block(bw: _BitWriter, lv: list[int], dctab, actab, prevdc: int) -> int:
    """编码一个 zigzag 序量化块（64 级），返回本块 DC。"""
    diff = lv[0] - prevdc
    s = _bitlen(diff)
    l, c = dctab[s]
    bw.put(c, l)
    if s:
        v = diff if diff >= 0 else diff + (1 << s) - 1
        bw.put(v, s)
    run = 0
    for i in range(1, 64):
        v = lv[i]
        if v == 0:
            run += 1
            continue
        while run >= 16:
            l, c = actab[0xF0]
            bw.put(c, l)
            run -= 16
        ss = _bitlen(v)
        l, c = actab[(run << 4) | ss]
        bw.put(c, l)
        bw.put(v if v >= 0 else v + (1 << ss) - 1, ss)
        run = 0
    if run:
        l, c = actab[0x00]  # EOB
        bw.put(c, l)
    return lv[0]


def _blocks8(plane: np.ndarray) -> np.ndarray:
    """(H,W) -> (by,bx,8,8)。"""
    h, w = plane.shape
    return plane.reshape(h // 8, 8, w // 8, 8).transpose(0, 2, 1, 3)


def _quant_zig(blk: np.ndarray, table: np.ndarray) -> np.ndarray:
    """批量 DCT + zigzag 重排 + 量化（round-half-away-from-zero，与原版一致）。

    返回必须 C 连续：[..., ZIG] 高级索引可能给出非连续结果，C 核按连续内存读取。
    """
    c = np.einsum("ab,nbc,dc->nad", _DCT_M, blk.reshape(-1, 8, 8), _DCT_M)
    c = c.reshape(blk.shape[0], blk.shape[1], 64)[..., ZIG]
    return np.ascontiguousarray(np.sign(c) * np.floor(np.abs(c) / table + 0.5),
                                dtype=np.int32)


def encode_frame(y: np.ndarray, u: np.ndarray, v: np.ndarray) -> bytes:
    """编码一帧（yuvj420p 平面，uint8，尺寸为 16 的倍数）。

    注意：传入前必须已按显示内容做 vflip（原版比特流 quirk，见模块 docstring）。
    非 16 倍数尺寸补齐到 16 的倍数编码，填充值 = 亮度 0（黑）/ 色度 128（中性）：
    160x120 原工具 AVI 参考比特流终裁（chk160u/x，末 MCU 行 B 结构 corr=1.0、
    tail=0、badDC=0），非边缘复制。设备只裁显有效区，填充仅影响边界块 IDCT。
    返回无头 JPEG：FFD8 + 熵数据 + FFD9。
    """
    h, w = y.shape
    if h % 16 or w % 16:
        ph, pw = -h % 16, -w % 16
        y = np.pad(y, ((0, ph), (0, pw)), constant_values=0)
        u = np.pad(u, ((0, ph // 2), (0, pw // 2)), constant_values=128)
        v = np.pad(v, ((0, ph // 2), (0, pw // 2)), constant_values=128)
        h, w = y.shape
    mcu_y, mcu_x = h // 16, w // 16
    Y = _quant_zig(_blocks8(y.astype(np.float64)) - 128.0, TY)
    U = _quant_zig(_blocks8(u.astype(np.float64)) - 128.0, TC)
    V = _quant_zig(_blocks8(v.astype(np.float64)) - 128.0, TC)
    if _FAST is not None:
        cap = mcu_y * mcu_x * 6 * 64 * 4 + 64  # 每系数最坏 ~26bit，宽裕上界
        buf = np.empty(cap, np.uint8)
        ret = _FAST(Y.ctypes.data, U.ctypes.data, V.ctypes.data, mcu_y, mcu_x,
                    _DCY_C.ctypes.data, _DCY_L.ctypes.data,
                    _ACY_C.ctypes.data, _ACY_L.ctypes.data,
                    _DCC_C.ctypes.data, _DCC_L.ctypes.data,
                    _ACC_C.ctypes.data, _ACC_L.ctypes.data,
                    buf.ctypes.data, cap)
        if ret >= 0:
            return b"\xff\xd8" + buf[:ret].tobytes() + b"\xff\xd9"
    bw = _BitWriter()
    dc = [0, 0, 0]
    for mcy in range(mcu_y):
        for mcx in range(mcu_x):
            for by, bx in ((2 * mcy, 2 * mcx), (2 * mcy, 2 * mcx + 1),
                           (2 * mcy + 1, 2 * mcx), (2 * mcy + 1, 2 * mcx + 1)):
                dc[0] = _enc_block(bw, Y[by, bx].tolist(), DCY, ACY, dc[0])
            dc[1] = _enc_block(bw, U[mcy, mcx].tolist(), DCC, ACC, dc[1])
            dc[2] = _enc_block(bw, V[mcy, mcx].tolist(), DCC, ACC, dc[2])
    bw.flush()
    return b"\xff\xd8" + bytes(bw.out) + b"\xff\xd9"


def split_amv(data: bytes) -> tuple[bytes, list[bytes]]:
    """从 ffmpeg amv muxer 产物提取 (头部区含 'movi' fourcc, 音频块列表)。

    视频块丢弃（由本模块重编码替换）；音频 01wb 块原样保留（376B IMA-AMV）。
    """
    if data[8:12] != b"AMV ":
        raise ValueError("donor 不是 RIFF('AMV ') 容器")
    q = data.find(b"movi")
    if q < 0:
        raise ValueError("donor 无 movi 列表")
    header = data[:q + 4]
    auds: list[bytes] = []
    p = q + 4
    while p + 8 <= len(data):
        fcc = data[p:p + 4]
        sz = struct.unpack_from("<I", data, p + 4)[0]
        if fcc == b"01wb":
            auds.append(data[p + 8:p + 8 + sz])
        elif fcc != b"00dc":
            break  # 'AMV_END_' 哨兵等尾部
        p += 8 + sz  # 实测：奇数块不填充
    return header, auds


def build_file(donor: bytes, frames: Iterable[bytes]) -> bytes:
    """donor 头部/音频 + 重编码视频帧 -> 固件原版布局的完整 AMV 文件字节。

    帧数与音频块数取 min（1:1 交织）；调用方应保证 frames 已按音频块数补齐。
    尾部追加原版 idx1 尾区：'AMV_END_'(8B 裸哨兵) + 'idx1' + size + 16B 记录
    （fourcc+flags 0x10+off+size，off 基准 = 'movi' fourcc 位置）。
    idx1 实效验证（2026-11-24 AVI 三件套 A0/A1/A2 实机）：固件顺序播放走块头
    流式解码，但快进/seek 完全依赖 idx1——删 idx1 快进报格式错误、篡改 idx1
    进度条异常+快进报错。原版两参考文件 size 字段一个 2×bug 一个正确均可播，
    写正确值。
    """
    header, auds = split_amv(donor)
    hdr = bytearray(header)
    q = len(hdr) - 4  # 'movi' fourcc 位置
    movi = bytearray()
    idx = bytearray()
    off = 4
    for f, a in zip(frames, auds):
        movi += b"00dc" + struct.pack("<I", len(f)) + f
        idx += struct.pack("<4sIII", b"00dc", 0x10, off, len(f))
        off += 8 + len(f)
        movi += b"01wb" + struct.pack("<I", len(a)) + a
        idx += struct.pack("<4sIII", b"01wb", 0x10, off, len(a))
        off += 8 + len(a)
    tail = b"AMV_END_" + b"idx1" + struct.pack("<I", len(idx)) + bytes(idx)
    struct.pack_into("<I", hdr, 4, len(hdr) + len(movi) + len(tail) - 8)  # RIFF size
    struct.pack_into("<I", hdr, q - 4, 4 + len(movi))                     # movi LIST size
    return bytes(hdr) + bytes(movi) + tail
