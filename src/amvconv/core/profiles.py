"""AMV 转换参数表 —— 逆向自原程序 AmvTransform.ini [Option] 段。

原始 ini（GBK）关键行：
  VIDEOSIZE=96X64(16;25;30),...,320X240(16;20;25)   # UI 可选档位
  AVISIZE=96X64(14;22;22),...,320X240(14;22;22)     # 疑似内部实际编码帧率
  CHIPTYPE=1, BMPADJUST=0

已知硬约束（实测 ffmpeg）：音频固定 22050 Hz 单声道，且 22050 必须能被 fps 整除，
每帧音频块样本数 = 22050 / fps。UI 的 16/30 与 AVISIZE 的 14/22 存在矛盾，
属于待深挖项（见 docs/01-reverse-engineering-notes.md）。
"""

from __future__ import annotations

from dataclasses import dataclass

AMV_AUDIO_RATE = 22050  # 原程序与 ffmpeg 输出均为固定 22050 Hz 单声道

# VIDEOSIZE 表：UI 档位（分辨率 -> 可选 fps 列表），照抄 ini
UI_PROFILES: dict[tuple[int, int], list[int]] = {
    (96, 64): [16, 25, 30],
    (128, 96): [16, 25, 30],
    (128, 128): [16, 25, 30],
    (160, 120): [16, 25, 30],
    (160, 128): [16, 25, 30],
    (176, 128): [16, 25, 30],
    (176, 144): [16, 25, 30],
    (192, 160): [16, 25, 30],
    (208, 120): [16, 25, 30],
    (208, 160): [16, 25, 30],
    (208, 176): [16, 25, 30],
    (320, 240): [16, 20, 25],
}

# AVISIZE 表：所有档位均为 (14;22;22)
INTERNAL_PROFILES: dict[tuple[int, int], list[int]] = {
    size: [14, 22, 22] for size in UI_PROFILES
}

# 22050 的整数因子中合理的帧率（1~30）
EXACT_DIVISOR_FPS = [1, 2, 3, 5, 6, 7, 9, 10, 14, 15, 18, 21, 25, 30]


@dataclass(frozen=True)
class Profile:
    width: int
    height: int
    fps: int

    @property
    def audio_block_samples(self) -> int | None:
        """每视频帧对应的音频样本数；非整除时为 None（需填充，策略待逆向确认）。"""
        if AMV_AUDIO_RATE % self.fps == 0:
            return AMV_AUDIO_RATE // self.fps
        return None

    @property
    def ffmpeg_safe(self) -> bool:
        """ffmpeg 的 adpcm_ima_amv 要求 22050 % fps == 0，否则直接拒绝。"""
        return AMV_AUDIO_RATE % self.fps == 0


def list_profiles(source: str = "ui") -> list[Profile]:
    table = UI_PROFILES if source == "ui" else INTERNAL_PROFILES
    return [Profile(w, h, fps) for (w, h), fps_list in table.items() for fps in fps_list]


def pick_closest_fps(requested: int) -> int:
    """当请求帧率不整除 22050 时，选最近的合法帧率（临时策略，最终以逆向结论为准）。"""
    return min(EXACT_DIVISOR_FPS, key=lambda f: (abs(f - requested), f))
