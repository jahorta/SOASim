#pragma once
#include "../Input/InputPlan.h"
#include <cstdint>
#include <optional>
#include <utility>
#include <vector>
#include <functional>
#include <string>

namespace simcore {

    // Multiple input choices at specific absolute frames [0..total_frames)
    struct DecisionPoint {
        uint32_t frame_index;                // unique, ascending
        std::vector<GCInputFrame> options;   // >=1 choices
    };

    // Description of a branching run
    struct BranchSpec {
        uint32_t total_frames = 0;
        GCInputFrame default_frame{};        // used on frames without a decision
        std::vector<DecisionPoint> decisions; // sorted by frame_index, unique
    };

    // A single concrete expansion (materialized)
    struct BranchInstance {
        InputPlan plan; // length == total_frames
        // (frame_index, option_index) for debugging/logging/result keys
        std::vector<std::pair<uint32_t, size_t>> chosen;
    };

    // Lazy DFS enumerator over the Cartesian product of decision options.
    class BranchExplorer {
    public:
        explicit BranchExplorer(BranchSpec spec);

        std::optional<BranchInstance> next();

        void reset();

        using PerFrameCallback = std::function<bool(uint32_t frame_idx, const GCInputFrame& frame)>;
        size_t for_each_branch(const PerFrameCallback& cb);

    private:
        BranchSpec m_spec;

        // Mixed-radix driver
        size_t m_index = 0;                  // current combination [0..m_total)
        size_t m_total = 0;                  // product of option counts (or 1 if no decisions)
        std::vector<size_t> m_radix;         // weights for each digit: product of sizes to the right


        static bool is_sorted_unique(const std::vector<DecisionPoint>& d);
        static std::string validate_spec(const BranchSpec& s);

        BranchInstance materialize_with_indices(const std::vector<size_t>& choice_idx) const;
    };

} // namespace simcore
