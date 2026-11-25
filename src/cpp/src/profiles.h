#pragma once
// AMV 转换参数表（profiles.py 1:1 移植：原程序 AmvTransform.ini [Option] 逆向）
#include <cstdlib>
#include <vector>

namespace profiles {

inline constexpr int AMV_AUDIO_RATE = 22050;

struct Entry {
    int w, h;
    int fps[3];
};

// VIDEOSIZE 表：UI 档位（分辨率 -> 可选 fps 列表），照抄 ini（顺序即 UI 顺序）
inline const std::vector<Entry>& ui_profiles() {
    static const std::vector<Entry> t = {
        {96, 64, {16, 25, 30}},
        {128, 96, {16, 25, 30}},
        {128, 128, {16, 25, 30}},
        {160, 120, {16, 25, 30}},
        {160, 128, {16, 25, 30}},
        {176, 128, {16, 25, 30}},
        {176, 144, {16, 25, 30}},
        {192, 160, {16, 25, 30}},
        {208, 120, {16, 25, 30}},
        {208, 160, {16, 25, 30}},
        {208, 176, {16, 25, 30}},
        {320, 240, {16, 20, 25}},
    };
    return t;
}

// 22050 的整数因子中合理的帧率（1~30）
inline const std::vector<int>& exact_divisor_fps() {
    static const std::vector<int> v = {1, 2, 3, 5, 6, 7, 9, 10, 14, 15, 18, 21, 25, 30};
    return v;
}

// ffmpeg 的 adpcm_ima_amv 要求 22050 % fps == 0，否则直接拒绝
inline bool ffmpeg_safe(int fps) { return fps > 0 && AMV_AUDIO_RATE % fps == 0; }

// 请求帧率不整除 22050 时，选最近的合法帧率（min by (|f-r|, f)）
inline int pick_closest_fps(int requested) {
    int best = exact_divisor_fps()[0];
    int bd = std::abs(best - requested);
    for (int f : exact_divisor_fps()) {
        int d = std::abs(f - requested);
        if (d < bd) { bd = d; best = f; }
    }
    return best;
}

}  // namespace profiles
