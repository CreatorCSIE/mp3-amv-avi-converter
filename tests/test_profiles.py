from amvconv.core import profiles
from amvconv.core.profiles import Profile, AMV_AUDIO_RATE


def test_block_sizes():
    assert Profile(160, 120, 15).audio_block_samples == 1470
    assert Profile(160, 120, 14).audio_block_samples == 1575
    assert Profile(320, 240, 25).audio_block_samples == 882


def test_ffmpeg_safety():
    assert not Profile(96, 64, 16).ffmpeg_safe      # 22050 % 16 != 0
    assert not Profile(96, 64, 22).ffmpeg_safe      # AVISIZE 的 22 也不整除，待逆向解释
    assert Profile(96, 64, 30).ffmpeg_safe


def test_tables_match_ini():
    assert len(profiles.UI_PROFILES) == 12
    assert profiles.UI_PROFILES[(320, 240)] == [16, 20, 25]
    assert all(v == [14, 22, 22] for v in profiles.INTERNAL_PROFILES.values())
    assert AMV_AUDIO_RATE == 22050


def test_build_command_strict_and_block():
    from pathlib import Path
    from amvconv.core.ffmpeg import build_donor_command, build_video_command
    from amvconv.core.models import ConversionJob

    def job_for(w, h, fps):
        return ConversionJob(
            source=Path("in.mp4"), target=Path("out.amv"),
            profile=Profile(w, h, fps), extra_video_args=[], extra_audio_args=[],
        )

    # 16 倍数高度：donor 有 -block_size，无 -strict
    c = build_donor_command(job_for(160, 128, 15), Path("d.amv"), "ffmpeg")
    assert "-block_size" in c and "1470" in c
    assert "-strict" not in c
    assert c[-1] == "d.amv"
    # 非 16 倍数高度（160x120）：补 -strict -1，否则 amv 编码器拒绝、固件本可接受
    c = build_donor_command(job_for(160, 120, 15), Path("d.amv"), "ffmpeg")
    i = c.index("-strict")
    assert c[i + 1] == "-1"
    assert i < c.index("d.amv")  # 必须在输出路径之前
    # 视频通道：硬拉伸 + fps + yuvj420p，输出 rawvideo 管道
    v = build_video_command(job_for(128, 128, 30), "ffmpeg")
    vf = v[v.index("-vf") + 1]
    assert vf == "scale=128:128,fps=30,format=yuvj420p"
    assert v[-3:] == ["-f", "rawvideo", "-"]
