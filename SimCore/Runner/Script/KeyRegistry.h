#pragma once
#include <string_view>
#include <cstdint>
#include <cstddef>
#include <string>
#include "KeyIds.h"

namespace simcore::keys {

	std::string_view name_for_id(KeyId id);
	bool id_for_name(std::string_view name, KeyId& out);

	const KeyPair* all_keys(size_t& out_count);
	uint32_t registry_hash();
	bool validate_registry(std::string* err_out = nullptr);

} // namespace simcore::keys
