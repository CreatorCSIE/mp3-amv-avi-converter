#pragma once
// 极简 JSON 解析（仅够 ffprobe -print_format json 的取值需求；无 unicode 转义还原）
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

struct JVal {
    enum T { NUL, BOOL, NUM, STR, ARR, OBJ } t = NUL;
    double num = 0;
    std::string str;
    std::vector<JVal> arr;
    std::vector<std::pair<std::string, JVal>> obj;

    const JVal* find(const std::string& key) const {
        for (auto& kv : obj) if (kv.first == key) return &kv.second;
        return nullptr;
    }
    std::string as_str() const {
        if (t == STR) return str;
        if (t == NUM) { char b[32]; snprintf(b, sizeof b, "%g", num); return b; }
        return "";
    }
    double as_num() const {
        if (t == NUM) return num;
        if (t == STR) return strtod(str.c_str(), nullptr);
        return 0;
    }
    bool truthy() const { return t == BOOL ? num != 0 : t != NUL; }
};

namespace jsondetail {
inline void skip_ws(const std::string& s, size_t& p) {
    while (p < s.size() && (unsigned char)s[p] <= ' ') p++;
}
inline bool parse_val(const std::string& s, size_t& p, JVal& out);

inline bool parse_str(const std::string& s, size_t& p, std::string& out) {
    if (p >= s.size() || s[p] != '"') return false;
    p++;
    out.clear();
    while (p < s.size()) {
        char c = s[p++];
        if (c == '"') return true;
        if (c == '\\') {
            if (p >= s.size()) return false;
            char e = s[p++];
            switch (e) {
                case 'n': out.push_back('\n'); break;
                case 't': out.push_back('\t'); break;
                case 'r': out.push_back('\r'); break;
                case 'b': out.push_back('\b'); break;
                case 'f': out.push_back('\f'); break;
                case 'u': if (p + 4 <= s.size()) p += 4; out.push_back('?'); break;
                default: out.push_back(e);
            }
        } else out.push_back(c);
    }
    return false;
}

inline bool parse_val(const std::string& s, size_t& p, JVal& out) {
    skip_ws(s, p);
    if (p >= s.size()) return false;
    char c = s[p];
    if (c == '{') {
        p++;
        out.t = JVal::OBJ;
        skip_ws(s, p);
        if (p < s.size() && s[p] == '}') { p++; return true; }
        for (;;) {
            skip_ws(s, p);
            std::string k;
            if (!parse_str(s, p, k)) return false;
            skip_ws(s, p);
            if (p >= s.size() || s[p] != ':') return false;
            p++;
            JVal v;
            if (!parse_val(s, p, v)) return false;
            out.obj.emplace_back(std::move(k), std::move(v));
            skip_ws(s, p);
            if (p < s.size() && s[p] == ',') { p++; continue; }
            if (p < s.size() && s[p] == '}') { p++; return true; }
            return false;
        }
    }
    if (c == '[') {
        p++;
        out.t = JVal::ARR;
        skip_ws(s, p);
        if (p < s.size() && s[p] == ']') { p++; return true; }
        for (;;) {
            JVal v;
            if (!parse_val(s, p, v)) return false;
            out.arr.push_back(std::move(v));
            skip_ws(s, p);
            if (p < s.size() && s[p] == ',') { p++; continue; }
            if (p < s.size() && s[p] == ']') { p++; return true; }
            return false;
        }
    }
    if (c == '"') {
        out.t = JVal::STR;
        return parse_str(s, p, out.str);
    }
    if (s.compare(p, 4, "true") == 0) { p += 4; out.t = JVal::BOOL; out.num = 1; return true; }
    if (s.compare(p, 5, "false") == 0) { p += 5; out.t = JVal::BOOL; out.num = 0; return true; }
    if (s.compare(p, 4, "null") == 0) { p += 4; out.t = JVal::NUL; return true; }
    size_t st = p;
    while (p < s.size() && (isdigit((unsigned char)s[p]) || s[p] == '-' || s[p] == '+' ||
                            s[p] == '.' || s[p] == 'e' || s[p] == 'E')) p++;
    if (p == st) return false;
    out.t = JVal::NUM;
    out.num = strtod(s.substr(st, p - st).c_str(), nullptr);
    return true;
}
}  // namespace jsondetail

inline bool json_parse(const std::string& s, JVal& out) {
    size_t p = 0;
    return jsondetail::parse_val(s, p, out);
}
