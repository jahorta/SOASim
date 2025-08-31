#include "PredicateCatalog.h"

PredicateCatalog::PredicateCatalog(std::vector<PredicateDef> base) : defs_(std::move(base)) {
    for (size_t i = 0; i < defs_.size(); ++i) idx_[defs_[i].key] = i;
}

std::vector<PredicateDef> PredicateCatalog::select(const std::vector<std::string>& keys) const {
    std::vector<PredicateDef> out; out.reserve(keys.size());
    for (const auto& k : keys) {
        auto it = idx_.find(k);
        if (it != idx_.end()) out.push_back(defs_[it->second]);
    }
    return out;
}