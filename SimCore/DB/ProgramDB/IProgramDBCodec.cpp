#include "IProgramDBCodec.h"
#include <unordered_map>
static std::unordered_map<int, IProgramDBCodec*>* g_map;
void ProgramDBCodecRegistry::register_codec(int program_kind, IProgramDBCodec* impl) {
    if (!g_map) g_map = new std::unordered_map<int, IProgramDBCodec*>();
    (*g_map)[program_kind] = impl;
}
IProgramDBCodec& ProgramDBCodecRegistry::for_kind(int program_kind) {
    return *(*g_map)[program_kind];
}
