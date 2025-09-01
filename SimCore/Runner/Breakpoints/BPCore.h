#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>
#include <fstream>

using BPKey = uint16_t;

struct BPAddr
{
    BPKey key;
    uint32_t pc;
    const char* name;
};

struct BreakpointMap
{
    std::vector<BPAddr> addrs;
    BPKey start_key{ 0 };
    BPKey terminal_key{ 0 };

    std::optional<BPKey> match(uint32_t pc) const
    {
        for (const auto& a : addrs)
            if (a.pc == pc) return a.key;
        return std::nullopt;
    }

    const char* label(BPKey k) const
    {
        for (const auto& a : addrs)
            if (a.key == k) return a.name;
        return "??";
    }

    bool set_pc(BPKey k, uint32_t new_pc)
    {
        for (auto& a : addrs)
        {
            if (a.key == k) { a.pc = new_pc; return true; }
        }
        return false;
    }

    const BPAddr* find(BPKey k) const {
        for (const auto& e : addrs) if (e.key == k) return &e;
        return nullptr;
    }

    inline std::vector<BPKey> get_keys() {
        std::vector<BPKey> out;
        for (auto e : addrs) out.emplace_back(e.key);
        return out;
    }
};

// helpers (header-only) for tiny config parsing
inline std::string bp_trim(std::string s)
{
    size_t i = 0, j = s.size();
    while (i < j && (unsigned char)s[i] <= ' ') ++i;
    while (j > i && (unsigned char)s[j - 1] <= ' ') --j;
    return s.substr(i, j - i);
}

inline std::optional<uint32_t> bp_parse_u32(const std::string& s)
{
    if (s.empty()) return std::nullopt;
    char* end = nullptr;
    unsigned long v = 0;
    if (s.size() > 2 && s[0] == '0' && (s[1] == 'x' || s[1] == 'X'))
        v = std::strtoul(s.c_str() + 2, &end, 16);
    else
        v = std::strtoul(s.c_str(), &end, 10);
    if (!end || *end != '\0') return std::nullopt;
    if (v > 0xFFFFFFFFul) return std::nullopt;
    return static_cast<uint32_t>(v);
}

inline BreakpointMap load_bpmap_file(const std::string& path, BreakpointMap base)
{
    if (path.empty()) return base;

    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line))
    {
        auto t = bp_trim(line);
        if (t.empty() || t[0] == '#' || t[0] == ';') continue;

        auto eq = t.find('=');
        if (eq == std::string::npos) continue;

        auto key = bp_trim(t.substr(0, eq));
        auto val = bp_trim(t.substr(eq + 1));
        auto v = bp_parse_u32(val);
        if (!v) continue;

        for (auto& a : base.addrs)
        {
            if (key == a.name)
            {
                a.pc = *v;
                break;
            }
        }
    }
    return base;
}