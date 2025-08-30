#include <gtest/gtest.h>
#include "Runner/RunEvaluator.h"
#include "Runner/Breakpoints/BattleBreakpoints.h"

using namespace simcore;

struct FakeRunnerPhase : IDolphinRunner
{
    std::vector<uint32_t> pcs;
    size_t i{ 0 };
    uint32_t armed_hit_pc{ 0 };
    bool step_one_frame() override { if (i < pcs.size()) ++i; return true; }
    uint32_t get_pc() const override { return (i == 0) ? 0u : pcs[i - 1]; }
    void set_next_input(const simcore::GCInputFrame&) override {}
    bool read_u8(uint32_t, uint8_t&) const override { return false; }
    bool read_u16(uint32_t, uint16_t&) const override { return false; }
    bool read_u32(uint32_t, uint32_t&) const override { return false; }
    bool read_f32(uint32_t, float&) const override { return false; }
    bool read_f64(uint32_t, double&) const override { return false; }

    bool arm_pc_breakpoints(const std::vector<uint32_t>& pcs_) override { if (!pcs_.empty()) armed_hit_pc = pcs_[0]; return true; }
    RunUntilHitResult run_until_breakpoint_blocking(uint32_t) override {
        if (armed_hit_pc) return { true, armed_hit_pc, "breakpoint" };
        return { false, 0u, "timeout" };
    }
};

TEST(Phases, InputsThenBP)
{
    auto base = battle::defaults();
    FakeRunnerPhase host;
    RunEvaluator eval{ host, base, {}, nullptr };
    SimulationResult res{};
    std::vector<simcore::GCInputFrame> frames(3);

    auto d1 = eval.run_inputs(frames, res);
    EXPECT_EQ(d1.next, SimPhase::UntilBP);
    EXPECT_TRUE(d1.progressed);
    EXPECT_EQ(res.frames_run, 3u);

    PhaseBOptions opt{};
    if (base.start_key) opt.end_keys.push_back(base.start_key);

    auto d2 = eval.run_until_bp(opt, res);
    EXPECT_TRUE(d2.progressed);
}
