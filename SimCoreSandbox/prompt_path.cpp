#include <iostream>
#include <string>
#include <filesystem>

namespace fs = std::filesystem;

static std::string trim(std::string s) {
    auto ws = [](unsigned char c) { return std::isspace(c); };
    while (!s.empty() && ws(s.front())) s.erase(s.begin());
    while (!s.empty() && ws(s.back()))  s.pop_back();
    return s;
}

static std::string strip_quotes(std::string s) {
    if (s.size() >= 2) {
        if ((s.front() == '"' && s.back() == '"') ||
            (s.front() == '\'' && s.back() == '\''))
            return s.substr(1, s.size() - 2);
    }
    return s;
}

static fs::path prompt_path(const char* prompt, bool require_exists, bool allow_empty) {
    for (;;) {
        std::cout << prompt;
        std::string line;
        if (!std::getline(std::cin, line)) return {};
        line = trim(strip_quotes(line));
        if (line.empty()) {
            if (allow_empty) return {};
            continue;
        }
        fs::path p = fs::path(line).lexically_normal();
        if (require_exists && !fs::exists(p)) {
            std::cout << "Not found, try again.\n";
            continue;
        }
        return p;
    }
}
