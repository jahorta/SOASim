// SimCore/DB/ProgramDB/Fingerprint.h
#pragma once
#include <string>
#include <vector>
#include "mbedtls/sha256.h"

// Canonical: program_kind + program_version + blueprint
inline std::string make_job_fingerprint(int program_kind, int program_version, const std::string& blueprint_ini) {
    std::string buf;
    buf.append(reinterpret_cast<const char*>(&program_kind), sizeof(program_kind));
    buf.append(reinterpret_cast<const char*>(&program_version), sizeof(program_version));
    buf.append(blueprint_ini);
    unsigned char out[32];
    if (!mbedtls_sha256_ret(reinterpret_cast<const unsigned char *>(buf.c_str()), buf.size(), out, 0)) return "";
    return std::string(reinterpret_cast<char*>(out), 32);
}
