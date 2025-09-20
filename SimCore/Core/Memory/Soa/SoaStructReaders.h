#pragma once
#include <cstdint>
#include <cstring>
#include "../MemView.h"
#include "../Endian.h"

namespace soa::readers {

	// Copy a struct image verbatim from MEM1 into host memory.
	// (No field-wise swapping yet; safe & deterministic snapshot.)
	template <class T>
	inline bool read_raw(const simcore::MemView& view, uint32_t va, T& out) {
		if (!view.valid() || !view.in_mem1(va)) return false;
		return view.read_block(va, &out, sizeof(T));
	}

	template <class T>
	inline bool read(const simcore::MemView& view, uint32_t va, T& out) {
		if (!read_raw(view, va, out)) return false;
		simcore::endian::fix_endianness_in_place(out);
		return true;
	}

} // namespace soa::readers
