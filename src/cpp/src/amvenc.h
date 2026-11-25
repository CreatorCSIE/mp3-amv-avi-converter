#pragma once
#include <cstdint>

// 编码一帧 yuvj420p 平面（uint8，未 vflip 前已由调用方处理方向）→ 无头 JPEG。
// 与 amvenc.encode_frame 逐位一致：非 16 倍数尺寸补齐（y=0 / uv=128），
// 输出 FFD8 + 熵数据 + FFD9，返回总字节数；缓冲不足返回 -1。
int amv_encode_frame(const uint8_t* y, const uint8_t* u, const uint8_t* v,
                     int w, int h, uint8_t* out, int out_cap);
