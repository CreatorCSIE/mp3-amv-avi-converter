/* 生成文件，勿手改 —— 由 scripts/gen_tables_h.py 从 amvenc（Python 金标准）导出。 */
#pragma once
#include <bit>
#include <cstdint>

// 位型十六进制 double：与 numpy float64 逐位一致（无字面量舍入）
static const double DCT_M[64] = {
    std::bit_cast<double>(0x3FD6A09E667F3BCCULL), std::bit_cast<double>(0x3FD6A09E667F3BCCULL), std::bit_cast<double>(0x3FD6A09E667F3BCCULL), std::bit_cast<double>(0x3FD6A09E667F3BCCULL),
    std::bit_cast<double>(0x3FD6A09E667F3BCCULL), std::bit_cast<double>(0x3FD6A09E667F3BCCULL), std::bit_cast<double>(0x3FD6A09E667F3BCCULL), std::bit_cast<double>(0x3FD6A09E667F3BCCULL),
    std::bit_cast<double>(0x3FDF6297CFF75CB0ULL), std::bit_cast<double>(0x3FDA9B66290EA1A3ULL), std::bit_cast<double>(0x3FD1C73B39AE68C9ULL), std::bit_cast<double>(0x3FB8F8B83C69A60DULL),
    std::bit_cast<double>(0xBFB8F8B83C69A608ULL), std::bit_cast<double>(0xBFD1C73B39AE68C6ULL), std::bit_cast<double>(0xBFDA9B66290EA1A4ULL), std::bit_cast<double>(0xBFDF6297CFF75CB0ULL),
    std::bit_cast<double>(0x3FDD906BCF328D46ULL), std::bit_cast<double>(0x3FC87DE2A6AEA964ULL), std::bit_cast<double>(0xBFC87DE2A6AEA962ULL), std::bit_cast<double>(0xBFDD906BCF328D46ULL),
    std::bit_cast<double>(0xBFDD906BCF328D47ULL), std::bit_cast<double>(0xBFC87DE2A6AEA96DULL), std::bit_cast<double>(0x3FC87DE2A6AEA967ULL), std::bit_cast<double>(0x3FDD906BCF328D44ULL),
    std::bit_cast<double>(0x3FDA9B66290EA1A3ULL), std::bit_cast<double>(0xBFB8F8B83C69A608ULL), std::bit_cast<double>(0xBFDF6297CFF75CB0ULL), std::bit_cast<double>(0xBFD1C73B39AE68C8ULL),
    std::bit_cast<double>(0x3FD1C73B39AE68C5ULL), std::bit_cast<double>(0x3FDF6297CFF75CB2ULL), std::bit_cast<double>(0x3FB8F8B83C69A5FEULL), std::bit_cast<double>(0xBFDA9B66290EA199ULL),
    std::bit_cast<double>(0x3FD6A09E667F3BCDULL), std::bit_cast<double>(0xBFD6A09E667F3BCCULL), std::bit_cast<double>(0xBFD6A09E667F3BCEULL), std::bit_cast<double>(0x3FD6A09E667F3BCBULL),
    std::bit_cast<double>(0x3FD6A09E667F3BCEULL), std::bit_cast<double>(0xBFD6A09E667F3BC5ULL), std::bit_cast<double>(0xBFD6A09E667F3BC9ULL), std::bit_cast<double>(0x3FD6A09E667F3BC4ULL),
    std::bit_cast<double>(0x3FD1C73B39AE68C9ULL), std::bit_cast<double>(0xBFDF6297CFF75CB0ULL), std::bit_cast<double>(0x3FB8F8B83C69A60CULL), std::bit_cast<double>(0x3FDA9B66290EA1A6ULL),
    std::bit_cast<double>(0xBFDA9B66290EA1A2ULL), std::bit_cast<double>(0xBFB8F8B83C69A641ULL), std::bit_cast<double>(0x3FDF6297CFF75CAFULL), std::bit_cast<double>(0xBFD1C73B39AE68C2ULL),
    std::bit_cast<double>(0x3FC87DE2A6AEA964ULL), std::bit_cast<double>(0xBFDD906BCF328D47ULL), std::bit_cast<double>(0x3FDD906BCF328D44ULL), std::bit_cast<double>(0xBFC87DE2A6AEA965ULL),
    std::bit_cast<double>(0xBFC87DE2A6AEA971ULL), std::bit_cast<double>(0x3FDD906BCF328D4CULL), std::bit_cast<double>(0xBFDD906BCF328D49ULL), std::bit_cast<double>(0x3FC87DE2A6AEA924ULL),
    std::bit_cast<double>(0x3FB8F8B83C69A60DULL), std::bit_cast<double>(0xBFD1C73B39AE68C8ULL), std::bit_cast<double>(0x3FDA9B66290EA1A6ULL), std::bit_cast<double>(0xBFDF6297CFF75CB2ULL),
    std::bit_cast<double>(0x3FDF6297CFF75CB1ULL), std::bit_cast<double>(0xBFDA9B66290EA198ULL), std::bit_cast<double>(0x3FD1C73B39AE68DCULL), std::bit_cast<double>(0xBFB8F8B83C69A598ULL),
};
static const double TY[64] = {
    std::bit_cast<double>(0x4020000000000000ULL), std::bit_cast<double>(0x4018000000000000ULL), std::bit_cast<double>(0x4018000000000000ULL), std::bit_cast<double>(0x401C000000000000ULL),
    std::bit_cast<double>(0x4018000000000000ULL), std::bit_cast<double>(0x4014000000000000ULL), std::bit_cast<double>(0x4020000000000000ULL), std::bit_cast<double>(0x401C000000000000ULL),
    std::bit_cast<double>(0x401C000000000000ULL), std::bit_cast<double>(0x401C000000000000ULL), std::bit_cast<double>(0x4022000000000000ULL), std::bit_cast<double>(0x4022000000000000ULL),
    std::bit_cast<double>(0x4020000000000000ULL), std::bit_cast<double>(0x4024000000000000ULL), std::bit_cast<double>(0x4028000000000000ULL), std::bit_cast<double>(0x4034000000000000ULL),
    std::bit_cast<double>(0x402A000000000000ULL), std::bit_cast<double>(0x4028000000000000ULL), std::bit_cast<double>(0x4026000000000000ULL), std::bit_cast<double>(0x4026000000000000ULL),
    std::bit_cast<double>(0x4028000000000000ULL), std::bit_cast<double>(0x4039000000000000ULL), std::bit_cast<double>(0x4032000000000000ULL), std::bit_cast<double>(0x4033000000000000ULL),
    std::bit_cast<double>(0x402E000000000000ULL), std::bit_cast<double>(0x4034000000000000ULL), std::bit_cast<double>(0x403D000000000000ULL), std::bit_cast<double>(0x403A000000000000ULL),
    std::bit_cast<double>(0x403F000000000000ULL), std::bit_cast<double>(0x403E000000000000ULL), std::bit_cast<double>(0x403D000000000000ULL), std::bit_cast<double>(0x403A000000000000ULL),
    std::bit_cast<double>(0x403C000000000000ULL), std::bit_cast<double>(0x403C000000000000ULL), std::bit_cast<double>(0x4040000000000000ULL), std::bit_cast<double>(0x4042000000000000ULL),
    std::bit_cast<double>(0x4047000000000000ULL), std::bit_cast<double>(0x4043800000000000ULL), std::bit_cast<double>(0x4040000000000000ULL), std::bit_cast<double>(0x4041000000000000ULL),
    std::bit_cast<double>(0x4046000000000000ULL), std::bit_cast<double>(0x4041800000000000ULL), std::bit_cast<double>(0x403C000000000000ULL), std::bit_cast<double>(0x403C000000000000ULL),
    std::bit_cast<double>(0x4044000000000000ULL), std::bit_cast<double>(0x404B800000000000ULL), std::bit_cast<double>(0x4044800000000000ULL), std::bit_cast<double>(0x4046000000000000ULL),
    std::bit_cast<double>(0x4048000000000000ULL), std::bit_cast<double>(0x4048800000000000ULL), std::bit_cast<double>(0x404A000000000000ULL), std::bit_cast<double>(0x404A000000000000ULL),
    std::bit_cast<double>(0x404A000000000000ULL), std::bit_cast<double>(0x403F000000000000ULL), std::bit_cast<double>(0x4043800000000000ULL), std::bit_cast<double>(0x404C800000000000ULL),
    std::bit_cast<double>(0x404E800000000000ULL), std::bit_cast<double>(0x404C000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x404E000000000000ULL),
    std::bit_cast<double>(0x4047000000000000ULL), std::bit_cast<double>(0x4049800000000000ULL), std::bit_cast<double>(0x404A000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
};
static const double TC[64] = {
    std::bit_cast<double>(0x4022000000000000ULL), std::bit_cast<double>(0x4022000000000000ULL), std::bit_cast<double>(0x4022000000000000ULL), std::bit_cast<double>(0x4028000000000000ULL),
    std::bit_cast<double>(0x4026000000000000ULL), std::bit_cast<double>(0x4028000000000000ULL), std::bit_cast<double>(0x4038000000000000ULL), std::bit_cast<double>(0x402A000000000000ULL),
    std::bit_cast<double>(0x402A000000000000ULL), std::bit_cast<double>(0x4038000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4040800000000000ULL),
    std::bit_cast<double>(0x403C000000000000ULL), std::bit_cast<double>(0x4040800000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
    std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL), std::bit_cast<double>(0x4049000000000000ULL),
};

static const int ZIG[64] = {
    0, 1, 8, 16, 9, 2, 3, 10, 17, 24, 32, 25, 18, 11, 4, 5,
    12, 19, 26, 33, 40, 48, 41, 34, 27, 20, 13, 6, 7, 14, 21, 28,
    35, 42, 49, 56, 57, 50, 43, 36, 29, 22, 15, 23, 30, 37, 44, 51,
    58, 59, 52, 45, 38, 31, 39, 46, 53, 60, 61, 54, 47, 55, 62, 63
};

// 符号 -> (码长, 码值)，按符号索引；与 _entropy.c 的入参布局一致
static const int32_t DCY_L[16] = {
    2, 3, 3, 3, 3, 3, 4, 5, 6, 7, 8, 9, 0, 0, 0, 0
};
static const int32_t DCY_C[16] = {
    0, 2, 3, 4, 5, 6, 14, 30, 62, 126, 254, 510, 0, 0, 0, 0
};
static const int32_t ACY_L[256] = {
    4, 2, 2, 3, 4, 5, 7, 8, 10, 16, 16, 0, 0, 0, 0, 0,
    0, 4, 5, 7, 9, 11, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 5, 8, 10, 12, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 6, 9, 12, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 6, 10, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 7, 11, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 7, 12, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 8, 12, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 9, 15, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 9, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 9, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 10, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 10, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 11, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    11, 16, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0
};
static const int32_t ACY_C[256] = {
    10, 0, 1, 4, 11, 26, 120, 248, 1014, 65410, 65411, 0, 0, 0, 0, 0,
    0, 12, 27, 121, 502, 2038, 65412, 65413, 65414, 65415, 65416, 0, 0, 0, 0, 0,
    0, 28, 249, 1015, 4084, 65417, 65418, 65419, 65420, 65421, 65422, 0, 0, 0, 0, 0,
    0, 58, 503, 4085, 65423, 65424, 65425, 65426, 65427, 65428, 65429, 0, 0, 0, 0, 0,
    0, 59, 1016, 65430, 65431, 65432, 65433, 65434, 65435, 65436, 65437, 0, 0, 0, 0, 0,
    0, 122, 2039, 65438, 65439, 65440, 65441, 65442, 65443, 65444, 65445, 0, 0, 0, 0, 0,
    0, 123, 4086, 65446, 65447, 65448, 65449, 65450, 65451, 65452, 65453, 0, 0, 0, 0, 0,
    0, 250, 4087, 65454, 65455, 65456, 65457, 65458, 65459, 65460, 65461, 0, 0, 0, 0, 0,
    0, 504, 32704, 65462, 65463, 65464, 65465, 65466, 65467, 65468, 65469, 0, 0, 0, 0, 0,
    0, 505, 65470, 65471, 65472, 65473, 65474, 65475, 65476, 65477, 65478, 0, 0, 0, 0, 0,
    0, 506, 65479, 65480, 65481, 65482, 65483, 65484, 65485, 65486, 65487, 0, 0, 0, 0, 0,
    0, 1017, 65488, 65489, 65490, 65491, 65492, 65493, 65494, 65495, 65496, 0, 0, 0, 0, 0,
    0, 1018, 65497, 65498, 65499, 65500, 65501, 65502, 65503, 65504, 65505, 0, 0, 0, 0, 0,
    0, 2040, 65506, 65507, 65508, 65509, 65510, 65511, 65512, 65513, 65514, 0, 0, 0, 0, 0,
    0, 65515, 65516, 65517, 65518, 65519, 65520, 65521, 65522, 65523, 65524, 0, 0, 0, 0, 0,
    2041, 65525, 65526, 65527, 65528, 65529, 65530, 65531, 65532, 65533, 65534, 0, 0, 0, 0, 0
};
static const int32_t DCC_L[16] = {
    2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 0, 0, 0, 0
};
static const int32_t DCC_C[16] = {
    0, 1, 2, 6, 14, 30, 62, 126, 254, 510, 1022, 2046, 0, 0, 0, 0
};
static const int32_t ACC_L[256] = {
    2, 2, 3, 4, 5, 5, 6, 7, 9, 10, 12, 0, 0, 0, 0, 0,
    0, 4, 6, 8, 9, 11, 12, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 5, 8, 10, 12, 15, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 5, 8, 10, 12, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 6, 9, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 6, 10, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 7, 11, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 7, 11, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 8, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 9, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 9, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 9, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 9, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 11, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    0, 14, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0,
    10, 15, 16, 16, 16, 16, 16, 16, 16, 16, 16, 0, 0, 0, 0, 0
};
static const int32_t ACC_C[256] = {
    0, 1, 4, 10, 24, 25, 56, 120, 500, 1014, 4084, 0, 0, 0, 0, 0,
    0, 11, 57, 246, 501, 2038, 4085, 65416, 65417, 65418, 65419, 0, 0, 0, 0, 0,
    0, 26, 247, 1015, 4086, 32706, 65420, 65421, 65422, 65423, 65424, 0, 0, 0, 0, 0,
    0, 27, 248, 1016, 4087, 65425, 65426, 65427, 65428, 65429, 65430, 0, 0, 0, 0, 0,
    0, 58, 502, 65431, 65432, 65433, 65434, 65435, 65436, 65437, 65438, 0, 0, 0, 0, 0,
    0, 59, 1017, 65439, 65440, 65441, 65442, 65443, 65444, 65445, 65446, 0, 0, 0, 0, 0,
    0, 121, 2039, 65447, 65448, 65449, 65450, 65451, 65452, 65453, 65454, 0, 0, 0, 0, 0,
    0, 122, 2040, 65455, 65456, 65457, 65458, 65459, 65460, 65461, 65462, 0, 0, 0, 0, 0,
    0, 249, 65463, 65464, 65465, 65466, 65467, 65468, 65469, 65470, 65471, 0, 0, 0, 0, 0,
    0, 503, 65472, 65473, 65474, 65475, 65476, 65477, 65478, 65479, 65480, 0, 0, 0, 0, 0,
    0, 504, 65481, 65482, 65483, 65484, 65485, 65486, 65487, 65488, 65489, 0, 0, 0, 0, 0,
    0, 505, 65490, 65491, 65492, 65493, 65494, 65495, 65496, 65497, 65498, 0, 0, 0, 0, 0,
    0, 506, 65499, 65500, 65501, 65502, 65503, 65504, 65505, 65506, 65507, 0, 0, 0, 0, 0,
    0, 2041, 65508, 65509, 65510, 65511, 65512, 65513, 65514, 65515, 65516, 0, 0, 0, 0, 0,
    0, 16352, 65517, 65518, 65519, 65520, 65521, 65522, 65523, 65524, 65525, 0, 0, 0, 0, 0,
    1018, 32707, 65526, 65527, 65528, 65529, 65530, 65531, 65532, 65533, 65534, 0, 0, 0, 0, 0
};
