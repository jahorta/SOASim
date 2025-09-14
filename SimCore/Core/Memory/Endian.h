#pragma once
#include <cstdint>
#include <cstring>
#include <type_traits>

namespace simcore::endian {

    // byte swaps (constexpr-friendly)
    constexpr inline uint16_t bswap16(uint16_t v) { return uint16_t((v >> 8) | (v << 8)); }

    constexpr inline uint32_t bswap32(uint32_t v) {
        return (v >> 24) |
            ((v >> 8) & 0x0000FF00u) |
            ((v << 8) & 0x00FF0000u) |
            (v << 24);
    }

    constexpr inline uint64_t bswap64(uint64_t v) {
        return (v >> 56) |
            ((v >> 40) & 0x000000000000FF00ull) |
            ((v >> 24) & 0x0000000000FF0000ull) |
            ((v >> 8) & 0x00000000FF000000ull) |
            ((v << 8) & 0x000000FF00000000ull) |
            ((v << 24) & 0x0000FF0000000000ull) |
            ((v << 40) & 0x00FF000000000000ull) |
            (v << 56);
    }

    // swap helpers for floats without UB
    inline float bswapf(float f) {
        uint32_t u; std::memcpy(&u, &f, 4); u = bswap32(u); std::memcpy(&f, &u, 4); return f;
    }
    inline double bswapd(double d) {
        uint64_t u; std::memcpy(&u, &d, 8); u = bswap64(u); std::memcpy(&d, &u, 8); return d;
    }

} // namespace soa::endian
