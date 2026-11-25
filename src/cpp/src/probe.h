#pragma once
#include <string>

// ffprobe JSON 探测（ffmpeg.py probe 的按需裁剪版）
struct ProbeInfo {
    double duration = 0;
    bool has_video = false, has_audio = false;
    int width = 0, height = 0;
    std::string avg_frame_rate;   // "n/d"
    int audio_rate = 0, audio_ch = 0;
};

// 成功返 true；ffprobe 缺失/失败返 false 并置 err
bool probe_media(const std::string& path, ProbeInfo& info, std::string& err);
