#include "Fingerprint.h"
#include <stdint.h>
#include <string.h>
#include <string>

// Compact public-domain SHA-256 (small, single-file)
namespace {
    static const uint32_t K[64] = {
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2 };
    inline uint32_t ROR(uint32_t x, uint32_t n) { return (x >> n) | (x << (32 - n)); }
    inline uint32_t Ch(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (~x & z); }
    inline uint32_t Maj(uint32_t x, uint32_t y, uint32_t z) { return (x & y) ^ (x & z) ^ (y & z); }
    inline uint32_t S0(uint32_t x) { return ROR(x, 2) ^ ROR(x, 13) ^ ROR(x, 22); }
    inline uint32_t S1(uint32_t x) { return ROR(x, 6) ^ ROR(x, 11) ^ ROR(x, 25); }
    inline uint32_t s0(uint32_t x) { return ROR(x, 7) ^ ROR(x, 18) ^ (x >> 3); }
    inline uint32_t s1(uint32_t x) { return ROR(x, 17) ^ ROR(x, 19) ^ (x >> 10); }

    struct SHA256 {
        uint64_t len = 0; uint8_t buf[64]; size_t idx = 0;
        uint32_t H[8] = { 0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19 };
        void process(const uint8_t* b) {
            uint32_t w[64]; for (int i = 0; i < 16; i++) { w[i] = (b[4 * i] << 24) | (b[4 * i + 1] << 16) | (b[4 * i + 2] << 8) | b[4 * i + 3]; }
            for (int i = 16; i < 64; i++) w[i] = s1(w[i - 2]) + w[i - 7] + s0(w[i - 15]) + w[i - 16];
            uint32_t a = H[0], b0 = H[1], c = H[2], d = H[3], e = H[4], f = H[5], g = H[6], h = H[7];
            for (int i = 0; i < 64; i++) { uint32_t T1 = h + S1(e) + Ch(e, f, g) + K[i] + w[i]; uint32_t T2 = S0(a) + Maj(a, b0, c); h = g; g = f; f = e; e = d + T1; d = c; c = b0; b0 = a; a = T1 + T2; }
            H[0] += a; H[1] += b0; H[2] += c; H[3] += d; H[4] += e; H[5] += f; H[6] += g; H[7] += h;
        }
        void update(const void* data, size_t n) {
            const uint8_t* p = (const uint8_t*)data; len += n;
            while (n) { size_t t = 64 - idx; size_t take = t < n ? t : n; memcpy(buf + idx, p, take); idx += take; p += take; n -= take; if (idx == 64) { process(buf); idx = 0; } }
        }
        void finish(uint8_t out[32]) {
            uint64_t bitlen = len * 8; buf[idx++] = 0x80;
            if (idx > 56) { while (idx < 64) buf[idx++] = 0; process(buf); idx = 0; }
            while (idx < 56) buf[idx++] = 0;
            for (int i = 7; i >= 0; i--) buf[idx++] = ((uint8_t*)&bitlen)[i];
            process(buf);
            for (int i = 0; i < 8; i++) { out[4 * i] = H[i] >> 24; out[4 * i + 1] = (H[i] >> 16) & 0xff; out[4 * i + 2] = (H[i] >> 8) & 0xff; out[4 * i + 3] = H[i] & 0xff; }
        }
    };
}

std::string compute_sha256_hex(const void* data, size_t len) {
    SHA256 s; s.update(data, len); uint8_t d[32]; s.finish(d);
    static const char* hex = "0123456789abcdef";
    std::string out; out.resize(64);
    for (int i = 0; i < 32; i++) { out[2 * i] = hex[(d[i] >> 4) & 0xF]; out[2 * i + 1] = hex[d[i] & 0xF]; }
    return out;
}
