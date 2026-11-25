// Phase1 验证 CLI：与 Python 版同参数的端到端转换（金标准比对入口）。
// 用法：amvconv_cli --mode amv|avi --in SRC --out DST --w W --h H
//        AMV: --fps F [--flip N]   AVI: [--q N]
#include "ffproc.h"

#include <cstdio>
#include <cstring>
#include <string>

int main(int argc, char** argv) {
    ConvertOptions opt;
    Profile p{0, 0, 25};
    std::string in, out;
    for (int i = 1; i < argc; i++) {
        auto next = [&](const char* name) -> const char* {
            if (i + 1 >= argc) {
                std::fprintf(stderr, "缺参数值: %s\n", name);
                std::exit(2);
            }
            return argv[++i];
        };
        if (!std::strcmp(argv[i], "--mode")) opt.mode = next("--mode");
        else if (!std::strcmp(argv[i], "--in")) in = next("--in");
        else if (!std::strcmp(argv[i], "--out")) out = next("--out");
        else if (!std::strcmp(argv[i], "--w")) p.width = std::atoi(next("--w"));
        else if (!std::strcmp(argv[i], "--h")) p.height = std::atoi(next("--h"));
        else if (!std::strcmp(argv[i], "--fps")) p.fps = std::atoi(next("--fps"));
        else if (!std::strcmp(argv[i], "--flip")) opt.flip = std::atoi(next("--flip"));
        else if (!std::strcmp(argv[i], "--q")) opt.q = std::atoi(next("--q"));
        else {
            std::fprintf(stderr, "未知参数: %s\n", argv[i]);
            return 2;
        }
    }
    if (in.empty() || out.empty() || p.width <= 0 || p.height <= 0) {
        std::fprintf(stderr, "用法: amvconv_cli --mode amv|avi --in SRC --out DST --w W --h H [--fps F] [--flip N] [--q N]\n");
        return 2;
    }
    std::string err;
    const int rc = convert(in, out, p, opt, err);
    if (rc != 0) {
        std::fprintf(stderr, "FAIL rc=%d %s\n", rc, err.c_str());
        return rc < 0 ? 1 : rc;
    }
    return 0;
}
