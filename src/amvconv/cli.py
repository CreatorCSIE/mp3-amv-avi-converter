"""amv-conv 命令行入口 —— 也是逆向对比测试的驱动工具。

用法：
  python -m amvconv.cli profiles              # 打印参数表
  python -m amvconv.cli convert IN OUT -s 160x120 -f 15
  python -m amvconv.cli inspect A.amv [B.amv] # 容器结构摘要（可多个，用于对比）
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

from .core import amv, profiles
from .core.models import ConversionJob
from .core.profiles import Profile


def _cmd_profiles(args: argparse.Namespace) -> int:
    table = profiles.UI_PROFILES if args.source == "ui" else profiles.INTERNAL_PROFILES
    for (w, h), fps_list in table.items():
        marks = []
        for f in fps_list:
            ok = profiles.AMV_AUDIO_RATE % f == 0
            marks.append(f"{f}{'' if ok else '×'}")
        print(f"{w}x{h}: {' '.join(marks)}   (×=不整除22050, ffmpeg 会拒绝)")
    return 0


def _parse_size(s: str) -> tuple[int, int] | None:
    try:
        w, h = s.lower().replace("×", "x").split("x")
        return int(w), int(h)
    except ValueError:
        return None


def _check_size(w: int, h: int, allow_custom: bool) -> str | None:
    """白名单=12 档 VIDEOSIZE；--allow-custom 放行表外实验尺寸，底线：偶数、≥16、≤320x240。"""
    if (w, h) in profiles.UI_PROFILES:
        return None
    if not allow_custom:
        sizes = " ".join(f"{a}x{b}" for a, b in profiles.UI_PROFILES)
        return f"{w}x{h} 不在原版参数表内。可选：{sizes}（实验尺寸请加 --allow-custom，产物不保证设备可播）"
    if w % 2 or h % 2:
        return f"自定义尺寸须为偶数（4:2:0 色度要求），给定 {w}x{h}"
    if w < 16 or h < 16:
        return f"自定义尺寸不得小于 16x16（一个 MCU），给定 {w}x{h}"
    if w > 320 or h > 240:
        return f"超出 ATJ2157 硬件上限 320x240，给定 {w}x{h}"
    return None


def _check_fps(w: int, h: int, fps: int) -> str | None:
    """表内档位帧率须命中候选；自定义尺寸只要求正整数（整除性沿用既有临时策略）。"""
    if fps < 1:
        return f"帧率须为正整数，给定 {fps}"
    cand = profiles.UI_PROFILES.get((w, h))
    if cand is not None and fps not in cand:
        return f"{w}x{h} 档帧率候选为 {cand}，给定 {fps}"
    return None


def _cmd_convert(args: argparse.Namespace) -> int:
    size = _parse_size(args.size)
    if size is None:
        print(f"错误：无法解析分辨率 {args.size!r}（形如 160x120）", file=sys.stderr)
        return 2
    w, h = size
    if err := _check_size(w, h, args.allow_custom):
        print(f"错误：{err}", file=sys.stderr)
        return 2

    if args.avi:
        from .core import aviwriter, ffmpeg as ffmod

        try:
            dur = float(ffmod.probe(Path(args.src))["format"].get("duration", 0)) or None
        except Exception:
            dur = None
        proc = aviwriter.convert_avi(Path(args.src), Path(args.dst), w, h, q=args.q,
                                     duration=dur,
                                     progress=lambda d, t: print(f"\r{d}{'' if t is None else f'/{t}'} frames",
                                                                 end="", file=sys.stderr))
        print()
        print("OK" if proc.returncode == 0 else f"FAIL rc={proc.returncode}\n{(proc.stderr or '')[-800:]}")
        return 0 if proc.returncode == 0 else 1

    if err := _check_fps(w, h, args.fps):
        print(f"错误：{err}", file=sys.stderr)
        return 2

    from .core import ffmpeg

    prof = Profile(w, h, args.fps)
    if not prof.ffmpeg_safe:
        fixed = profiles.pick_closest_fps(args.fps)
        print(f"警告: {args.fps}fps 不整除 22050，改用 {fixed}fps（临时策略）", file=sys.stderr)
        prof = Profile(w, h, fixed)
    job = ConversionJob(source=Path(args.src), target=Path(args.dst), profile=prof, flip=args.flip)
    res = ffmpeg.convert(job)
    print(res.stderr_tail, file=sys.stderr)
    print("OK" if res.ok else f"FAIL rc={res.returncode}")
    return 0 if res.ok else 1


def _cmd_inspect(args: argparse.Namespace) -> int:
    reports = {}
    for p in args.paths:
        reports[Path(p).name] = amv.inspect_file(Path(p))
    print(json.dumps(reports, ensure_ascii=False, indent=1))
    return 0


def _ensure_console_utf8() -> None:
    """Windows 控制台默认跟随 ANSI 代码页（非中文系统 print 中文直接 UnicodeEncodeError，
    CI 实锤 cp1252）：把进程输出代码页切到 UTF-8 并重配 stdio；重定向到日志/管道无害。"""
    if sys.platform == "win32":
        try:
            import ctypes
            ctypes.windll.kernel32.SetConsoleOutputCP(65001)
        except OSError:
            pass
    for s in (sys.stdout, sys.stderr):
        try:
            s.reconfigure(encoding="utf-8", errors="replace")
        except (AttributeError, OSError):
            pass


def main(argv: list[str] | None = None) -> int:
    _ensure_console_utf8()
    ap = argparse.ArgumentParser(prog="amv-conv", description="AMV 转换器（逆向重写）")
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("profiles", help="打印原程序参数表")
    p.add_argument("--source", choices=["ui", "internal"], default="ui")
    p.set_defaults(fn=_cmd_profiles)

    p = sub.add_parser("convert", help="转换为 .amv（--avi 转原版 AVI 模式 .avi）")
    p.add_argument("src")
    p.add_argument("dst")
    p.add_argument("-s", "--size", default="160x120",
                   help="如 160x120（须命中 profiles 参数表 12 档；实验尺寸加 --allow-custom）")
    p.add_argument("--allow-custom", action="store_true",
                   help="放行表外实验尺寸（底线：偶数、≥16x16、≤320x240；产物不保证设备可播）")
    p.add_argument("-f", "--fps", type=int, default=15)
    p.add_argument("--flip", type=int, default=0, help="BMPADJUST 0~5（仅 AMV）")
    p.add_argument("--avi", action="store_true",
                   help="AVI 模式（固件兼容布局，帧率恒 ~21.68 音频驱动，PC 播放器友好）")
    p.add_argument("--q", type=int, default=4, help="AVI 模式 JPEG 质量 2-31，越小越好")
    p.set_defaults(fn=_cmd_convert)

    p = sub.add_parser("inspect", help="AMV 容器结构摘要（对比测试用）")
    p.add_argument("paths", nargs="+")
    p.set_defaults(fn=_cmd_inspect)

    ns = ap.parse_args(argv)
    return ns.fn(ns)


if __name__ == "__main__":
    raise SystemExit(main())
