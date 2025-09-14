#pragma once
#include <cstdint>
#include <cstring>
#include "../../Runner/Memory/MemView.h"

namespace soa::readers {

	// Copy a struct image verbatim from MEM1 into host memory.
	// (No field-wise swapping yet; safe & deterministic snapshot.)
	template <class T>
	inline bool read_raw(const simcore::MemView& view, uint32_t va, T& out) {
		if (!view.valid() || !view.in_mem1(va)) return false;
		return view.read_block(va, &out, sizeof(T));
	}

	// If/when you want to move to host-order fields, add a fix_endianness_in_place(T&) overload
	// and call it after read_raw(). We keep this stub here to minimize churn later.
	template <class T>
	inline void fix_endianness_in_place(T&) {
		// intentionally empty for now — structs currently carry a faithful byte image
	}

	template <class T>
	inline bool read(const simcore::MemView& view, uint32_t va, T& out) {
		if (!read_raw(view, va, out)) return false;
		fix_endianness_in_place(out);
		return true;
	}

} // namespace soa::readers
