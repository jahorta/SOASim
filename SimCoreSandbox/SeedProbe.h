#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <functional>

#include "Core/Input/InputPlan.h" // for GCInputFrame
#include <Phases/RNGSeedDeltaMap.h>

namespace sandbox {

    void print_family_grid(const simcore::RandSeedProbeResult& r, simcore::SeedFamily fam, int N, const char* title);
    void log_probe_summary(const simcore::RandSeedProbeResult& r);
    std::vector<std::string> to_csv_lines(const simcore::RandSeedProbeResult& r);

} // namespace simcore
