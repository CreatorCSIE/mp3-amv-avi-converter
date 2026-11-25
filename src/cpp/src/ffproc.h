#pragma once
#include <atomic>
#include <string>

// ffmpeg 子进程编排（定稿：归 core）。移植 ffmpeg.py convert + aviwriter.py convert_avi：
// CLI 子进程方式，不嵌库。视频编码不走 ffmpeg（表值不符），只借 donor 通道 + rawvideo 管道。

struct Profile {
    int width, height, fps;
};

// done/total 逐帧回调（total<0 表示未知）；可为 nullptr
using ProgressFn = void (*)(int done, int total, void* user);

struct ConvertOptions {
    std::string mode = "amv";      // "amv" | "avi"
    int flip = 0;                  // BMPADJUST（transpose=N），仅 AMV 通道
    int q = 4;                     // JPEG 质量（2-31），仅 AVI 通道
    double duration = 0.0;         // 源时长（秒），仅 AVI 通道用于估算总帧数；<=0 未知
    ProgressFn progress = nullptr;
    void* user = nullptr;
    const std::atomic<bool>* cancel = nullptr;
};

// 成功返 0；失败返非 0 并置 err（donor rc / ffmpeg stderr 尾部 / 取消=-2）。
int convert(const std::string& in, const std::string& out, const Profile& p,
            const ConvertOptions& opt, std::string& err);
