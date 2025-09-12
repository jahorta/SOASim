// SimCore/IPC/Wire.h
#pragma once
#include <cstdint>
#include "../../Core/Input/InputPlan.h"

#pragma once
#include <cstdint>

namespace simcore {

    enum : uint32_t {
        MSG_READY = 0x01,
        MSG_JOB = 0x02,
        MSG_RESULT = 0x03,
        // control-mode (program lifecycle):
        MSG_SET_PROGRAM = 0x10,
        MSG_RUN_INIT_ONCE = 0x11,
        MSG_ACTIVATE_MAIN = 0x12,
        MSG_ACK = 0x13
    };

    enum : uint32_t {
        WERR_None = 0,
        WERR_SysMissing = 1,
        WERR_BootFail = 2,
        WERR_LoadGame = 3,
        WERR_VMInit = 4,
        WERR_NoProgramLoaded = 5,
        WERR_DecodePayloadFail = 7,
        WERR_EncodePayloadFail = 8,
    };

    enum : uint8_t {
        WSTATE_NoProgram = 0,
        WSTATE_ProgramReady = 1
    };

    enum : uint8_t {
        PK_None = 0,
        PK_SeedProbe = 1,
        PK_TasMovie = 2,
    };

    // Payload used for TAS jobs (paths are NUL-terminated, Windows MAX_PATH safe)
    struct TasJobPayload {
        char dtm_path[260];
        char save_dir[260];
        uint32_t run_until_ms;
    };

    struct WireReady {
        uint32_t tag;   // MSG_READY
        uint8_t  ok;    // 1=ok
        uint8_t  state; // WSTATE_*
        uint16_t _pad;
        uint32_t error; // WERR_*
    };

    struct WireResult {
        uint32_t tag;   // MSG_RESULT
        uint64_t job_id;
        uint32_t epoch;
        uint8_t  ok;
        uint8_t  _pad0[3];
        uint32_t last_pc;
        uint32_t seed;  // optional (0 if absent)
    };

    struct WireSetProgram {
        uint32_t tag;         // MSG_SET_PROGRAM
        uint8_t  init_kind;   // PK_*
        uint8_t  main_kind;   // PK_*
        uint16_t _pad0;
        uint32_t timeout_ms;
        char     savestate_path[260]; // empty => start from boot
    };

    struct WireAck {
        uint32_t tag; // MSG_ACK
        uint8_t  ok;  // 1=ok
        uint8_t  code;// 'S' (set), 'I' (init done), 'A' (main active)
        uint16_t _pad0;
    };

    struct WireJobHeader {
        uint32_t tag;      // MSG_JOB
        uint64_t job_id;
        uint32_t epoch;
        uint32_t payload_len;  // number of bytes that follow immediately
    };

} // namespace simcore
