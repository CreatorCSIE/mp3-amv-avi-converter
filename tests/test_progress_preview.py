"""转换进度/取消回调与预览解码的端到端测试（依赖 PATH 上的 ffmpeg）。"""

from __future__ import annotations

import subprocess
import threading
from pathlib import Path

import pytest

from amvconv.core import aviwriter, ffmpeg, preview
from amvconv.core.models import ConversionJob
from amvconv.core.profiles import Profile

pytestmark = pytest.mark.skipif(
    subprocess.run(["ffmpeg", "-version"], capture_output=True).returncode != 0,
    reason="需要 ffmpeg")


@pytest.fixture(scope="module")
def src(tmp_path_factory) -> Path:
    p = tmp_path_factory.mktemp("prog") / "src.mp4"
    subprocess.run(
        ["ffmpeg", "-hide_banner", "-v", "error", "-y",
         "-f", "lavfi", "-i", "testsrc=size=320x240:rate=25:duration=2",
         "-f", "lavfi", "-i", "sine=frequency=440:duration=2",
         "-shortest", str(p)], check=True)
    return p


def test_amv_progress(src, tmp_path):
    seen = []
    job = ConversionJob(source=src, target=tmp_path / "o.amv", profile=Profile(160, 120, 25))
    res = ffmpeg.convert(job, progress=lambda d, t: seen.append((d, t)))
    assert res.ok
    assert seen and seen[0][0] == 1 and seen[-1] == (seen[-1][1], seen[-1][1])
    assert all(a[0] <= b[0] and b[0] - a[0] == 1 for a, b in zip(seen, seen[1:]))


def test_amv_cancel(src, tmp_path):
    ev = threading.Event()
    n = []

    def prog(d, t):
        n.append(d)
        if len(n) >= 3:
            ev.set()

    job = ConversionJob(source=src, target=tmp_path / "c.amv", profile=Profile(160, 120, 25))
    res = ffmpeg.convert(job, progress=prog, cancel=ev)
    assert not res.ok and res.returncode == -2
    assert not (tmp_path / "c.amv").exists()


def test_avi_progress_cancel(src, tmp_path):
    seen = []
    dur = float(ffmpeg.probe(src)["format"]["duration"])
    res = aviwriter.convert_avi(src, tmp_path / "o.avi", 160, 120,
                                progress=lambda d, t: seen.append((d, t)), duration=dur)
    assert res.returncode == 0 and seen and seen[-1][0] > 10
    out = (tmp_path / "o.avi").read_bytes()
    assert out[:4] == b"RIFF" and b"idx1" in out

    ev = threading.Event()
    res2 = aviwriter.convert_avi(src, tmp_path / "x.avi", 160, 120, cancel=ev)
    ev.set()
    res2 = aviwriter.convert_avi(src, tmp_path / "x.avi", 160, 120,
                                 progress=lambda d, t: ev.set(), cancel=ev)
    assert res2.returncode == -2


def test_preview(src, tmp_path):
    target = tmp_path / "p.amv"
    job = ConversionJob(source=src, target=target, profile=Profile(160, 120, 25))
    assert ffmpeg.convert(job).ok
    pv = preview.Preview(target)
    try:
        assert (pv.width, pv.height) == (160, 120)
        f0 = pv.read()
        assert f0 is not None and f0.shape == (120, 160, 3)
        pv.seek(1.0)
        assert pv.read() is not None
    finally:
        pv.close()
