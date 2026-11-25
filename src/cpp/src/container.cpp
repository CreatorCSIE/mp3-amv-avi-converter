// 容器移植（逐字节对齐 Python 金标准）：
// - amvenc.split_amv/build_file：AMV 头部/音频 donor + 重编码帧 → 固件布局；
// - aviwriter._packets/_patch_app0/fix_jpeg_layout/remux：AVI 重封装。
// 注意各函数的边界语义与 Python 切片行为一致（越界截断、奇数块不填充等）。
#include "container.h"
#include "container_data.h"

#include <cstring>

namespace {

uint32_t le32(const uint8_t* p) {
    return (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}

void put_le32(std::vector<uint8_t>& v, size_t pos, uint32_t x) {
    v[pos] = (uint8_t)(x & 0xFF);
    v[pos + 1] = (uint8_t)((x >> 8) & 0xFF);
    v[pos + 2] = (uint8_t)((x >> 16) & 0xFF);
    v[pos + 3] = (uint8_t)((x >> 24) & 0xFF);
}

void put_be16(std::vector<uint8_t>& v, size_t pos, uint16_t x) {
    v[pos] = (uint8_t)(x >> 8);
    v[pos + 1] = (uint8_t)(x & 0xFF);
}

// bytes.find：返回首个 fourcc 下标，未找到返 -1
long find_seq(const uint8_t* d, size_t len, const char* s, size_t slen, long from = 0) {
    if ((size_t)from + slen > len) return -1;
    for (size_t i = from; i + slen <= len; i++)
        if (std::memcmp(d + i, s, slen) == 0) return (long)i;
    return -1;
}

long rfind_seq(const uint8_t* d, size_t len, const char* s, size_t slen) {
    for (size_t i = len; i >= slen; i--)
        if (std::memcmp(d + i - slen, s, slen) == 0) return (long)(i - slen);
    return -1;
}

void append_fourcc(std::vector<uint8_t>& v, const char* s) {
    v.insert(v.end(), s, s + 4);
}

void append_le32(std::vector<uint8_t>& v, uint32_t x) {
    v.push_back((uint8_t)(x & 0xFF));
    v.push_back((uint8_t)((x >> 8) & 0xFF));
    v.push_back((uint8_t)((x >> 16) & 0xFF));
    v.push_back((uint8_t)((x >> 24) & 0xFF));
}

void append_idx_rec(std::vector<uint8_t>& idx, const char* fcc, uint32_t flags,
                    uint32_t off, uint32_t size) {
    append_fourcc(idx, fcc);
    append_le32(idx, flags);
    append_le32(idx, off);
    append_le32(idx, size);
}

// 原版帧头模板（SOF0 宽高 patch 后）
std::vector<uint8_t> frame_header(int w, int h) {
    std::vector<uint8_t> hdr(ORIG_FRAME_HDR, ORIG_FRAME_HDR + sizeof(ORIG_FRAME_HDR));
    long pos = -1;
    for (size_t i = 0; i + 2 <= hdr.size(); i++)
        if (hdr[i] == 0xFF && hdr[i + 1] == 0xC0) { pos = (long)i; break; }
    // struct.pack_into(">HH", hdr, pos+5, height, width)
    put_be16(hdr, (size_t)pos + 5, (uint16_t)h);
    put_be16(hdr, (size_t)pos + 7, (uint16_t)w);
    return hdr;
}

const uint8_t kApp0[6] = {0xFF, 0xD8, 0xFF, 0xE0, 0x00, 0x10};

// APP0(JFIF) → APP0(AVI1)，长度不变
std::vector<uint8_t> patch_app0(std::vector<uint8_t> jpeg) {
    if (jpeg.size() >= 20 && std::memcmp(jpeg.data(), kApp0, 6) == 0) {
        std::memcpy(jpeg.data() + 6, "AVI1", 4);
        std::memset(jpeg.data() + 10, 0, 10);
    }
    return jpeg;
}

}  // namespace

bool amv_split(const std::vector<uint8_t>& data, AmvDonor& out, std::string& err) {
    if (data.size() < 12 || std::memcmp(data.data() + 8, "AMV ", 4) != 0) {
        err = "donor 不是 RIFF('AMV ') 容器";
        return false;
    }
    long q = find_seq(data.data(), data.size(), "movi", 4);
    if (q < 0) {
        err = "donor 无 movi 列表";
        return false;
    }
    out.header.assign(data.begin(), data.begin() + q + 4);
    out.auds.clear();
    const size_t len = data.size();
    size_t p = (size_t)q + 4;
    while (p + 8 <= len) {
        const uint8_t* fcc = data.data() + p;
        uint32_t sz = le32(fcc + 4);
        if (std::memcmp(fcc, "01wb", 4) == 0) {
            size_t avail = len - (p + 8);
            size_t n = sz < avail ? sz : avail;  // Python 切片越界截断语义
            out.auds.emplace_back(data.begin() + p + 8, data.begin() + p + 8 + n);
        } else if (std::memcmp(fcc, "00dc", 4) != 0) {
            break;  // 'AMV_END_' 哨兵等尾部
        }
        p += 8 + sz;  // 实测：奇数块不填充
    }
    return true;
}

std::vector<uint8_t> amv_build(const AmvDonor& donor,
                               const std::vector<std::vector<uint8_t>>& frames) {
    std::vector<uint8_t> hdr = donor.header;
    const size_t q = hdr.size() - 4;  // 'movi' fourcc 位置
    std::vector<uint8_t> movi, idx;
    uint32_t off = 4;
    const size_t n = frames.size() < donor.auds.size() ? frames.size() : donor.auds.size();
    for (size_t i = 0; i < n; i++) {
        const auto& f = frames[i];
        const auto& a = donor.auds[i];
        append_fourcc(movi, "00dc");
        append_le32(movi, (uint32_t)f.size());
        movi.insert(movi.end(), f.begin(), f.end());
        append_idx_rec(idx, "00dc", 0x10, off, (uint32_t)f.size());
        off += 8 + (uint32_t)f.size();
        append_fourcc(movi, "01wb");
        append_le32(movi, (uint32_t)a.size());
        movi.insert(movi.end(), a.begin(), a.end());
        append_idx_rec(idx, "01wb", 0x10, off, (uint32_t)a.size());
        off += 8 + (uint32_t)a.size();
    }
    std::vector<uint8_t> tail;
    tail.insert(tail.end(), "AMV_END_", "AMV_END_" + 8);
    append_fourcc(tail, "idx1");
    append_le32(tail, (uint32_t)idx.size());
    tail.insert(tail.end(), idx.begin(), idx.end());

    put_le32(hdr, 4, (uint32_t)(hdr.size() + movi.size() + tail.size() - 8));  // RIFF size
    put_le32(hdr, q - 4, (uint32_t)(4 + movi.size()));                          // movi LIST size

    std::vector<uint8_t> out;
    out.reserve(hdr.size() + movi.size() + tail.size());
    out.insert(out.end(), hdr.begin(), hdr.end());
    out.insert(out.end(), movi.begin(), movi.end());
    out.insert(out.end(), tail.begin(), tail.end());
    return out;
}

bool avi_packets(const std::vector<uint8_t>& data, std::vector<AviPacket>& out, std::string& err) {
    long i = find_seq(data.data(), data.size(), "movi", 4);
    if (i < 0) {
        err = "输入 AVI 无 movi 列表";
        return false;
    }
    out.clear();
    const size_t end = data.size();
    size_t p = (size_t)i + 4;
    while (p + 8 <= end) {
        const uint8_t* cid = data.data() + p;
        uint32_t sz = le32(cid + 4);
        if (std::memcmp(cid, "idx1", 4) == 0 || std::memcmp(cid, "JUNK", 4) == 0) break;
        if ((cid[0] == '0' && (cid[1] == '0' || cid[1] == '1')) && sz <= end - p - 8) {
            AviPacket pk;
            pk.cid.assign((const char*)cid, 4);
            pk.payload.assign(data.begin() + p + 8, data.begin() + p + 8 + sz);
            out.push_back(std::move(pk));
        }
        p += 8 + sz + (sz & 1);
    }
    return true;
}

std::vector<uint8_t> avi_fix_jpeg(const std::vector<uint8_t>& jpeg, int w, int h) {
    const size_t len = jpeg.size();
    if (len < 2 || jpeg[0] != 0xFF || jpeg[1] != 0xD8) return jpeg;
    size_t p = 2;
    while (p + 4 <= len && jpeg[p] == 0xFF) {
        const uint8_t m = jpeg[p + 1];
        if (m == 0xDA) {
            std::vector<uint8_t> out = frame_header(w, h);
            out.insert(out.end(), jpeg.begin() + p, jpeg.end());
            return out;
        }
        const size_t ln = ((size_t)jpeg[p + 2] << 8) | jpeg[p + 3];
        p += 2 + ln;
    }
    return jpeg;
}

std::vector<uint8_t> avi_remux(const std::vector<uint8_t>& raw, int w, int h, std::string& err) {
    std::vector<AviPacket> pkts;
    if (!avi_packets(raw, pkts, err)) return {};
    std::vector<std::vector<uint8_t>> vids, auds;
    for (auto& pk : pkts) {
        if (pk.cid == "00dc") vids.push_back(patch_app0(avi_fix_jpeg(pk.payload, w, h)));
        else if (pk.cid == "01wb") auds.push_back(std::move(pk.payload));
    }
    if (vids.empty() || auds.empty()) {
        err = "ffmpeg 输出缺少 00dc/01wb 包";
        return {};
    }
    const size_t n = vids.size() < auds.size() ? vids.size() : auds.size();  // 严格 1:1

    std::vector<uint8_t> movi, idx;
    uint32_t off = 4;
    uint32_t movi_sz = 0;
    for (size_t i = 0; i < n; i++) {
        const auto& v = vids[i];
        const auto& a = auds[i];
        const uint32_t vp = (uint32_t)v.size() + ((4u - (uint32_t)v.size() % 4u) % 4u);  // 4B 对齐
        append_fourcc(movi, "00dc");
        append_le32(movi, (uint32_t)v.size());  // 块头 size = 真实 JPEG 长（不含 pad）
        movi.insert(movi.end(), v.begin(), v.end());
        movi.insert(movi.end(), vp - (uint32_t)v.size(), 0);
        append_idx_rec(idx, "00dc", 0x10, off, vp);  // idx1 size = 对齐后长度
        off += 8 + vp;
        movi_sz += 8 + vp;
        append_fourcc(movi, "01wb");
        append_le32(movi, (uint32_t)a.size());
        movi.insert(movi.end(), a.begin(), a.end());
        append_idx_rec(idx, "01wb", 0x10, off, (uint32_t)a.size());
        off += 8 + (uint32_t)a.size();
        movi_sz += 8 + (uint32_t)a.size();
    }
    std::vector<uint8_t> idx1;
    append_fourcc(idx1, "idx1");
    append_le32(idx1, (uint32_t)(16 * idx.size() / 16));
    idx1.insert(idx1.end(), idx.begin(), idx.end());

    std::vector<uint8_t> hdr(ORIG_HDR, ORIG_HDR + sizeof(ORIG_HDR));
    long avih = find_seq(hdr.data(), hdr.size(), "avih", 4);
    put_le32(hdr, (size_t)avih + 8 + 4, (uint32_t)n);  // dwTotalFrames
    long auds_pos = find_seq(hdr.data(), hdr.size(), "auds", 4);
    put_le32(hdr, (size_t)auds_pos + 32, (uint32_t)(n * 258));  // 原版 dwLength 约定
    long movi_pos = rfind_seq(hdr.data(), hdr.size(), "movi", 4);
    put_le32(hdr, (size_t)movi_pos - 4, 4 + movi_sz);  // movi LIST size

    // out = hdr + movi + idx1；再 "RIFF" + le32(len-8) + out[8:]
    const size_t total = hdr.size() + movi.size() + idx1.size();
    std::vector<uint8_t> out;
    out.reserve(total);
    append_fourcc(out, "RIFF");
    append_le32(out, (uint32_t)(total - 8));
    out.insert(out.end(), hdr.begin() + 8, hdr.end());
    out.insert(out.end(), movi.begin(), movi.end());
    out.insert(out.end(), idx1.begin(), idx1.end());
    return out;
}
