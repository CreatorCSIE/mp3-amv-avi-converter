"""从 aviwriter（Python 金标准）导出 src/cpp/src/container_data.h（原版容器模板字节）。

_ORIG_HDR / _ORIG_FRAME_HDR 是实机 A/B 定案的固件 gate 模板，改一处必须重跑本脚本，
保持 Python/C++ 单一事实源。

用法：python scripts/gen_container_h.py
"""

from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from amvconv.core.aviwriter import _ORIG_FRAME_HDR, _ORIG_HDR  # noqa: E402


def byte_arr(name: str, data: bytes) -> str:
    lines = [", ".join(f"0x{b:02X}" for b in chunk)
             for chunk in [list(data[i:i + 20]) for i in range(0, len(data), 20)]]
    return f"static const uint8_t {name}[{len(data)}] = {{\n    " + ",\n    ".join(lines) + "\n};"


def main() -> int:
    out = [
        "/* 生成文件，勿手改 —— 由 scripts/gen_container_h.py 从 aviwriter（Python 金标准）导出。 */",
        "#pragma once",
        "#include <cstdint>",
        "",
        f"// 原版容器头部区（RIFF..movi LIST 头，{len(_ORIG_HDR)}B，逐字节）",
        byte_arr("ORIG_HDR", _ORIG_HDR),
        "",
        f"// 原版首帧 JPEG 头（SOI..SOS 前，{len(_ORIG_FRAME_HDR)}B）",
        byte_arr("ORIG_FRAME_HDR", _ORIG_FRAME_HDR),
        "",
    ]
    dst = ROOT / "src" / "cpp" / "src" / "container_data.h"
    dst.write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"-> {dst}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
