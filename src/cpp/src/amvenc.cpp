// AMV 编码内核 C++ 移植（amvenc.py 等价实现，输出逐字节一致）。
// 逐位契约：
// - DCT_M/TY/TC 为 numpy float64 位型快照（tables.h 由 gen_tables_h.py 导出）；
// - einsum("ab,nbc,dc->nad") 实测累加序 = b 外 c 内逐项 acc += (M[a,b]*X[b,c])*M[d,c]
//   （probe_einsum 对 coeff_*_nat.f64 转储 20480/20480 逐位验证）；
// - 量化 round = sign(c) * floor(|c|/T + 0.5)，与原版一致；
// - 编译须禁 FMA 收缩（见 CMakeLists 注释）。
#include "amvenc.h"
#include "tables.h"

#include <cmath>
#include <cstring>
#include <vector>

extern "C" int amv_entropy(const int32_t* Y, const int32_t* U, const int32_t* V,
                           int mcu_y, int mcu_x,
                           const int32_t* dcy_c, const int32_t* dcy_l,
                           const int32_t* acy_c, const int32_t* acy_l,
                           const int32_t* dcc_c, const int32_t* dcc_l,
                           const int32_t* acc_c, const int32_t* acc_l,
                           uint8_t* out, int out_cap);

namespace {

// 单平面：(H,W) uint8 → 预量化 zigzag int32，块序 (by,bx,64)（_entropy.c 入参布局）
void quant_plane(const uint8_t* p, int H, int W, const double* table, int32_t* out) {
    const int by = H / 8, bx = W / 8;
    for (int i = 0; i < by * bx; i++) {
        const uint8_t* src = p + (size_t)(i / bx) * 8 * W + (i % bx) * 8;
        double x[64];
        for (int r = 0; r < 8; r++)
            for (int c = 0; c < 8; c++)
                x[r * 8 + c] = (double)src[r * W + c] - 128.0;
        double nat[64];
        for (int a = 0; a < 8; a++) {
            for (int d = 0; d < 8; d++) {
                double acc = 0.0;
                for (int b = 0; b < 8; b++) {
                    const double ab = DCT_M[a * 8 + b];
                    for (int c = 0; c < 8; c++)
                        acc += ab * x[b * 8 + c] * DCT_M[d * 8 + c];
                }
                nat[a * 8 + d] = acc;
            }
        }
        for (int k = 0; k < 64; k++) {
            const double v = nat[ZIG[k]];
            const double q = std::floor(std::fabs(v) / table[k] + 0.5);
            out[i * 64 + k] = v < 0.0 ? (int32_t)-q : (int32_t)q;
        }
    }
}

}  // namespace

int amv_encode_frame(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                     int w, int h, uint8_t* out, int out_cap) {
    // 向上取整到 16：C++ 的 % 向零截断（-120%16=-8），绝不能用 -h%16（Python 语义）
    const int H = (h + 15) & ~15, W = (w + 15) & ~15;
    const int Hc = H / 2, Wc = W / 2;
    std::vector<uint8_t> Y((size_t)H * W, 0), U((size_t)Hc * Wc, 128), V((size_t)Hc * Wc, 128);
    for (int r = 0; r < h; r++) std::memcpy(&Y[(size_t)r * W], y + (size_t)r * w, w);
    for (int r = 0; r < h / 2; r++) std::memcpy(&U[(size_t)r * Wc], u + (size_t)r * (w / 2), w / 2);
    for (int r = 0; r < h / 2; r++) std::memcpy(&V[(size_t)r * Wc], v + (size_t)r * (w / 2), w / 2);

    const int mcu_y = H / 16, mcu_x = W / 16;
    std::vector<int32_t> QY((size_t)(H / 8) * (W / 8) * 64),
        QU((size_t)mcu_y * mcu_x * 64), QV((size_t)mcu_y * mcu_x * 64);
    quant_plane(Y.data(), H, W, TY, QY.data());
    quant_plane(U.data(), Hc, Wc, TC, QU.data());
    quant_plane(V.data(), Hc, Wc, TC, QV.data());

    out[0] = 0xFF;
    out[1] = 0xD8;
    const int n = amv_entropy(QY.data(), QU.data(), QV.data(), mcu_y, mcu_x,
                              DCY_C, DCY_L, ACY_C, ACY_L, DCC_C, DCC_L, ACC_C, ACC_L,
                              out + 2, out_cap - 3);
    if (n < 0) return -1;
    out[2 + n] = 0xFF;
    out[3 + n] = 0xD9;
    return n + 4;
}
