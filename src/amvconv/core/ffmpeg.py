"""FFmpeg 子进程后端（用户已确认：CLI 子进程方式，不嵌库）。"""

from __future__ import annotations

import json
import shutil
import subprocess
from pathlib import Path

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


def build_command(job: ConversionJob) -> list[str]:
    ff, _ = find_binaries()
    p = job.profile
    filters = [
        f"scale={p.width}:{p.height}:force_original_aspect_ratio=decrease",
        f"pad={p.width}:{p.height}:(ow-iw)/2:(oh-ih)/2",
    ]
    if job.flip:
        filters.append(f"transpose={job.flip}")  # 映射关系待与原工具 BMPADJUST 对齐
    cmd = [
        ff, "-hide_banner", "-y",
        "-i", str(job.source),
        "-r", str(p.fps),
        "-vf", ",".join(filters),
        "-c:v", "amv",
        "-c:a", "adpcm_ima_amv",
        "-ar", str(AMV_AUDIO_RATE),
        "-ac", "1",
        *job.extra_video_args,
        *job.extra_audio_args,
        str(job.target),
    ]
    if p.audio_block_samples is not None:
        # 15fps -> 1470 样本/帧；具体参数名以当前 ffmpeg 版本实测为准
        cmd[-1:-1] = ["-block_size", str(p.audio_block_samples)]
    return cmd


def convert(job: ConversionJob) -> ConversionResult:
    cmd = build_command(job)
    proc = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    tail = (proc.stderr or "").strip().splitlines()[-8:]
    return ConversionResult(
        ok=proc.returncode == 0,
        command=cmd,
        returncode=proc.returncode,
        stderr_tail="\n".join(tail),
    )
