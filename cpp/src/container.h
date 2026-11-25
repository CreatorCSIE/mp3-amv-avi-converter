#pragma once
#include <cstdint>
#include <string>
#include <vector>

// 容器移植：amvenc.py split_amv/build_file + aviwriter.py _packets/fix_jpeg_layout/remux。
// 纯字节机械操作，无浮点，跨平台逐位一致。

// ---- AMV（donor = ffmpeg amv muxer 产物）----
struct AmvDonor {
    std::vector<uint8_t> header;                  // 头部区，含 'movi' fourcc（尾部 4B）
    std::vector<std::vector<uint8_t>> auds;       // 01wb 音频块 payload（376B IMA-AMV）
};

// 从 donor 提取头部区 + 音频块列表（视频块丢弃，由重编码替换）。失败置 err。
bool amv_split(const std::vector<uint8_t>& data, AmvDonor& out, std::string& err);

// 头部/音频 + 重编码视频帧（1:1 交织取 min）→ 完整 AMV 文件字节。
// 尾部 'AMV_END_' 哨兵 + idx1；RIFF/movi size 回填。
std::vector<uint8_t> amv_build(const AmvDonor& donor,
                               const std::vector<std::vector<uint8_t>>& frames);

// ---- AVI（raw = ffmpeg 标准 AVI）----
struct AviPacket {
    std::string cid;                  // fourcc，如 "00dc"/"01wb"
    std::vector<uint8_t> payload;
};

// 从 movi 提取包序列（遇 idx1/JUNK 停；奇数块按 1B 填充跳过）。无 movi 置 err 返 false。
bool avi_packets(const std::vector<uint8_t>& data, std::vector<AviPacket>& out, std::string& err);

// 现代化 ffmpeg 帧 → 原版帧：SOI..SOS 前整段替换为模板（SOF0 patch 宽高），保留扫描数据。
std::vector<uint8_t> avi_fix_jpeg(const std::vector<uint8_t>& jpeg, int w, int h);

// 标准 AVI → 固件原版布局（头部模板 + 帧头替换 + 4B 对齐 + idx1）。失败置 err 返空。
std::vector<uint8_t> avi_remux(const std::vector<uint8_t>& raw, int w, int h, std::string& err);
