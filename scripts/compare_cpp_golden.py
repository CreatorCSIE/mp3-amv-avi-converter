"""Phase1 文件级逐位比对 —— C++ CLI 产物 vs golden/manifest.json（Python 预言机）。

前置：
  python scripts/gen_golden.py            # 语料 + manifest（同机 ffmpeg）
  cmake --build cpp/build                 # 产 cpp/build/amvconv_cli.exe

用法：
  python scripts/compare_cpp_golden.py [--exe PATH] [--only ID_FILTER]

C++ 产物写入 golden/out_cpp/（gitignore），逐条 SHA256 对照 manifest 条目。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--exe", default=str(ROOT / "cpp" / "build" /
                                         ("amvconv_cli.exe" if sys.platform == "win32" else "amvconv_cli")))
    ap.add_argument("--only", default="", help="只跑 id 含此子串的条目")
    args = ap.parse_args()

    exe = Path(args.exe)
    if not exe.is_file():
        raise SystemExit(f"C++ CLI 不存在：{exe}（先 cmake --build cpp/build）")
    manifest_path = ROOT / "golden" / "manifest.json"
    if not manifest_path.is_file():
        raise SystemExit("缺 golden/manifest.json（先跑 scripts/gen_golden.py）")
    manifest = json.loads(manifest_path.read_text(encoding="utf-8"))

    src_dir = ROOT / "golden" / "src"
    out_dir = ROOT / "golden" / "out_cpp"
    out_dir.mkdir(parents=True, exist_ok=True)

    entries = [e for e in manifest["entries"] if args.only in e["id"]]
    bad = 0
    for i, e in enumerate(entries, 1):
        src = src_dir / f"{e['src']}.mkv"
        if not src.is_file():
            raise SystemExit(f"缺合成源 {src}（先跑 scripts/gen_golden.py）")
        w, h = (int(v) for v in e["size"].split("x"))
        cmd = [str(exe), "--mode", e["mode"], "--in", str(src),
               "--out", str(out_dir / e["out"]), "--w", str(w), "--h", str(h)]
        if e["mode"] == "amv":
            cmd += ["--fps", str(e["fps_effective"])]
            if e["flip"]:
                cmd += ["--flip", str(e["flip"])]
        else:
            cmd += ["--q", str(e["q"])]
        r = subprocess.run(cmd, capture_output=True, text=True,
                           encoding="utf-8", errors="replace")
        out = out_dir / e["out"]
        if r.returncode != 0 or not out.is_file():
            bad += 1
            print(f"FAIL  {e['id']}  rc={r.returncode} {r.stderr.strip()[-300:]}")
            continue
        got = sha256_file(out)
        if got != e["sha256"]:
            bad += 1
            print(f"DIFF  {e['id']}  期望 {e['sha256'][:16]}… 实得 {got[:16]}…"
                  f"（{e['bytes']}B vs {out.stat().st_size}B）")
        else:
            print(f"[{i}/{len(entries)}] OK {e['id']}")
    print(f"cpp-vs-golden: {len(entries) - bad}/{len(entries)} 逐位一致")
    return 1 if bad else 0


if __name__ == "__main__":
    raise SystemExit(main())
