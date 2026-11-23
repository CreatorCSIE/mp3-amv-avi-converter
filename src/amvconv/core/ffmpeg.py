"""FFmpeg 子进程后端（用户已确认：CLI 子进程方式，不嵌库）。

视频编码不走 ffmpeg（其 amv/mjpeg 编码器表值与设备真值不符 → 灰底/画质差），
只借 ffmpeg 两件事：
  1. amv muxer 产容器头部 + adpcm_ima_amv 音频块（donor，视频负载丢弃）
  2. rawvideo 管道取缩放后的 yuvj420p 帧 → amvenc 纯 Python 编码（实机 P5 定案）
"""

from __future__ import annotations

import json
import shutil
import subprocess
from pathlib import Path

import numpy as np

from . import amvenc
from .models import ConversionJob, ConversionResult
from .profiles import AMV_AUDIO_RATE


class FfmpegNotFoundError(RuntimeError):
    pass


def find_binaries() -> tuple[str, str]:
    ff = shutil.which("ffmpeg")
    fp = shutil.which("ffprobe")
    if not ff or not fp:
        raise FfmpegNotFoundError("PATH 中未找到 ffmpeg / ffprobe")
    return ff, fp


def probe(path: Path) -> dict:
    """返回 {streams:[...], format:{...}}，供 UI 显示源信息与时长估算。"""
    _, fp = find_binaries()
    out = subprocess.run(
        [fp, "-hide_banner", "-print_format", "json", "-show_format", "-show_streams", str(path)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    out.check_returncode()
    return json.loads(out.stdout)


def build_donor_command(job: ConversionJob, tmp: Path, ff: str) -> list[str]:
    """donor 通道：黑帧 lavfi 假视频（仅定 amvh 宽高/帧率，编码开销可忽略）+ 真音频。

    amv muxer 硬要求恰好 2 条流；音频块（376B IMA-AMV @22050）与头部区
    与原版工具逐字节一致（仅 amvh+12 标志位与 movi LIST size 两个非 gate 字段差异）。
    """
    p = job.profile
    cmd = [
        ff, "-hide_banner", "-v", "error", "-y",
        "-i", str(job.source),
        "-f", "lavfi", "-i", f"color=c=black:s={p.width}x{p.height}:r={p.fps}",
        "-map", "1:v", "-map", "0:a",
        "-c:v", "amv", "-c:a", "adpcm_ima_amv",
        "-ar", str(AMV_AUDIO_RATE), "-ac", "1",
        "-block_size", str(p.audio_block_samples),
        "-shortest",
        "-f", "amv",
    ]
    if p.height % 16 != 0:
        # amv 编码器默认拒绝非 16 倍数高度；固件实测接受 160x120/176x144，需放行
        cmd += ["-strict", "-1"]
    cmd += [*job.extra_audio_args, str(tmp)]
    return cmd


def build_video_command(job: ConversionJob, ff: str) -> list[str]:
    """视频通道：解码 + 硬拉伸到目标分辨率（原版工具不保比例不加边）+ fps 对齐 + yuvj420p。"""
    p = job.profile
    filters = [f"scale={p.width}:{p.height}"]
    if job.flip:
        filters.append(f"transpose={job.flip}")  # BMPADJUST 用户选项，先于管线 vflip
    filters += [f"fps={p.fps}", "format=yuvj420p"]
    return [
        ff, "-hide_banner", "-v", "error",
        "-i", str(job.source),
        "-vf", ",".join(filters),
        *job.extra_video_args,
        "-f", "rawvideo", "-",
    ]


def _read_exact(stream, size: int) -> bytes:
    buf = bytearray()
    while len(buf) < size:
        chunk = stream.read(size - len(buf))
        if not chunk:
            break
        buf += chunk
    return bytes(buf)


def convert(job: ConversionJob) -> ConversionResult:
    ff, _ = find_binaries()
    p = job.profile
    if p.audio_block_samples is None:
        return ConversionResult(ok=False, command=[], returncode=-1,
                                stderr_tail=f"{p.fps}fps 不整除 22050，无法确定音频块大小")
    tmp = job.target.with_suffix(".donor.amv")
    donor_cmd = build_donor_command(job, tmp, ff)
    try:
        dproc = subprocess.run(donor_cmd, capture_output=True, text=True,
                               encoding="utf-8", errors="replace")
        if dproc.returncode != 0:
            return ConversionResult(ok=False, command=donor_cmd, returncode=dproc.returncode,
                                    stderr_tail=(dproc.stderr or "").strip()[-2000:])
        donor = tmp.read_bytes()
        _, auds = amvenc.split_amv(donor)

        vcmd = build_video_command(job, ff)
        proc = subprocess.Popen(vcmd, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        w, h = p.width, p.height
        frame_sz = w * h * 3 // 2
        fy, fu = w * h, w * h // 4

        def frames():
            last = None
            for _ in auds:  # 1:1 交织：视频不足时重复末帧补齐
                raw = _read_exact(proc.stdout, frame_sz)
                if len(raw) >= frame_sz:
                    y = np.frombuffer(raw[:fy], np.uint8).reshape(h, w)
                    u = np.frombuffer(raw[fy:fy + fu], np.uint8).reshape(h // 2, w // 2)
                    v = np.frombuffer(raw[fy + fu:], np.uint8).reshape(h // 2, w // 2)
                    # 原版比特流 = vflip(显示内容)（BMP bottom-up quirk，实机定案）
                    last = (y[::-1, :], u[::-1, :], v[::-1, :])
                elif last is None:
                    break
                y, u, v = last
                yield amvenc.encode_frame(np.ascontiguousarray(y),
                                          np.ascontiguousarray(u),
                                          np.ascontiguousarray(v))

        try:
            job.target.write_bytes(amvenc.build_file(donor, frames()))
        finally:
            proc.stdout.close()
            proc.stderr.close()
            proc.wait()
        return ConversionResult(ok=True, command=donor_cmd, returncode=0,
                                stderr_tail=f"{len(auds)} blocks")
    finally:
        tmp.unlink(missing_ok=True)
