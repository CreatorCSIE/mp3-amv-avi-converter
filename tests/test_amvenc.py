"""纯 Python AMV 编码器回归测试（不依赖 ffmpeg）。

基准来自实机定案：ORIG(lianqu1980-128x128.amv) 首帧逐字节（P5 实机验证同源）；
DQT 配对结论 T_ORIG(自然序)[ZIG] == 原版帧头 DQT 段。
"""
import struct
from pathlib import Path

import numpy as np
import pytest

from amvconv.core.amvenc import (T_ORIG_C, T_ORIG_Y, ZIG, build_file,
                                 encode_frame, split_amv)
from amvconv.core import amvenc
from amvconv.core.aviwriter import _ORIG_FRAME_HDR

_ORIG = Path(__file__).resolve().parents[1] / "lianqu1980-128x128.amv"


def _orig_frame0() -> bytes:
    d = _ORIG.read_bytes()
    q = d.find(b"movi") + 4
    sz = struct.unpack_from("<I", d, q + 4)[0]
    return d[q + 8:q + 8 + sz]


def _dqt_segments(hdr: bytes) -> list[bytes]:
    out, i = [], 2
    while i + 4 <= len(hdr) and hdr[i] == 0xFF:
        m = hdr[i + 1]
        ln = int.from_bytes(hdr[i + 2:i + 4], "big")
        if m == 0xDB:
            out.append(hdr[i + 4:i + 2 + ln])
        if m == 0xDA:
            break
        i += 2 + ln
    return out


def test_dqt_pairing_natural_order():
    """定案：T_ORIG 列表是自然序，DQT0/DQT1 == T_ORIG_Y/C[ZIG] 逐字节。"""
    segs = _dqt_segments(_ORIG_FRAME_HDR)
    assert len(segs) == 2
    assert segs[0][0] == 0x00 and segs[1][0] == 0x01  # Tq 亮度/色度
    assert segs[0][1:] == bytes(np.array(T_ORIG_Y, np.uint8)[ZIG])
    assert segs[1][1:] == bytes(np.array(T_ORIG_C, np.uint8)[ZIG])


@pytest.mark.skipif(not _ORIG.is_file(), reason="需要原版基准文件 lianqu1980-128x128.amv")
def test_encode_frame_black_byte_exact_vs_orig():
    """纯黑帧（yuvj420p: Y=0, U=V=128）必须与 ORIG 首帧逐字节一致。"""
    y = np.zeros((128, 128), np.uint8)
    u = v = np.full((64, 64), 128, np.uint8)
    assert encode_frame(y, u, v) == _orig_frame0()


def test_encode_frame_headerless():
    """无头帧：FFD8 后直接熵数据，帧内不得出现 DQT/DHT/SOS/SOF 段。"""
    rng = np.random.default_rng(7)
    f = encode_frame(rng.integers(0, 256, (128, 128), dtype=np.uint8),
                     rng.integers(0, 256, (64, 64), dtype=np.uint8),
                     rng.integers(0, 256, (64, 64), dtype=np.uint8))
    assert f[:2] == b"\xff\xd8" and f[-2:] == b"\xff\xd9"
    for m in (b"\xff\xdb", b"\xff\xc4", b"\xff\xda", b"\xff\xc0"):
        assert m not in f


def _python_encode(y, u, v):
    """encode_frame 的纯 Python 熵编码路径（绕过 C 核），作逐字节对照。"""
    h, w = y.shape
    mcu_y, mcu_x = h // 16, w // 16
    Y = amvenc._quant_zig(amvenc._blocks8(y.astype(np.float64)) - 128.0, amvenc.TY)
    U = amvenc._quant_zig(amvenc._blocks8(u.astype(np.float64)) - 128.0, amvenc.TC)
    V = amvenc._quant_zig(amvenc._blocks8(v.astype(np.float64)) - 128.0, amvenc.TC)
    bw = amvenc._BitWriter()
    dc = [0, 0, 0]
    for mcy in range(mcu_y):
        for mcx in range(mcu_x):
            for by, bx in ((2 * mcy, 2 * mcx), (2 * mcy, 2 * mcx + 1),
                           (2 * mcy + 1, 2 * mcx), (2 * mcy + 1, 2 * mcx + 1)):
                dc[0] = amvenc._enc_block(bw, Y[by, bx].tolist(), amvenc.DCY, amvenc.ACY, dc[0])
            dc[1] = amvenc._enc_block(bw, U[mcy, mcx].tolist(), amvenc.DCC, amvenc.ACC, dc[1])
            dc[2] = amvenc._enc_block(bw, V[mcy, mcx].tolist(), amvenc.DCC, amvenc.ACC, dc[2])
    bw.flush()
    return b"\xff\xd8" + bytes(bw.out) + b"\xff\xd9"


@pytest.mark.skipif(amvenc._FAST is None, reason="C 核未编译（无 gcc/_entropy.c）")
@pytest.mark.parametrize("seed", [1, 2, 3])
def test_c_core_matches_python_byte_exact(seed):
    """C 熵编码核必须与纯 Python 路径逐字节一致（含 DC 负值、EOB、stuffing 路径）。"""
    rng = np.random.default_rng(seed)
    y = rng.integers(0, 256, (128, 128), dtype=np.uint8)
    u = rng.integers(0, 256, (64, 64), dtype=np.uint8)
    v = rng.integers(0, 256, (64, 64), dtype=np.uint8)
    assert encode_frame(y, u, v) == _python_encode(y, u, v)
    # 平滑帧（大量 EOB）
    yy = np.tile(np.linspace(0, 255, 128, dtype=np.uint8), (128, 1))
    uu = np.full((64, 64), 128, np.uint8)
    assert encode_frame(yy, uu, uu) == _python_encode(yy, uu, uu)


@pytest.mark.skipif(amvenc._FAST is None, reason="C 核未编译")
def test_quant_output_is_c_contiguous():
    """C 核按连续内存读取：[..., ZIG] 高级索引后必须强制 C 连续（曾静默回退丢 7x 提速）。"""
    y = np.zeros((128, 128), np.uint8)
    Y = amvenc._quant_zig(amvenc._blocks8(y.astype(np.float64)) - 128.0, amvenc.TY)
    assert Y.flags.c_contiguous and Y.dtype == np.int32


def _fake_donor(blocks: list[tuple[bytes, bytes]]) -> bytes:
    """最小 RIFF('AMV ') donor：头部区 + 'movi' + 交织块（奇数不填充，同实测）。"""
    hdr = b"RIFF" + struct.pack("<I", 0) + b"AMV " + b"LIST" + struct.pack("<I", 0) + b"hdrl"
    body = b"".join(b"00dc" + struct.pack("<I", len(v)) + v + b"01wb" + struct.pack("<I", len(a)) + a
                    for v, a in blocks)
    return hdr + b"LIST" + struct.pack("<I", 0) + b"movi" + body + b"AMV_END_"


def test_split_and_build_file_layout():
    donor = _fake_donor([(b"\xff\xd8junk\xff\xd9", b"A" * 376),
                         (b"\xff\xd8x\xff\xd9", b"B" * 375)])  # 奇数音频块
    header, auds = split_amv(donor)
    assert auds == [b"A" * 376, b"B" * 375]
    assert header[8:12] == b"AMV " and header[-4:] == b"movi"

    out = build_file(donor, [b"\xff\xd8f1\xff\xd9", b"\xff\xd8longer2\xff\xd9", b"extra"])
    q = out.find(b"movi")
    # 块序列：v a v a（zip 截断到 donor 音频块数；多余帧丢弃）；奇数负载不填充
    p = q + 4
    seq = []
    while p + 8 <= len(out):
        fcc = out[p:p + 4]
        if fcc not in (b"00dc", b"01wb"):
            break
        sz = struct.unpack_from("<I", out, p + 4)[0]
        seq.append((fcc, sz))
        p += 8 + sz
    assert seq == [(b"00dc", 6), (b"01wb", 376), (b"00dc", 11), (b"01wb", 375)]
    # size 字段回填真实值（RIFF 含尾区，movi 仅数据区）
    assert struct.unpack_from("<I", out, 4)[0] == len(out) - 8
    assert struct.unpack_from("<I", out, q - 4)[0] == 4 + (p - q - 4)
    # idx1 尾区：'AMV_END_' 裸哨兵 + idx1 chunk，记录 16B，off 基准 = 'movi' fourcc
    tail = out[p:]
    assert tail[:8] == b"AMV_END_" and tail[8:12] == b"idx1"
    nrec = struct.unpack_from("<I", tail, 12)[0] // 16
    assert 16 + 16 * nrec == len(tail)  # size 字段 = 实际记录字节数（非原版 2× bug）
    recs = [struct.unpack_from("<4sIII", tail, 16 + 16 * k) for k in range(nrec)]
    assert [(fcc, fl) for fcc, fl, _, _ in recs] == \
        [(b"00dc", 0x10), (b"01wb", 0x10), (b"00dc", 0x10), (b"01wb", 0x10)]
    for fcc, _fl, off, sz in recs:  # 自洽：每条记录指向正确块头
        assert struct.unpack_from("<I", out, q + off + 4)[0] == sz
        assert out[q + off:q + off + 4] == fcc
    assert recs[0][2] == 4  # 首块 off = 4（同原版实测）
