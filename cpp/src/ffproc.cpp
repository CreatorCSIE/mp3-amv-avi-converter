// ffmpeg 编排移植（对齐 ffmpeg.py / aviwriter.py 的进程与字节行为）：
// - AMV：donor 通道（lavfi 黑帧 + 真音频 → amv muxer）+ rawvideo 管道逐帧自研编码；
// - AVI：ffmpeg 标准 AVI → avi_remux 固件布局。
// 子进程用 system/popen（Windows/POSIX 通用；GUI 阶段如需 terminate 语义再换原生 API）。
#include "ffproc.h"
#include "amvenc.h"
#include "container.h"

#include <cstdio>
#include <cstring>
#include <thread>
#include <vector>

#ifdef _WIN32
#define AMV_POPEN _popen
#define AMV_PCLOSE _pclose
#else
#define AMV_POPEN popen
#define AMV_PCLOSE pclose
#endif

namespace {

constexpr int AMV_AUDIO_RATE = 22050;
constexpr int AUDIO_BLOCK = 512;    // AVI 模式音频块字节
constexpr int AUDIO_SAMPLES = 1017; // (512-4)*2+1

std::string quote(const std::string& s) { return "\"" + s + "\""; }

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

std::string tail_file(const std::string& path, size_t maxn = 2000) {
    bool ok;
    auto d = read_file(path, ok);
    if (!ok || d.empty()) return "";
    size_t from = d.size() > maxn ? d.size() - maxn : 0;
    return std::string((const char*)d.data() + from, d.size() - from);
}

// Windows 上杀死 ffmpeg 后句柄释放滞后，删除需短暂重试（aviwriter._unlink_retry）
void unlink_retry(const std::string& path) {
    for (int i = 0; i < 20; i++) {
        if (std::remove(path.c_str()) == 0) return;
        std::this_thread::sleep_for(std::chrono::milliseconds(50));
    }
    std::remove(path.c_str());
}

std::string donor_cmd(const std::string& in, const std::string& tmp, const Profile& p,
                      int block_samples, const std::string& errfile) {
    std::string c = "ffmpeg -hide_banner -v error -y -i " + quote(in) +
        " -f lavfi -i " + quote("color=c=black:s=" + std::to_string(p.width) + "x" +
                                std::to_string(p.height) + ":r=" + std::to_string(p.fps)) +
        " -map 1:v -map 0:a -c:v amv -c:a adpcm_ima_amv -ar " + std::to_string(AMV_AUDIO_RATE) +
        " -ac 1 -block_size " + std::to_string(block_samples) +
        " -shortest -f amv";
    if (p.height % 16 != 0) c += " -strict -1";  // 放行非 16 倍数高度（固件实测接受）
    return c + " " + quote(tmp) + " 2> " + quote(errfile);
}

std::string video_cmd(const std::string& in, const Profile& p, int flip,
                      const std::string& errfile) {
    std::string vf = "scale=" + std::to_string(p.width) + ":" + std::to_string(p.height);
    if (flip) vf += ",transpose=" + std::to_string(flip);
    vf += ",fps=" + std::to_string(p.fps) + ",format=yuvj420p";
    return "ffmpeg -hide_banner -v error -i " + quote(in) + " -vf " + quote(vf) +
           " -f rawvideo - 2> " + quote(errfile);
}

std::string avi_cmd(const std::string& in, const std::string& tmp, int w, int h, int q,
                    const std::string& errfile) {
    return "ffmpeg -hide_banner -y -i " + quote(in) +
           " -r " + std::to_string(AMV_AUDIO_RATE) + "/" + std::to_string(AUDIO_SAMPLES) +
           " -vf " + quote("scale=" + std::to_string(w) + ":" + std::to_string(h) + ",format=yuvj420p") +
           " -c:v mjpeg -q:v " + std::to_string(q) + " -huffman default"
           " -c:a adpcm_ima_wav -ar " + std::to_string(AMV_AUDIO_RATE) + " -ac 1"
           " -block_size " + std::to_string(AUDIO_BLOCK) +
           " -f avi " + quote(tmp) + " 2> " + quote(errfile);
}

// 读满 size 字节（EOF 提前返回短读，与 Python _read_exact 一致）
size_t read_exact(FILE* f, uint8_t* buf, size_t size) {
    size_t got = 0;
    while (got < size) {
        size_t n = std::fread(buf + got, 1, size - got, f);
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
    const std::string verr = replace_ext(out, ".vff.err");
    const std::string derr = replace_ext(out, ".dff.err");

    int rc = std::system(donor_cmd(in, tmp, p, AMV_AUDIO_RATE / p.fps, derr).c_str());
    if (rc != 0) {
        err = "donor 失败 rc=" + std::to_string(rc) + " " + tail_file(derr);
        std::remove(derr.c_str());
        return rc;
    }
    std::remove(derr.c_str());

    bool ok;
    auto donor = read_file(tmp, ok);
    std::remove(tmp.c_str());
    if (!ok) { err = "donor 读取失败"; return -1; }

    AmvDonor d;
    if (!amv_split(donor, d, err)) return -1;

    FILE* pipe = AMV_POPEN(video_cmd(in, p, opt.flip, verr).c_str(), "rb");
    if (!pipe) { err = "视频通道启动失败（ffmpeg 不在 PATH？）"; return -1; }

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
            AMV_PCLOSE(pipe);
            std::remove(verr.c_str());
            err = "已取消";
            return -2;
        }
        size_t got = read_exact(pipe, raw.data(), fsz);
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
    AMV_PCLOSE(pipe);
    std::remove(verr.c_str());
    if (!err.empty()) return -1;

    auto bytes = amv_build(d, frames);
    if (!write_file(out, bytes)) { err = "写出失败: " + out; return -1; }
    return 0;
}

int convert_avi(const std::string& in, const std::string& out, int w, int h, int q,
                std::string& err) {
    const std::string tmp = replace_ext(out, ".raw.avi");
    const std::string ferr = replace_ext(out, ".ff.err");
    int rc = std::system(avi_cmd(in, tmp, w, h, q, ferr).c_str());
    if (rc != 0) {
        err = "ffmpeg 失败 rc=" + std::to_string(rc) + " " + tail_file(ferr);
        unlink_retry(tmp);
        std::remove(ferr.c_str());
        return rc;
    }
    std::remove(ferr.c_str());
    bool ok;
    auto raw = read_file(tmp, ok);
    unlink_retry(tmp);
    if (!ok) { err = "临时 AVI 读取失败"; return -1; }
    auto bytes = avi_remux(raw, w, h, err);
    if (bytes.empty()) return -1;
    if (!write_file(out, bytes)) { err = "写出失败: " + out; return -1; }
    return 0;
}

}  // namespace

int convert(const std::string& in, const std::string& out, const Profile& p,
            const ConvertOptions& opt, std::string& err) {
    if (opt.mode == "avi") return convert_avi(in, out, p.width, p.height, opt.q, err);
    return convert_amv(in, out, p, opt, err);
}
