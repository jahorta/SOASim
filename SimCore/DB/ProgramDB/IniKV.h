// SimCore/DB/ProgramDB/IniKV.h
#pragma once
#include <string>
#include <vector>
#include <algorithm>

struct IniKV {
    std::vector<std::pair<std::string, std::string>> kv;

    void add(std::string k, std::string v) { kv.emplace_back(std::move(k), std::move(v)); }
    std::string to_string_sorted() const {
        std::vector<std::pair<std::string, std::string>> c = kv;
        std::sort(c.begin(), c.end(), [](auto& a, auto& b) { return a.first < b.first; });
        std::string s;
        for (auto& e : c) { s.append(e.first); s.push_back('='); s.append(e.second); s.push_back('\n'); }
        return s;
    }
    static std::vector<std::pair<std::string, std::string>> parse(const std::string& text) {
        std::vector<std::pair<std::string, std::string>> out;
        size_t i = 0, n = text.size();
        while (i < n) {
            size_t j = text.find('\n', i);
            size_t e = (j == std::string::npos) ? n : j;
            if (e > i) {
                size_t eq = text.find('=', i);
                if (eq != std::string::npos && eq < e) {
                    out.emplace_back(text.substr(i, eq - i), text.substr(eq + 1, e - eq - 1));
                }
            }
            if (j == std::string::npos) break; else i = j + 1;
        }
        return out;
    }
};
