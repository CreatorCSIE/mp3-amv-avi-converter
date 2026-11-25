"""校验脚本共用：定位 Python 预言机包（amvconv core）。

C++ 分支不保留 Py 源码 —— 预言机 = PySide6 分支。解析顺序：
  1. 环境变量 AMVCONV_ORACLE（指向含 amvconv 包的 src 目录）；
  2. 兄弟 worktree ../mp3-amv-avi-converter-oracle/src。
本地一次性准备：git worktree add --detach ../mp3-amv-avi-converter-oracle PySide6
"""

from __future__ import annotations

import os
from pathlib import Path


def oracle_src(root: Path) -> Path:
    env = os.environ.get("AMVCONV_ORACLE")
    cand = Path(env) if env else root.parent / "mp3-amv-avi-converter-oracle" / "src"
    if not (cand / "amvconv").is_dir():
        raise SystemExit(
            f"未找到 Python 预言机包（{cand}）\n"
            "本地：git worktree add --detach ../mp3-amv-avi-converter-oracle PySide6\n"
            "或设 AMVCONV_ORACLE=<含 amvconv 的 src 目录>")
    return cand
