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
