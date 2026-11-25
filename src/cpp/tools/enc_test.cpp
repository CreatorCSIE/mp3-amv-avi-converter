// 帧级逐位回归：golden/frames/ 的 yuv 输入 → C++ 编码 → 与 Python 期望 .bin 全等。
// 用法：enc_test <frames目录>   （目录含 manifest.txt: "id w h n" 行）
#include "amvenc.h"

#include <cstdio>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

static std::vector<uint8_t> read_file(const std::string& p) {
    std::ifstream f(p, std::ios::binary);
    return {(std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>()};
}

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(stderr, "用法: enc_test <frames目录>\n");
        return 2;
    }
    const std::string dir = argv[1];
    std::ifstream mf(dir + "/manifest.txt");
    if (!mf) {
        std::fprintf(stderr, "缺 %s/manifest.txt（先跑 scripts/gen_frame_golden.py）\n", dir.c_str());
        return 2;
    }
    std::string id;
    int w, h, n;
    int total = 0, pass = 0;
    std::vector<uint8_t> buf(4 << 20);
    while (mf >> id >> w >> h >> n) {
        const size_t fsz = (size_t)w * h * 3 / 2;
        for (int k = 0; k < n; k++) {
            const auto yuv = read_file(dir + "/" + id + "_f" + std::to_string(k) + ".yuv");
            const auto exp = read_file(dir + "/" + id + "_f" + std::to_string(k) + ".bin");
            if (yuv.size() != fsz || exp.empty()) {
                std::fprintf(stderr, "FAIL %s f%d 缺文件\n", id.c_str(), k);
                total++;
                continue;
            }
            const int got = amv_encode_frame(yuv.data(), yuv.data() + (size_t)w * h,
                                             yuv.data() + (size_t)w * h * 5 / 4,
                                             w, h, buf.data(), (int)buf.size());
            total++;
            if (got != (int)exp.size() ||
                std::memcmp(buf.data(), exp.data(), exp.size()) != 0) {
                long off = 0;
                while (off < (long)exp.size() && off < got && buf[off] == exp[off]) off++;
                std::fprintf(stderr, "FAIL %s f%d 期望%lldB 实得%dB 首个差异@%lld\n",
                             id.c_str(), k, (long long)exp.size(), got, (long long)off);
            } else {
                pass++;
            }
        }
        std::printf("%s: %d 帧\n", id.c_str(), n);
    }
    std::printf("bit-exact: %d/%d\n", pass, total);
    return pass == total ? 0 : 1;
}
