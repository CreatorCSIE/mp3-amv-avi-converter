/* AMV 熵编码热循环 C 核（amvenc.py 的等价实现，输出必须逐字节一致）。
 *
 * 输入：预量化 zigzag 系数块（int32 C 连续数组）+ 哈夫曼码表（按符号索引的
 * code/len 数组）+ MCU 网格尺寸。输出：FFD8/FFD9 之间的熵字节（含 0xFF stuffing），
 * 返回写入字节数；缓冲区不足返回 -1（调用方回退 Python 路径）。
 *
 * 构建：gcc -O2 -shared -fPIC -o _entropy.dll _entropy.c
 * （amvenc.py 首次导入时自动尝试编译；失败则纯 Python 回退。）
 */
#include <stdint.h>
#include <string.h>

typedef struct {
    uint8_t *out;
    int cap, len;
    uint32_t cur;
    int n;
} BW;

static inline int bw_put(BW *w, int32_t code, int32_t len) {
    for (int i = len - 1; i >= 0; i--) {
        w->cur = (w->cur << 1) | ((code >> i) & 1);
        if (++w->n == 8) {
            if (w->len + 2 > w->cap) return -1; /* stuffing 也预留 */
            w->out[w->len++] = (uint8_t)w->cur;
            if (w->cur == 0xFF) w->out[w->len++] = 0x00;
            w->cur = 0;
            w->n = 0;
        }
    }
    return 0;
}

/* 编码一个 64 系数块，*newdc 输出新 prevDC；返回 0，<0 表示溢出 */
static int enc_block(BW *w, const int32_t *lv,
                     const int32_t *dc_c, const int32_t *dc_l,
                     const int32_t *ac_c, const int32_t *ac_l,
                     int prevdc, int *newdc) {
    int diff = lv[0] - prevdc;
    unsigned a = diff < 0 ? (unsigned)-diff : (unsigned)diff;
    int s = 0;
    for (unsigned t = a; t; t >>= 1) s++; /* bit_length */
    if (bw_put(w, dc_c[s], dc_l[s]) < 0) return -1;
    if (s) {
        int v = diff >= 0 ? diff : diff + (1 << s) - 1;
        if (bw_put(w, v, s) < 0) return -1;
    }
    int run = 0;
    for (int i = 1; i < 64; i++) {
        int v = lv[i];
        if (v == 0) { run++; continue; }
        while (run >= 16) {
            if (bw_put(w, ac_c[0xF0], ac_l[0xF0]) < 0) return -1;
            run -= 16;
        }
        unsigned av = v < 0 ? (unsigned)-v : (unsigned)v;
        int ss = 0;
        for (unsigned t = av; t; t >>= 1) ss++; /* bit_length */
        if (bw_put(w, ac_c[(run << 4) | ss], ac_l[(run << 4) | ss]) < 0) return -1;
        if (bw_put(w, v >= 0 ? v : v + (1 << ss) - 1, ss) < 0) return -1;
        run = 0;
    }
    if (run) {
        if (bw_put(w, ac_c[0x00], ac_l[0x00]) < 0) return -1; /* EOB */
    }
    *newdc = lv[0];
    return 0;
}

int amv_entropy(const int32_t *Y, const int32_t *U, const int32_t *V,
                int mcu_y, int mcu_x,
                const int32_t *dcy_c, const int32_t *dcy_l,
                const int32_t *acy_c, const int32_t *acy_l,
                const int32_t *dcc_c, const int32_t *dcc_l,
                const int32_t *acc_c, const int32_t *acc_l,
                uint8_t *out, int out_cap) {
    BW w = {out, out_cap, 0, 0, 0};
    int dc[3] = {0, 0, 0};
    int ybx = 2 * mcu_x;
    for (int mcy = 0; mcy < mcu_y; mcy++) {
        for (int mcx = 0; mcx < mcu_x; mcx++) {
            static const int OFF[4][2] = {{0,0},{0,1},{1,0},{1,1}};
            for (int k = 0; k < 4; k++) {
                int by = 2 * mcy + OFF[k][0], bx = 2 * mcx + OFF[k][1];
                if (enc_block(&w, Y + (size_t)(by * ybx + bx) * 64,
                              dcy_c, dcy_l, acy_c, acy_l, dc[0], &dc[0]) < 0) return -1;
            }
            if (enc_block(&w, U + (size_t)(mcy * mcu_x + mcx) * 64,
                          dcc_c, dcc_l, acc_c, acc_l, dc[1], &dc[1]) < 0) return -1;
            if (enc_block(&w, V + (size_t)(mcy * mcu_x + mcx) * 64,
                          dcc_c, dcc_l, acc_c, acc_l, dc[2], &dc[2]) < 0) return -1;
        }
    }
    if (w.n) { /* 尾部补 1 */
        if (w.len + 2 > w.cap) return -1;
        w.cur = (w.cur << (8 - w.n)) | ((1u << (8 - w.n)) - 1);
        w.out[w.len++] = (uint8_t)w.cur;
        if (w.cur == 0xFF) w.out[w.len++] = 0x00;
    }
    return w.len;
}
