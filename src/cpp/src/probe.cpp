#include "probe.h"
#include "jsonlite.h"
#include "proc.h"

bool probe_media(const std::string& path, ProbeInfo& info, std::string& err) {
    Process p;
    if (!p.spawn({"ffprobe", "-hide_banner", "-print_format", "json",
                  "-show_format", "-show_streams", path}, true, true)) {
        err = "PATH 中未找到 ffprobe";
        return false;
    }
    std::string out;
    uint8_t buf[8192];
    size_t n;
    while ((n = p.read_out(buf, sizeof buf)) > 0) out.append((char*)buf, n);
    std::string ferr = p.read_err_all();
    int rc = p.wait(-1);
    p.close();
    if (rc != 0) { err = "ffprobe 失败 rc=" + std::to_string(rc); return false; }

    JVal root;
    if (!json_parse(out, root)) { err = "ffprobe JSON 解析失败"; return false; }
    if (const JVal* fmt = root.find("format")) {
        if (const JVal* d = fmt->find("duration")) info.duration = d->as_num();
    }
    if (const JVal* sts = root.find("streams")) {
        for (const JVal& s : sts->arr) {
            std::string ct = s.find("codec_type") ? s.find("codec_type")->as_str() : "";
            if (ct == "video" && !info.has_video) {
                info.has_video = true;
                info.width = (int)(s.find("width") ? s.find("width")->as_num() : 0);
                info.height = (int)(s.find("height") ? s.find("height")->as_num() : 0);
                if (const JVal* r = s.find("avg_frame_rate")) info.avg_frame_rate = r->as_str();
            } else if (ct == "audio" && !info.has_audio) {
                info.has_audio = true;
                info.audio_rate = (int)(s.find("sample_rate") ? s.find("sample_rate")->as_num() : 0);
                info.audio_ch = (int)(s.find("channels") ? s.find("channels")->as_num() : 1);
                if (info.audio_ch <= 0) info.audio_ch = 1;
            }
        }
    }
    return true;
}
