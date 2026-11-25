"""Phase0 金标准语料生成器 —— 把 Python 版的逐位行为固化成 C++ 移植的预言机。

用法：
  python scripts/gen_golden.py                 # 生成 golden/（源 + 产物 + manifest.json）
  python scripts/gen_golden.py --verify        # 全量重转并与 manifest SHA256 比对（确定性自检）
  python scripts/gen_golden.py --src a.mp4     # 追加真实素材（可重复；manifest 记源哈希）

设计约定（docs/06 定稿）：
- 合成源走 lavfi（lossless x264 + PCM），同版本 ffmpeg 下可再生；manifest 记录
  ffmpeg 版本与源 SHA256——版本漂移导致哈希不一致是信号，不是要静默吞掉的噪声。
- AMV 档位沿用 CLI 策略：fps 不整除 22050 时 pick_closest_fps，manifest 同时记
  requested/effective。
- 媒体本体不入库（*.amv/*.avi/*.mkv 已在 .gitignore），manifest.json 入库作参考；
  Phase1 CI 在同一 runner 上现生成、现比对。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import platform
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from amvconv.core import aviwriter  # noqa: E402
from amvconv.core import ffmpeg as ffmod  # noqa: E402
from amvconv.core.models import ConversionJob  # noqa: E402
from amvconv.core.profiles import AMV_AUDIO_RATE, UI_PROFILES, Profile, pick_closest_fps  # noqa: E402

# 合成源：id -> (lavfi 视频, lavfi 音频, 秒)。testsrc2 含运动元素与渐变，DCT/量化覆盖好。
SYNTH = {
    "grad": ("testsrc2=size=320x240:rate=25", "sine=frequency=440:beep_factor=4:sample_rate=22050", 4),
    "bars": ("smptebars=size=256x192:rate=25", "sine=frequency=880:sample_rate=22050", 3),
}
# bars 只跑代表性子集，控制总时长（全矩阵在 grad 上）
BARS_SIZES = [(96, 64), (160, 128), (320, 240)]
BARS_FPS = [16, 25]


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def ff_version() -> str:
    out = subprocess.run(["ffmpeg", "-version"], capture_output=True, text=True,
                         encoding="utf-8", errors="replace")
    return out.stdout.splitlines()[0] if out.stdout else "unknown"


def make_synth(sid: str, out: Path) -> None:
    video, audio, dur = SYNTH[sid]
    ff = shutil.which("ffmpeg")
    if not ff:
        raise SystemExit("PATH 中未找到 ffmpeg")
    cmd = [ff, "-hide_banner", "-v", "error", "-y",
           "-f", "lavfi", "-i", video,
           "-f", "lavfi", "-i", audio,
           "-c:v", "libx264", "-qp", "0",   # 无损 → 解码确定性
           "-c:a", "pcm_s16le",
           "-t", str(dur), str(out)]
    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        raise SystemExit(f"合成源 {sid} 失败：{r.stderr[-800:]}")


def build_jobs(sources: dict[str, Path]) -> list[dict]:
    """矩阵定义（与素材无关的部分），entries 在生成时补 src 哈希与产物哈希。"""
    jobs: list[dict] = []
    for sid in sources:
        full = sid == "grad"
        sizes = UI_PROFILES if full else {s: UI_PROFILES[s] for s in BARS_SIZES}
        for (w, h), fpss in sizes.items():
            for f in (fpss if full else BARS_FPS):
                eff = f if AMV_AUDIO_RATE % f == 0 else pick_closest_fps(f)
                jobs.append(dict(mode="amv", src=sid, w=w, h=h, req=f, eff=eff, flip=0, q=None))
        if full:
            jobs.append(dict(mode="amv", src=sid, w=160, h=120, req=25, eff=25, flip=1, q=None))
            for w, h in UI_PROFILES:
                jobs.append(dict(mode="avi", src=sid, w=w, h=h, req=None, eff=None, flip=0, q=4))
            jobs.append(dict(mode="avi", src=sid, w=160, h=120, req=None, eff=None, flip=0, q=16))
        else:
            jobs.append(dict(mode="avi", src=sid, w=128, h=96, req=None, eff=None, flip=0, q=4))
    for j in jobs:
        tag = f"{j['mode']}_{j['src']}_{j['w']}x{j['h']}"
        if j["mode"] == "amv":
            tag += f"_f{j['req']}" + (f"e{j['eff']}" if j["eff"] != j["req"] else "")
            if j["flip"]:
                tag += "_flip1"
        else:
            tag += f"_q{j['q']}"
        j["id"] = tag
        j["out"] = f"{tag}.{'amv' if j['mode'] == 'amv' else 'avi'}"
    return jobs


def run_job(job: dict, src: Path, out: Path) -> None:
    if job["mode"] == "amv":
        res = ffmod.convert(ConversionJob(
            source=src, target=out,
            profile=Profile(job["w"], job["h"], job["eff"]), flip=job["flip"]))
        if not res.ok:
            raise SystemExit(f"{job['id']} 失败：{res.stderr_tail[-500:]}")
    else:
        proc = aviwriter.convert_avi(src, out, job["w"], job["h"], q=job["q"])
        if proc.returncode != 0:
            raise SystemExit(f"{job['id']} 失败：rc={proc.returncode}")


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--out-dir", default=str(ROOT / "golden"))
    ap.add_argument("--src", action="append", default=[], help="真实素材路径（可重复）")
    ap.add_argument("--verify", action="store_true", help="重转全量并与 manifest 比对")
    args = ap.parse_args()

    out_dir = Path(args.out_dir)
    src_dir, enc_dir = out_dir / "src", out_dir / "out"
    src_dir.mkdir(parents=True, exist_ok=True)
    enc_dir.mkdir(parents=True, exist_ok=True)
    manifest_path = out_dir / "manifest.json"

    sources: dict[str, Path] = {}
    src_meta: list[dict] = []
    for sid in SYNTH:
        p = src_dir / f"{sid}.mkv"
        if not p.exists():
            make_synth(sid, p)
        sources[sid] = p
        src_meta.append(dict(id=sid, kind="lavfi", spec=list(SYNTH[sid]),
                             sha256=sha256_file(p)))
    for s in args.src:
        p = Path(s).resolve()
        if not p.is_file():
            raise SystemExit(f"素材不存在：{p}")
        sid = p.stem
        sources[sid] = p
        src_meta.append(dict(id=sid, kind="file", spec=str(p), sha256=sha256_file(p)))

    jobs = build_jobs(sources)

    if args.verify:
        old = json.loads(manifest_path.read_text(encoding="utf-8"))
        ref = {e["id"]: e["sha256"] for e in old["entries"]}
        bad = 0
        for j in jobs:
            out = enc_dir / j["out"]
            run_job(j, sources[j["src"]], out)
            got = sha256_file(out)
            if ref.get(j["id"]) != got:
                bad += 1
                print(f"MISMATCH {j['id']} 期望 {ref.get(j['id'], '缺条目')[:16]}… 实得 {got[:16]}…")
        print(f"verify: {len(jobs) - bad}/{len(jobs)} 逐位一致")
        return 1 if bad else 0

    entries = []
    for i, j in enumerate(jobs, 1):
        out = enc_dir / j["out"]
        run_job(j, sources[j["src"]], out)
        entries.append(dict(id=j["id"], mode=j["mode"], src=j["src"],
                            size=f"{j['w']}x{j['h']}", fps_requested=j["req"],
                            fps_effective=j["eff"], flip=j["flip"], q=j["q"],
                            out=j["out"], sha256=sha256_file(out), bytes=out.stat().st_size))
        print(f"[{i}/{len(jobs)}] {j['id']} {out.stat().st_size}B")

    manifest = dict(schema=1,
                    created=datetime.now(timezone.utc).isoformat(timespec="seconds"),
                    env=dict(python=sys.version.split()[0], platform=platform.platform(),
                             ffmpeg=ff_version()),
                    sources=src_meta, entries=entries)
    manifest_path.write_text(json.dumps(manifest, ensure_ascii=False, indent=1), encoding="utf-8")
    print(f"manifest: {len(entries)} entries -> {manifest_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
