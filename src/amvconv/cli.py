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


def _cmd_convert(args: argparse.Namespace) -> int:
    if args.avi:
        from .core import aviwriter, ffmpeg as ffmod

        w, h = (int(v) for v in args.size.lower().split("x"))
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

    from .core import ffmpeg

    w, h = (int(v) for v in args.size.lower().split("x"))
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
    p.add_argument("-s", "--size", default="160x120", help="如 160x120")
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
