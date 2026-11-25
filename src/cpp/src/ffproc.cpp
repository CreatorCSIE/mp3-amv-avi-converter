// ffmpeg 编排移植（对齐 ffmpeg.py / aviwriter.py 的进程与字节行为）：
// - AMV：donor 通道（lavfi 黑帧 + 真音频 → amv muxer）+ rawvideo 管道逐帧自研编码；
// - AVI：ffmpeg 标准 AVI → avi_remux 固件布局。
// 子进程走 Process 原生 spawn（argv 数组、UTF-8 路径直通、terminate 语义，GUI 阶段定稿）。
#include "ffproc.h"
#include "amvenc.h"
#include "container.h"
#include "proc.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

namespace {

constexpr int AMV_AUDIO_RATE = 22050;
constexpr int AUDIO_BLOCK = 512;    // AVI 模式音频块字节
constexpr int AUDIO_SAMPLES = 1017; // (512-4)*2+1

// pathlib.with_suffix 语义：剥掉最后一段的扩展名再拼接
std::string replace_ext(const std::string& path, const std::string& new_ext) {
    size_t sep = path.find_last_of("/\\");
    size_t dot = path.find_last_of('.');
    if (dot == std::string::npos || (sep != std::string::npos && dot < sep)) return path + new_ext;
    return path.substr(0, dot) + new_ext;
}

bool write_file(const std::string& path, const std::vector<uint8_t>& data) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    size_t n = data.empty() ? 0 : std::fwrite(data.data(), 1, data.size(), f);
    std::fclose(f);
    return n == data.size();
}

std::vector<uint8_t> read_file(const std::string& path, bool& ok) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) { ok = false; return {}; }
    std::fseek(f, 0, SEEK_END);
    long sz = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    std::vector<uint8_t> d(sz > 0 ? (size_t)sz : 0);
    size_t got = d.empty() ? 0 : std::fread(d.data(), 1, d.size(), f);
    std::fclose(f);
    ok = got == d.size();
    d.resize(got);
    return d;
}

std::string tail_str(const std::string& s, size_t maxn = 2000) {
    return s.size() > maxn ? s.substr(s.size() - maxn) : s;
}

// Windows 上杀死 ffmpeg 后句柄释放滞后，删除需短暂重试（aviwriter._unlink_retry）
void unlink_retry(const std::string& path) {
    for (int i = 0; i < 20; i++) {
        if (std::remove(path.c_str()) == 0) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::remove(path.c_str());
}

std::vector<std::string> donor_argv(const std::string& in, const std::string& tmp,
                                    const Profile& p, int block_samples) {
    std::vector<std::string> c = {
        "ffmpeg", "-hide_banner", "-v", "error", "-y", "-i", in,
        "-f", "lavfi", "-i",
        "color=c=black:s=" + std::to_string(p.width) + "x" + std::to_string(p.height) +
            ":r=" + std::to_string(p.fps),
        "-map", "1:v", "-map", "0:a",
        "-c:v", "amv", "-c:a", "adpcm_ima_amv",
        "-ar", std::to_string(AMV_AUDIO_RATE), "-ac", "1",
        "-block_size", std::to_string(block_samples),
        "-shortest", "-f", "amv",
    };
    if (p.height % 16 != 0) c.insert(c.end(), {"-strict", "-1"});  // 放行非 16 倍数高度（固件实测接受）
    c.push_back(tmp);
    return c;
}

std::vector<std::string> video_argv(const std::string& in, const Profile& p, int flip) {
    std::string vf = "scale=" + std::to_string(p.width) + ":" + std::to_string(p.height);
    if (flip) vf += ",transpose=" + std::to_string(flip);
    vf += ",fps=" + std::to_string(p.fps) + ",format=yuvj420p";
    return {"ffmpeg", "-hide_banner", "-v", "error", "-i", in, "-vf", vf, "-f", "rawvideo", "-"};
}

std::vector<std::string> avi_argv(const std::string& in, const std::string& tmp, int w, int h, int q) {
    return {"ffmpeg", "-hide_banner", "-y", "-i", in,
            "-r", std::to_string(AMV_AUDIO_RATE) + "/" + std::to_string(AUDIO_SAMPLES),
            "-vf", "scale=" + std::to_string(w) + ":" + std::to_string(h) + ",format=yuvj420p",
            "-c:v", "mjpeg", "-q:v", std::to_string(q), "-huffman", "default",
            "-c:a", "adpcm_ima_wav", "-ar", std::to_string(AMV_AUDIO_RATE), "-ac", "1",
            "-block_size", std::to_string(AUDIO_BLOCK), "-f", "avi", tmp};
}

// 读满 size 字节（EOF 提前返回短读，与 Python _read_exact 一致）
size_t read_exact(Process& p, uint8_t* buf, size_t size) {
    size_t got = 0;
    while (got < size) {
        size_t n = p.read_out(buf + got, size - got);
        if (n == 0) break;
        got += n;
    }
    return got;
}

int convert_amv(const std::string& in, const std::string& out, const Profile& p,
                const ConvertOptions& opt, std::string& err) {
    if (AMV_AUDIO_RATE % p.fps != 0) {
        err = std::to_string(p.fps) + "fps 不整除 22050，无法确定音频块大小";
        return -1;
    }
    const std::string tmp = replace_ext(out, ".donor.amv");

    Process dp;
    if (!dp.spawn(donor_argv(in, tmp, p, AMV_AUDIO_RATE / p.fps), false, true)) {
        err = "donor 启动失败（ffmpeg 不在 PATH？）";
        return -1;
    }
    std::string derr = dp.read_err_all();
    int rc = dp.wait(-1);
    dp.close();
    if (rc != 0) {
        err = "donor 失败 rc=" + std::to_string(rc) + " " + tail_str(derr);
        return rc;
    }

    bool ok;
    auto donor = read_file(tmp, ok);
    std::remove(tmp.c_str());
    if (!ok) { err = "donor 读取失败"; return -1; }

    AmvDonor d;
    if (!amv_split(donor, d, err)) return -1;

    Process vp;
    if (!vp.spawn(video_argv(in, p, opt.flip), true, true)) {
        err = "视频通道启动失败（ffmpeg 不在 PATH？）";
        return -1;
    }

    const int w = p.width, h = p.height;
    const size_t fsz = (size_t)w * h * 3 / 2;
    const size_t fy = (size_t)w * h, fu = fy / 4;
    const int H = (h + 15) & ~15, W = (w + 15) & ~15;
    const size_t cap = (size_t)(H / 16) * (W / 16) * 6 * 64 * 4 + 64;

    std::vector<uint8_t> raw(fsz), last(fsz), enc(cap);
    bool have_last = false;
    std::vector<std::vector<uint8_t>> frames;
    for (size_t i = 0; i < d.auds.size(); i++) {
        if (opt.cancel && opt.cancel->load()) {
            vp.terminate();
            vp.wait(2000);
            vp.close();
            err = "已取消";
            return -2;
        }
        size_t got = read_exact(vp, raw.data(), fsz);
        if (got >= fsz) {
            // vflip（原版比特流 quirk）：行序倒置拷入 last（y|u|v 平面连续）
            for (int r = 0; r < h; r++)
                std::memcpy(last.data() + (size_t)r * w, raw.data() + (size_t)(h - 1 - r) * w, w);
            for (int r = 0; r < h / 2; r++)
                std::memcpy(last.data() + fy + (size_t)r * (w / 2),
                            raw.data() + fy + (size_t)(h / 2 - 1 - r) * (w / 2), w / 2);
            for (int r = 0; r < h / 2; r++)
                std::memcpy(last.data() + fy + fu + (size_t)r * (w / 2),
                            raw.data() + fy + fu + (size_t)(h / 2 - 1 - r) * (w / 2), w / 2);
            have_last = true;
        } else if (!have_last) {
            break;
        }
        const int n = amv_encode_frame(last.data(), last.data() + fy, last.data() + fy + fu,
                                       w, h, enc.data(), (int)cap);
        if (n < 0) { err = "帧编码失败（缓冲不足？）"; break; }
        frames.emplace_back(enc.begin(), enc.begin() + n);
        if (opt.progress) opt.progress((int)(i + 1), (int)d.auds.size(), opt.user);
    }
    vp.read_err_all();  // 排空防阻塞（-v error 下量极小）
    vp.wait(5000);
    vp.close();
    if (!err.empty()) return -1;

    auto bytes = amv_build(d, frames);
    if (!write_file(out, bytes)) { err = "写出失败: " + out; return -1; }
    return 0;
}

int convert_avi(const std::string& in, const std::string& out, int w, int h,
                const ConvertOptions& opt, std::string& err) {
    const std::string tmp = replace_ext(out, ".raw.avi");
    Process p;
    if (!p.spawn(avi_argv(in, tmp, w, h, opt.q), false, true)) {
        err = "ffmpeg 启动失败（不在 PATH？）";
        return -1;
    }
    const int total = opt.duration > 0 ? (int)std::lround(opt.duration * AMV_AUDIO_RATE / AUDIO_SAMPLES) : -1;

    // ffmpeg stderr 的 frame= 行 → 逐帧进度（aviwriter.convert_avi 语义）
    std::string buf;
    int last = 0;
    bool cancelled = false;
    for (;;) {
        char chunk[256];
        size_t n = p.read_err((uint8_t*)chunk, sizeof chunk);
        if (n == 0) break;
        buf.append(chunk, n);
        // 找 buf 中最后一个 frame=<num>
        int found = -1;
        for (size_t i = buf.find("frame="); i != std::string::npos; i = buf.find("frame=", i + 6)) {
            size_t j = i + 6;
            while (j < buf.size() && (buf[j] == ' ' || buf[j] == '=')) j++;
            int v = 0, digits = 0;
            while (j < buf.size() && isdigit((unsigned char)buf[j])) { v = v * 10 + (buf[j] - '0'); j++; digits++; }
            if (digits) found = v;
        }
        if (found >= 0) {
            last = found;
            buf.clear();
            if (opt.progress) opt.progress(last, total, opt.user);
        }
        if (opt.cancel && opt.cancel->load()) {
            p.terminate();
            cancelled = true;
            break;
        }
    }
    int rc = p.wait(-1);
    p.close();
    if (cancelled) {
        unlink_retry(tmp);
        err = "已取消";
        return -2;
    }
    if (rc == 0) {
        bool ok;
        auto raw = read_file(tmp, ok);
        unlink_retry(tmp);
        if (!ok) { err = "临时 AVI 读取失败"; return -1; }
        auto bytes = avi_remux(raw, w, h, err);
        if (bytes.empty()) { unlink_retry(tmp); return -1; }
        if (!write_file(out, bytes)) { err = "写出失败: " + out; return -1; }
        return 0;
    }
    unlink_retry(tmp);
    err = "ffmpeg 失败 rc=" + std::to_string(rc) + " " + tail_str(buf);
    return rc;
}

}  // namespace

int convert(const std::string& in, const std::string& out, const Profile& p,
            const ConvertOptions& opt, std::string& err) {
    if (opt.mode == "avi") return convert_avi(in, out, p.width, p.height, opt, err);
    return convert_amv(in, out, p, opt, err);
}
