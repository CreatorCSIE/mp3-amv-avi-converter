"""从 Python 单一事实源导出 src/cpp/src/tables.h（逐位一致的常量快照）。

DCT_M/TY/TC 直接取 amvenc 计算结果（double 位型十六进制打印，杜绝任何字面量
舍入差异）；ZIG 与哈夫曼 code/len 表同取 amvenc 运行时值。改表必须重跑本脚本。

用法：python scripts/gen_tables_h.py
"""

from __future__ import annotations

import sys
from pathlib import Path

import numpy as np

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "src"))

from amvconv.core import amvenc  # noqa: E402


def hex_doubles(arr: np.ndarray) -> str:
    return ", ".join(f"std::bit_cast<double>(0x{v:016X}ULL)"
                     for v in arr.view(np.uint64).reshape(-1))


def int_arr(name: str, values, ctype: str) -> str:
    body = ",\n    ".join(", ".join(str(v) for v in chunk)
                          for chunk in [list(values[i:i + 16]) for i in range(0, len(values), 16)])
    return f"static const {ctype} {name}[{len(values)}] = {{\n    {body}\n}};"


def dbl_arr(name: str, arr: np.ndarray) -> str:
    vals = arr.reshape(-1)
    lines = [f"    {hex_doubles(vals[i:i + 4])}," for i in range(0, len(vals), 4)]
    return f"static const double {name}[{len(vals)}] = {{\n" + "\n".join(lines) + "\n};"


def main() -> int:
    out = []
    out.append("/* 生成文件，勿手改 —— 由 scripts/gen_tables_h.py 从 amvenc（Python 金标准）导出。 */")
    out.append("#pragma once")
    out.append("#include <bit>")
    out.append("#include <cstdint>")
    out.append("")
    out.append("// 位型十六进制 double：与 numpy float64 逐位一致（无字面量舍入）")
    out.append(dbl_arr("DCT_M", amvenc._DCT_M))
    out.append(dbl_arr("TY", amvenc.TY))
    out.append(dbl_arr("TC", amvenc.TC))
    out.append("")
    out.append(int_arr("ZIG", amvenc.ZIG.tolist(), "int"))
    out.append("")
    out.append("// 符号 -> (码长, 码值)，按符号索引；与 _entropy.c 的入参布局一致")
    for name, d in [("DCY", amvenc._codes(0x00)), ("ACY", amvenc._codes(0x10)),
                    ("DCC", amvenc._codes(0x01)), ("ACC", amvenc._codes(0x11))]:
        n = 16 if name.startswith("DC") else 256
        lens = [d.get(s, (0, 0))[0] for s in range(n)]
        codes = [d.get(s, (0, 0))[1] for s in range(n)]
        out.append(int_arr(f"{name}_L", lens, "int32_t"))
        out.append(int_arr(f"{name}_C", codes, "int32_t"))
    dst = ROOT / "src" / "cpp" / "src" / "tables.h"
    dst.parent.mkdir(parents=True, exist_ok=True)
    dst.write_text("\n".join(out) + "\n", encoding="utf-8")
    print(f"-> {dst}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
