#include "preview.h"
#include "probe.h"

#include <cstdio>
#include <cstring>
#include <thread>

namespace {

// ffmpeg probe 拿不到时长时的容器级兜底（AMV 无 avih 时长字段）——_container_duration 移植
double container_duration(const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return 0.0;
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (sz <= 0) { std::fclose(f); return 0.0; }
    std::vector<uint8_t> d((size_t)sz);
    size_t got = std::fread(d.data(), 1, d.size(), f);
    std::fclose(f);
    d.resize(got);
    auto at = [&](size_t p) -> uint32_t {
        return (uint32_t)d[p] | ((uint32_t)d[p + 1] << 8) | ((uint32_t)d[p + 2] << 16) |
               ((uint32_t)d[p + 3] << 24);
    };
    if (d.size() >= 12 && memcmp(d.data() + 8, "AMV ", 4) == 0) {
        size_t q = 0;
        bool found = false;
        for (size_t i = 0; i + 4 <= d.size(); i++)
            if (memcmp(d.data() + i, "movi", 4) == 0) { q = i; found = true; break; }
        size_t p = found ? q + 4 : 4;
        size_t n = 0;
        while (p + 8 <= d.size()) {
            const uint8_t* fcc = d.data() + p;
            uint32_t s = at(p + 4);
            if (memcmp(fcc, "01wb", 4) == 0) n++;
            else if (memcmp(fcc, "00dc", 4) != 0) break;
            p += 8 + s;
        }
        return n * 1017.0 / 22050.0;
    }
    if (d.size() >= 12 && memcmp(d.data(), "RIFF", 4) == 0 && memcmp(d.data() + 8, "AVI ", 4) == 0) {
        for (size_t i = 0; i + 4 <= d.size(); i++) {
            if (memcmp(d.data() + i, "avih", 4) == 0 && i + 28 <= d.size()) {
                uint32_t usp = at(i + 16);
                uint32_t tot = at(i + 24);
                if (usp) return (double)tot * usp / 1e6;
                break;
            }
        }
    }
    return 0.0;
}

double parse_frac(const std::string& s) {
    size_t sl = s.find('/');
    double n = strtod(s.substr(0, sl).c_str(), nullptr);
    double d = sl == std::string::npos ? 1.0 : strtod(s.substr(sl + 1).c_str(), nullptr);
    if (d == 0) return 0;
    return n / d;
}

}  // namespace

bool Preview::open(const std::string& path_, std::string& err) {
    close();
    path = path_;
    ProbeInfo pi;
    if (!probe_media(path, pi, err)) return false;
    if (!pi.has_video) { err = "无视频流"; return false; }
    width = pi.width;
    height = pi.height;
    audio_rate = pi.has_audio ? pi.audio_rate : 0;
    audio_ch = pi.has_audio ? pi.audio_ch : 0;
    fps = parse_frac(pi.avg_frame_rate);
    if (!fps || fps > 240) fps = 25.0;
    duration = pi.duration;
    if (duration <= 0) duration = container_duration(path);
    seek(0.0);
    return width > 0 && height > 0;
}

void Preview::seek(double seconds) {
    close();
    if (seconds < 0) seconds = 0;
    double cap = duration - 0.05;
    if (cap < 0) cap = 0;
    if (seconds > cap) seconds = cap;
    char ss[32];
    snprintf(ss, sizeof ss, "%.3f", seconds);
    started_ = proc_.spawn({"ffmpeg", "-hide_banner", "-v", "error", "-ss", ss,
                            "-i", path, "-f", "rawvideo", "-pix_fmt", "rgb24", "-"},
                           true, false);
    pos_ = seconds;
}

std::vector<uint8_t> Preview::read() {
    if (!started_) return {};
    const size_t size = (size_t)width * height * 3;
    std::vector<uint8_t> buf(size);
    size_t got = 0;
    while (got < size) {
        size_t n = proc_.read_out(buf.data() + got, size - got);
        if (n == 0) return {};
        got += n;
    }
    pos_ += 1.0 / fps;
    return buf;
}

void Preview::close() {
    if (started_) {
        if (proc_.running()) proc_.terminate();
        proc_.wait(2000);
        proc_.close();
        started_ = false;
    }
}
