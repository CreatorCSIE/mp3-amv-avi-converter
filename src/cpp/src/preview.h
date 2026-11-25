#pragma once
#include <string>
#include <vector>
#include "proc.h"

// 流式预览（preview.py 移植）：ffmpeg rawvideo 管道解码 → RGB24 帧。
// AMV 解码自带 vflip 补偿 = 设备显示取向；seek = 重启子进程（-ss 快速定位）。
class Preview {
public:
    Preview() = default;
    ~Preview() { close(); }
    Preview(const Preview&) = delete;
    Preview& operator=(const Preview&) = delete;

    // 成功返 true；无视频流/probe 失败返 false 并置 err（对应 Python 构造异常）
    bool open(const std::string& path, std::string& err);
    void seek(double seconds);
    // 取下一帧 w*h*3 RGB；流结束返空
    std::vector<uint8_t> read();
    double position() const { return pos_; }
    void close();

    std::string path;
    int width = 0, height = 0;
    double fps = 25.0, duration = 0.0;
    int audio_rate = 0, audio_ch = 0;

private:
    Process proc_;
    double pos_ = 0.0;
    bool started_ = false;
};
