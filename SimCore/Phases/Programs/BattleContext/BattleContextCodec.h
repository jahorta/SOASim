#pragma once
#include <string>
#include "../../../Core/Memory/MemView.h"
#include "BattleContext.h"

namespace simcore::battlectx::codec {

	// Extracts the full context from a MEM1 snapshot (fills materialized structs)
	bool extract_from_mem1(const simcore::MemView& view, BattleContext& out);

	// Encodes/decodes the rich context (no version bump; v1 is now materialized)
	bool encode(const BattleContext& in, std::string& out);
	bool decode(std::string_view in, BattleContext& out);

} // namespace simcore::battlectx::codec
