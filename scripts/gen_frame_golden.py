"""帧级金标准提取 —— Phase1 C++ 编码内核逐位比对的测试基准。

文件级语料（gen_golden.py）验证整条管线；本脚本把"raw yuv 帧 → 无头 JPEG 帧"
这一段的输入输出逐帧落盘，让 C++ 内核在容器/编排移植前就能吃逐位反馈。

产物（golden/frames/）：
  <entry>_f<k>.yuv     送入 encode_frame 前的平面（已 vflip、未 pad，y|u|v 连续）
  <entry>_f<k>.bin     期望输出（FFD8 + 熵数据 + FFD9）
  coeff_*.f64/.i32     首帧调试转储：einsum 原始 float64（自然序）、zigzag 后
                       量化 int32 —— 用于确定 C++ 收缩序与 numpy 逐位一致
  tables.f64           DCT_M(64) + TY(64) + TC(64)，小端 double 十六进制语义
  meta.json            条目、尺寸、帧数、文件清单

用法：python scripts/gen_frame_golden.py
"""

from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from amvconv.core import amvenc, ffmpeg as ffmod  # noqa: E402
from amvconv.core.models import ConversionJob  # noqa: E402
from amvconv.core.profiles import Profile  # noqa: E402

# (entry_id, src_id, w, h, fps, 帧数)
ENTRIES = [
    ("grad_160x120_f25", "grad", 160, 120, 25, 4),   # h=120 → pad 到 128，覆盖补齐路径
    ("grad_320x240_f25", "grad", 320, 240, 25, 2),
    ("grad_176x144_f25", "grad", 176, 144, 25, 2),
    ("bars_96x64_f25", "bars", 96, 64, 25, 2),
]


def dump_tables(out_dir: Path) -> None:
    (out_dir / "tables.f64").write_bytes(
        np.concatenate([amvenc._DCT_M.reshape(-1), amvenc.TY, amvenc.TC]).astype("<f8").tobytes())


def read_frames(job: ConversionJob, n: int):
    """复刻 ffmpeg.convert 的取帧段：rawvideo 管道 → vflip 平面（比特流 quirk 前置）。"""
    ff, _ = ffmod.find_binaries()
    cmd = ffmod.build_video_command(job, ff)
    proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    w, h = job.profile.width, job.profile.height
    frame_sz = w * h * 3 // 2
    fy, fu = w * h, w * h // 4
    try:
        for _ in range(n):
            raw = proc.stdout.read(frame_sz)
            if len(raw) < frame_sz:
                break
            y = np.frombuffer(raw[:fy], np.uint8).reshape(h, w)[::-1, :]
            u = np.frombuffer(raw[fy:fy + fu], np.uint8).reshape(h // 2, w // 2)[::-1, :]
            v = np.frombuffer(raw[fy + fu:], np.uint8).reshape(h // 2, w // 2)[::-1, :]
            yield (np.ascontiguousarray(y), np.ascontiguousarray(u), np.ascontiguousarray(v))
    finally:
        proc.stdout.close()
        proc.stderr.close()
        proc.terminate()
        proc.wait()


def dump_coeff_debug(planes, entry: str, out_dir: Path) -> None:
    """首帧 Y 平面：einsum 自然序 float64 + zigzag 量化 int32（收缩序定案依据）。"""
    y, u, v = planes
    y16, *_ = _pad(y, u, v)
    blocks = amvenc._blocks8(y16.astype(np.float64)) - 128.0
    c = np.einsum("ab,nbc,dc->nad", amvenc._DCT_M, blocks.reshape(-1, 8, 8), amvenc._DCT_M)
    (out_dir / f"coeff_{entry}_nat.f64").write_bytes(c.astype("<f8").tobytes())
    q = amvenc._quant_zig(blocks, amvenc.TY)
    (out_dir / f"coeff_{entry}_zig.i32").write_bytes(q.astype("<i4").tobytes())


def _pad(y, u, v):
    h, w = y.shape
    if h % 16 or w % 16:
        ph, pw = -h % 16, -w % 16
        y = np.pad(y, ((0, ph), (0, pw)), constant_values=0)
        u = np.pad(u, ((0, ph // 2), (0, pw // 2)), constant_values=128)
        v = np.pad(v, ((0, ph // 2), (0, pw // 2)), constant_values=128)
    return y, u, v


def main() -> int:
    src_dir = ROOT / "golden" / "src"
    out_dir = ROOT / "golden" / "frames"
    out_dir.mkdir(parents=True, exist_ok=True)
    dump_tables(out_dir)
    meta = {"files": []}
    for entry, sid, w, h, fps, n in ENTRIES:
        src = src_dir / f"{sid}.mkv"
        if not src.exists():
            raise SystemExit(f"缺合成源 {src}，先跑 scripts/gen_golden.py")
        job = ConversionJob(source=src, target=out_dir / "_tmp.amv", profile=Profile(w, h, fps))
        first = None
        for k, planes in enumerate(read_frames(job, n)):
            if first is None:
                first = planes
            y, u, v = planes
            (out_dir / f"{entry}_f{k}.yuv").write_bytes(y.tobytes() + u.tobytes() + v.tobytes())
            (out_dir / f"{entry}_f{k}.bin").write_bytes(amvenc.encode_frame(y, u, v))
            meta["files"].append(f"{entry}_f{k}")
        dump_coeff_debug(first, entry, out_dir)
        meta.setdefault("entries", []).append(
            {"id": entry, "w": w, "h": h, "fps": fps, "frames": n})
        print(f"{entry}: {n} 帧 + coeff 转储完成")
    (out_dir / "meta.json").write_text(json.dumps(meta, ensure_ascii=False, indent=1), encoding="utf-8")
    # C++ 测试工具消费的极简清单（避免在 C++ 里解析 JSON）
    (out_dir / "manifest.txt").write_text(
        "\n".join(f"{e[0]} {e[2]} {e[3]} {e[5]}" for e in ENTRIES) + "\n", encoding="utf-8")
    print(f"-> {out_dir}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
