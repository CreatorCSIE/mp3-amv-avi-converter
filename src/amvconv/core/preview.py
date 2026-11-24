"""转换结果流式预览：ffmpeg 管道解码 AMV/AVI → RGB24 帧。

用途：GUI 预览对话框逐帧显示（无音频）。注意 ffmpeg 的 AMV 解码器自带
vflip 补偿（mjpegdec flipped=1），输出的就是设备显示取向，所见即所得。
seek 采用重启子进程（-ss 快速定位），播放即顺序 read()。
"""

from __future__ import annotations

import subprocess
from fractions import Fraction
from pathlib import Path

import numpy as np

from .ffmpeg import find_binaries, probe


def _container_duration(path: Path) -> float:
    """ffmpeg probe 拿不到时长时的容器级兜底（AMV 无 avih 时长字段）。"""
    d = path.read_bytes()
    if d[8:12] == b"AMV ":
        q = d.find(b"movi")
        p, n = q + 4, 0
        while p + 8 <= len(d):
            fcc = d[p:p + 4]
            sz = int.from_bytes(d[p + 4:p + 8], "little")
            if fcc == b"01wb":
                n += 1
            elif fcc not in (b"00dc",):
                break
            p += 8 + sz
        return n * 1017 / 22050
    if d[:4] == b"RIFF" and d[8:12] == b"AVI ":
        i = d.find(b"avih")
        if i >= 0:
            usp = int.from_bytes(d[i + 16:i + 20], "little")
            tot = int.from_bytes(d[i + 24:i + 28], "little")
            if usp:
                return tot * usp / 1e6
    return 0.0


class Preview:
    def __init__(self, path: Path):
        self.path = Path(path)
        self.ff, _ = find_binaries()
        info = probe(self.path)
        v = next((s for s in info["streams"] if s.get("codec_type") == "video"), None)
        if v is None:
            raise ValueError("无视频流")
        self.width = int(v["width"])
        self.height = int(v["height"])
        try:
            self.fps = float(Fraction(v.get("avg_frame_rate", "0/1")))
        except (ValueError, ZeroDivisionError):
            self.fps = 0.0
        if not self.fps or self.fps > 240:
            self.fps = 25.0
        self.duration = float(info.get("format", {}).get("duration", 0) or 0)
        if self.duration <= 0:
            self.duration = _container_duration(self.path)
        self._proc: subprocess.Popen | None = None
        self._pos = 0.0
        self.seek(0.0)

    def seek(self, seconds: float) -> None:
        """跳到指定秒（重启解码管道，-ss 在 -i 前走关键帧快速定位）。"""
        self.close()
        seconds = max(0.0, min(seconds, max(0.0, self.duration - 0.05)))
        self._proc = subprocess.Popen(
            [self.ff, "-hide_banner", "-v", "error", "-ss", f"{seconds:.3f}",
             "-i", str(self.path), "-f", "rawvideo", "-pix_fmt", "rgb24", "-"],
            stdout=subprocess.PIPE, bufsize=self.width * self.height * 3)
        self._pos = seconds

    def read(self) -> np.ndarray | None:
        """取下一帧，返回 (h, w, 3) uint8；流结束返回 None。"""
        assert self._proc is not None and self._proc.stdout is not None
        size = self.width * self.height * 3
        buf = bytearray()
        while len(buf) < size:
            chunk = self._proc.stdout.read(size - len(buf))
            if not chunk:
                return None
            buf += chunk
        self._pos += 1.0 / self.fps
        return np.frombuffer(bytes(buf), np.uint8).reshape(self.height, self.width, 3)

    @property
    def position(self) -> float:
        return self._pos

    def close(self) -> None:
        if self._proc is not None:
            try:
                if self._proc.poll() is None:
                    self._proc.terminate()
                self._proc.wait(timeout=2)
            except Exception:
                self._proc.kill()
            self._proc = None
