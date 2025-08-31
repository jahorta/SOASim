#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include "../../Core/DolphinWrapper.h"
#include "../Breakpoints/BPCore.h"

#pragma once
#include <string>
#include <unordered_map>
#include <vector>
#include <functional>
#include <variant>
#include "../Breakpoints/BPCore.h"

// Declared reads per predicate
enum class MemType { U8, U16, U32, F32, F64 };
struct MemRead { std::string key; uint32_t addr; MemType type; };

// Values passed into predicate at eval time
using PredValue = std::variant<uint8_t, uint16_t, uint32_t, float, double>;
using PredValues = std::unordered_map<std::string, PredValue>;
using Metrics = std::unordered_map<std::string, int64_t>;

// Host-independent predicate, tied to exactly one breakpoint key
using PredicateFn = std::function<bool(const PredValues&, Metrics&)>;

struct PredicateDef {
	std::string key;        // unique id
	BPKey bp;               // which canonical BP this predicate listens for
	std::vector<MemRead> reads;
	bool enabled_by_default{ true };
	PredicateFn fn;         // returns success/fail (no Undecided)
};

class PredicateCatalog {
public:
	explicit PredicateCatalog(std::vector<PredicateDef> base);
	const std::vector<PredicateDef>& all() const { return defs_; }
	std::vector<PredicateDef> select(const std::vector<std::string>& keys) const;
private:
	std::vector<PredicateDef> defs_;
	std::unordered_map<std::string, size_t> idx_;
};