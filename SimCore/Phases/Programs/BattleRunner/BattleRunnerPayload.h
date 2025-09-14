#pragma once
#include <vector>
#include <cstdint>
#include <string>
#include "../../../Runner/Script/PhaseScriptVM.h"
#include "../../../Runner/Script/VMCoreKeys.reg.h"
#include "BattleRunnerKeys.reg.h"

namespace simcore::battle {

    // 24-byte record
#pragma pack(push,1)
    struct PredicateRecord {
        uint16_t id;
        uint16_t required_bp;
        uint8_t  kind;    // 0 ABS, 1 DELTA
        uint8_t  width;   // 1,2,4,8  (8 = f64)
        uint8_t  cmp;     // 0==,1!=,2<,3<=,4>,5>=
        uint8_t  flags;   // bit0 = capture baseline at turn start
        uint32_t addr;
        uint64_t rhs;     // interpret by width/kind
    };
#pragma pack(pop)

    // encode side (parent)
    struct EncodeSpec {
        uint32_t run_ms{ 0 };
        uint32_t vi_stall_ms{ 0 };
        GCInputFrame initial{};
        std::vector<std::vector<GCInputFrame>> plans; // one vector per allowed turn
        std::vector<PredicateRecord> predicates;
    };

    // ProgramRegistry.decode -> fill ctx
    bool decode_payload(const std::vector<uint8_t>& in, PSContext& out_ctx);

    // parent helper
    bool encode_payload(const EncodeSpec& spec, std::vector<uint8_t>& out);

} // namespace simcore::battle
