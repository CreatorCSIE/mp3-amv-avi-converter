"""core 层数据结构：与任何界面壳（PySide6/Tauri/CLI）无关。"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path

from .profiles import Profile


@dataclass
class ConversionJob:
    """一次 AMV 转换任务。"""

    source: Path
    target: Path
    profile: Profile
    flip: int = 0  # 对应 BMPADJUST 0~5（翻转/旋转），默认不变换
    extra_video_args: list[str] = field(default_factory=list)
    extra_audio_args: list[str] = field(default_factory=list)


@dataclass
class ConversionResult:
    ok: bool
    command: list[str]
    returncode: int
    stderr_tail: str = ""
