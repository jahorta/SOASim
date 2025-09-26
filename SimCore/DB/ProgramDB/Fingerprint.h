// SimCore/DB/ProgramDB/Fingerprint.h
#pragma once
#include <string>
#include <vector>

// Provide a real SHA-256 in implementation (reuse any existing util if you have one).
std::string compute_sha256_hex(const void* data, size_t len);

// Canonical: program_kind + program_version + blueprint
inline std::string make_job_fingerprint(int program_kind, int program_version, const std::string& blueprint_ini) {
    std::string buf;
    buf.append(reinterpret_cast<const char*>(&program_kind), sizeof(program_kind));
    buf.append(reinterpret_cast<const char*>(&program_version), sizeof(program_version));
    buf.append(blueprint_ini);
    return compute_sha256_hex(buf.data(), buf.size());
}
