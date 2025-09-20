#pragma once
#include <cstdint>
#include <vector>
#include <span>
#include "ActionTypes.h"
#include "../../../Runner/IPC/Wire.h"

namespace soa::battle::actions {

    // Encode a full BattlePath into a single payload buffer:
    // Layout: u32 turn_count; repeat turn_count { u32 action_count; action_count * WireActionPlan }
    // Encode a terminal BattlePath (vector<TurnPlan>) into a compact buffer.
    void encode_turn_plans_to_buffer(const actions::BattlePath& path, std::vector<std::uint8_t>& out);

    // Decode a BattlePath from a buffer. Returns false if malformed.
    bool decode_turn_plans_from_buffer(std::span<const std::uint8_t> buf, actions::BattlePath& out);

} // namespace simcore::programs::battle
