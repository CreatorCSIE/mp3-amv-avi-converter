"""AVI writer 重封装布局测试（合成包，不依赖 ffmpeg）。"""
import struct

from amvconv.core.aviwriter import (HEADER_SIZE, MOVI_BASE, _ORIG_FRAME_HDR,
                                    _ORIG_HDR, fix_jpeg_layout, remux)


def _fake_ffmpeg_avi(frames):
    """构造最小 ffmpeg 风格 AVI：RIFF/AVI + LIST/hdrl(任意) + LIST/movi + idx1。"""
    movi = b""
    for jpeg, audio in frames:
        movi += b"00dc" + struct.pack("<I", len(jpeg)) + jpeg + (len(jpeg) & 1) * b"\0"
        movi += b"01wb" + struct.pack("<I", len(audio)) + audio + (len(audio) & 1) * b"\0"
    hdrl = b"LIST" + struct.pack("<I", 4 + 8) + b"hdrl" + b"junk" + struct.pack("<I", 0)
    body = b"AVI " + hdrl + b"LIST" + struct.pack("<I", 4 + len(movi)) + b"movi" + movi
    return b"RIFF" + struct.pack("<I", len(body)) + body + b"idx1" + struct.pack("<I", 0)


_JFIF0 = b"\xff\xd8\xff\xe0\x00\x10JFIF\x00\x01\x02\x00\x00\x01\x00\x01\x00\x00"
_SOMETHING = b"\xff\xdb\x00\x05\x00\x01" + b"\x00" * 20 + b"\xff\xd9"


def test_layout():
    frames = [(_JFIF0 + _SOMETHING, b"\x00" * 512),
              (_JFIF0 + _SOMETHING, b"\x00" * 512),
              (_JFIF0 + _SOMETHING, b"\x00" * 512)]
    out = remux(_fake_ffmpeg_avi(frames), 128, 128)
    assert out[:4] == b"RIFF" and out[8:12] == b"AVI "
    # 首帧 SOI 必须精确落在 336，APP0 标识为 AVI1
    assert out.find(b"\xff\xd8") == HEADER_SIZE
    assert out[342:346] == b"AVI1"
    assert b"JFIF" not in out
    # movi 首块头 @328
    assert out[328:332] == b"00dc"
    # 头部区 = 原版 328B 模板，仅 3 个动态字段被 patch（totalFrames/dwLength/movi size）
    expect = bytearray(_ORIG_HDR)
    movi_sz = 3 * (8 + 48 + 8 + 512)  # 假帧 48B 已 4 对齐
    struct.pack_into("<I", expect, 36, 3)
    struct.pack_into("<I", expect, 264, 3 * 258)
    struct.pack_into("<I", expect, 320, 4 + movi_sz)
    assert out[12:328] == bytes(expect[12:328])
    # 原版 quirk 在位
    assert struct.unpack_from("<I", out, 72)[0] == 22201
    assert struct.unpack_from("<H", out, 140)[0] == 2
    # idx1：6 条记录，偏移基准 = movi fourcc+4
    i = out.rfind(b"idx1")
    n = struct.unpack_from("<I", out, i + 4)[0] // 16
    assert n == 6
    rec0 = struct.unpack_from("<4sIII", out, i + 8)
    assert rec0[0] == b"00dc" and rec0[2] == 4 and rec0[3] == len(_JFIF0 + _SOMETHING)
    # 第二条 = 音频 512B
    rec1 = struct.unpack_from("<4sIII", out, i + 8 + 16)
    assert rec1[0] == b"01wb" and rec1[3] == 512
    # 偏移自洽：每条记录指向正确块头；视频块头 = 真实长，idx1 = 4B 对齐长
    for k in range(n):
        ck, _, off, ln = struct.unpack_from("<4sIII", out, i + 8 + 16 * k)
        p = MOVI_BASE + off
        assert out[p:p + 4] == ck
        hdr_sz = struct.unpack_from("<I", out, p + 4)[0]
        if ck == b"00dc":
            assert ln % 4 == 0 and hdr_sz <= ln < hdr_sz + 4
        else:
            assert hdr_sz == ln
        assert p % 4 == 0  # 原版布局：所有块头 4 字节对齐
    # RIFF size 字段正确
    assert struct.unpack_from("<I", out, 4)[0] == len(out) - 8


def _jpeg(*markers):
    """按 (marker, body) 序列拼 JPEG（body 不含 marker 与长度字段），SOS 的 body 含扫描数据。"""
    out = b"\xff\xd8"
    for m, body in markers:
        out += bytes((0xFF, m)) + struct.pack(">H", len(body) + 2) + body
    return out


def _marker_seq(jpeg):
    res, p = [], 2
    while p + 2 <= len(jpeg) and jpeg[p] == 0xFF:
        m = jpeg[p + 1]
        res.append(m)
        if m == 0xDA:
            break
        ln = int.from_bytes(jpeg[p + 2:p + 4], "big")
        p += 2 + ln
    return res


_APP0_BODY = b"JFIF\x00\x01\x02\x00\x00\x01\x00\x01\x00\x00"
_APP14_BODY = b"Lavc6100\x00" + b"\x00" * 7
_DQT_BODY = b"\x00" + bytes(range(1, 65))
_DHT_BODY = bytes([0x00]) + bytes([1] + [0] * 14 + [1]) + b"\x01"
_SOF0_BODY = b"\x08" + struct.pack(">HH", 96, 128) + b"\x03\x01\x22\x00\x02\x11\x00\x03\x11\x00"

# 新版 ffmpeg 风格帧：APP14 前置 + DHT 在 SOF0 之前（实机 A/B：固件首帧乱码/拒播）
_MODERN = _jpeg(
    (0xE0, _APP0_BODY), (0xFE, _APP14_BODY), (0xDB, _DQT_BODY),
    (0xC4, _DHT_BODY), (0xC4, bytes([0x10]) + _DHT_BODY[1:]), (0xC0, _SOF0_BODY),
    (0xDA, b"\x01\x00\x00\x3f\x00\x00\x11\x22\xff\xd9"))
_SCAN = _MODERN[_MODERN.index(b"\xff\xda"):]


def test_fix_jpeg_layout_injects_orig_header():
    fixed = fix_jpeg_layout(_MODERN, 128, 96)
    # 输出 = 原版帧头模板（SOF0 尺寸按参数 patch）+ 我们的扫描数据，逐字节
    expect = bytearray(_ORIG_FRAME_HDR)
    q = expect.index(b"\xff\xc0")
    struct.pack_into(">HH", expect, q + 5, 96, 128)
    assert fixed == bytes(expect) + _SCAN
    assert 0xFE not in _marker_seq(bytes(expect[:q]))  # 无 APP14
    # 色度 Tq=1（原版约定）
    assert fixed[q + 18] == 1
    # 幂等
    assert fix_jpeg_layout(fixed, 128, 96) == fixed
    # 不可解析的帧原样返回
    passthrough = _JFIF0 + _SOMETHING
    assert fix_jpeg_layout(passthrough) == passthrough


def test_fix_replaces_multi_table_dht():
    # 新版 ffmpeg huffman=default 的合并 DHT 段写法：头整段被模板替换后不复存在
    t1 = bytes([0x00]) + bytes([1] + [0] * 14 + [1]) + b"\x0a\x0b"
    t2 = bytes([0x10]) + bytes([1] + [0] * 14 + [1]) + b"\x0c" * 2
    merged = _jpeg((0xE0, _APP0_BODY), (0xDB, _DQT_BODY), (0xC4, t1 + t2),
                   (0xC0, _SOF0_BODY), (0xDA, b"\x01\x00\x00\x3f\x00\x00\x11\x22\xff\xd9"))
    fixed = fix_jpeg_layout(merged, 128, 128)
    assert fixed.startswith(_ORIG_FRAME_HDR)  # 合并 DHT 头被模板整体替换
    assert _ORIG_FRAME_HDR.count(b"\xff\xc4") == 4  # 一表一段
    assert merged[merged.index(b"\xff\xda"):] in fixed


def test_remux_applies_orig_conventions_by_default():
    frames = [(_MODERN, b"\x00" * 512)]
    out = remux(_fake_ffmpeg_avi(frames), 128, 96)
    chunk = out[HEADER_SIZE:]
    assert chunk.startswith(_ORIG_FRAME_HDR[:158])  # 模板注入（SOF0 前部分）
    assert out[342:346] == b"AVI1"
    assert b"Lavc" not in out
    # 头部 quirk 复刻：avih 保留区、strh_a dwLength=n×258
    ai = out.index(b"avih")
    assert struct.unpack_from("<I", out, ai + 8 + 40)[0] == 22201
    assert struct.unpack_from("<I", out, out.index(b"auds") + 32)[0] == 258
    # 4B 对齐布局：idx1 视频 size 为 4 的倍数，块头 size 为真实长
    i = out.rfind(b"idx1")
    ck, _, off, ln = struct.unpack_from("<4sIII", out, i + 8)
    assert ln % 4 == 0
    hdr_sz = struct.unpack_from("<I", out, MOVI_BASE + off + 4)[0]
    assert hdr_sz <= ln < hdr_sz + 4


def test_remux_custom_frame_filter():
    out = remux(_fake_ffmpeg_avi([(_MODERN, b"\x00" * 512)]), 128, 128,
                frame_filter=lambda j: j)
    assert b"Lavc" in out  # 显式覆盖默认修复时不被注入
