#include "Branching.h"
#include <algorithm>
#include <cassert>
#include <stdexcept>

namespace simcore {

    static uint32_t max_decision_frame(const std::vector<DecisionPoint>& d) {
        uint32_t m = 0;
        for (const auto& dp : d) m = std::max(m, dp.frame_index);
        return m;
    }

    bool BranchExplorer::is_sorted_unique(const std::vector<DecisionPoint>& d) {
        for (size_t i = 1; i < d.size(); ++i)
            if (d[i - 1].frame_index >= d[i].frame_index) return false;
        return true;
    }

    std::string BranchExplorer::validate_spec(const BranchSpec& s) {
        if (!is_sorted_unique(s.decisions))
            return "DecisionPoint.frame_index must be strictly increasing.";
        for (const auto& dp : s.decisions)
            if (dp.options.empty())
                return "DecisionPoint has no options.";
        if (!s.decisions.empty()) {
            const uint32_t last = max_decision_frame(s.decisions);
            if (s.total_frames == 0 || last >= s.total_frames)
                return "total_frames must be > max(decision.frame_index).";
        }
        return {};
    }

    BranchExplorer::BranchExplorer(BranchSpec spec) : m_spec(std::move(spec)) {
        if (auto err = validate_spec(m_spec); !err.empty())
            throw std::invalid_argument("BranchSpec invalid: " + err);

        // Build mixed-radix weights and total count.
        const size_t n = m_spec.decisions.size();
        m_radix.assign(n, 1);
        m_total = 1;
        // From right to left: radix[i] = product of sizes to the right.
        for (size_t i = n; i-- > 0; ) {
            m_radix[i] = (i + 1 < n) ? (m_radix[i + 1] * m_spec.decisions[i + 1].options.size()) : 1;
            m_total *= m_spec.decisions[i].options.size();
        }
        if (n == 0) { // one trivial combination (all defaults)
            m_total = 1;
            m_radix.clear();
        }
        m_index = 0;
    }

    void BranchExplorer::reset() {
        m_index = 0;
    }

    BranchInstance BranchExplorer::materialize_with_indices(const std::vector<size_t>& choice_idx) const {
        BranchInstance bi;
        bi.plan.assign(m_spec.total_frames, m_spec.default_frame);
        bi.chosen.reserve(m_spec.decisions.size());

        for (size_t i = 0; i < m_spec.decisions.size(); ++i) {
            const auto& dec = m_spec.decisions[i];
            const size_t opt = choice_idx[i];
            assert(opt < dec.options.size());
            assert(dec.frame_index < bi.plan.size());
            bi.plan[dec.frame_index] = dec.options[opt];
            bi.chosen.emplace_back(dec.frame_index, opt);
        }
        return bi;
    }

    std::optional<BranchInstance> BranchExplorer::next() {
        if (m_index >= m_total) return std::nullopt;

        // Derive current digits from m_index (mixed radix).
        std::vector<size_t> choice_idx;
        choice_idx.resize(m_spec.decisions.size(), 0);

        for (size_t i = 0; i < m_spec.decisions.size(); ++i) {
            const size_t base = m_spec.decisions[i].options.size();
            const size_t weight = (i + 1 < m_radix.size()) ? m_radix[i] : 1;
            // If there are no decisions, the loop doesn't run.
            choice_idx[i] = (base == 0) ? 0 : (m_index / weight) % base;
        }

        BranchInstance bi = materialize_with_indices(choice_idx);
        ++m_index; // move to the next combination
        return bi;
    }

    size_t BranchExplorer::for_each_branch(const PerFrameCallback& cb) {
        size_t visited = 0;
        reset();
        while (auto inst = next()) {
            ++visited;
            for (uint32_t f = 0; f < m_spec.total_frames; ++f) {
                if (!cb(f, inst->plan[f])) break;
            }
        }
        return visited;
    }

} // namespace simcore
