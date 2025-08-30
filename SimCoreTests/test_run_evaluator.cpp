#include <gtest/gtest.h>
#include "Runner/RunEvaluator.h"
#include "Runner/LineRunLogger.h"
#include "Runner/IDolphinRunner.h"
#include "Runner/Breakpoints/BPCore.h"
#include "Runner/Breakpoints/Predicate.h"
#include "Runner/Breakpoints/BattleBreakpoints.h"

struct FakeRunner : IDolphinRunner
{
    std::vector<uint32_t> pcs;
    std::unordered_map<uint32_t, uint32_t> mem;
    size_t i{ 0 };
    simcore::GCInputFrame last{};

    bool step_one_frame() override { if (i < pcs.size()) ++i; return true; }
    uint32_t get_pc() const override { return i ? pcs[i - 1] : 0; }
    bool load_savestate(std::string sav) override { return true; }

    void set_next_input(const simcore::GCInputFrame& f) override { last = f; }

    bool read_u8(uint32_t a, uint8_t& o) const override { auto it = mem.find(a); if (it == mem.end()) return false; o = (uint8_t)it->second; return true; }
    bool read_u16(uint32_t a, uint16_t& o) const override { auto it = mem.find(a); if (it == mem.end()) return false; o = (uint16_t)it->second; return true; }
    bool read_u32(uint32_t a, uint32_t& o) const override { auto it = mem.find(a); if (it == mem.end()) return false; o = it->second; return true; }
    bool read_f32(uint32_t, float&) const override { return false; }
    bool read_f64(uint32_t, double&) const override { return false; }
};

struct NullLogger : RunLogger {};

static std::string write_temp_bpmap(const std::string& filename, const std::string& contents)
{
    std::ofstream out(filename, std::ios::trunc);
    out << contents;
    out.close();
    return filename;
}

TEST(BreakpointMap, LoadOverridesByName_NoIfElse)
{

    // override StartTurn and EndBattle; include unknown key that should be ignored
    const std::string path = write_temp_bpmap("battle_test.bpmap",
        "# overrides\n"
        "StartTurn=0x1234\n"
        "EndBattle=65535\n"
        "UnknownKey=0xDEADBEEF\n");

    auto map = battle::load_from_file(path);

    uint32_t startTurnPC = 0, endBattlePC = 0, endTurnPC = 0;
    for (const auto& a : map.addrs)
    {
        if (a.name == std::string("StartTurn"))   startTurnPC = a.pc;
        if (a.name == std::string("EndBattle"))   endBattlePC = a.pc;
        if (a.name == std::string("EndTurn"))     endTurnPC = a.pc;
    }

    EXPECT_EQ(startTurnPC, 0x1234u);
    EXPECT_EQ(endBattlePC, 65535u);
    EXPECT_EQ(endTurnPC, 0u);
}

TEST(BreakpointMap, Evaluator_TerminatesAtTerminalBP_AndAggregatesPredicates)
{
    // Build a BP map with concrete PCs
    auto base = battle::defaults();

    // Set two PCs we will hit
    base.set_pc(battle::StartTurn, 0x2000);
    base.set_pc(battle::EndBattle, 0x4000);

    // Fake run hits StartTurn once, EndBattle once
    FakeRunner host;
    host.pcs = { 0x1000, 0x2000, 0x3000, 0x4000 };

    // Predicate tied to EndBattle: expect mem[0xBEEF] == 2 (we'll set 2 -> pass)
    host.mem[0xBEEF] = 2;
    std::vector<PredicatePtr> preds;
    preds.push_back(std::make_shared<MemEqualsU32>("Victory", battle::EndBattle, 0xBEEF, 2));

    RunEvaluator eval{ host, base, preds, new NullLogger() };
    std::vector<simcore::GCInputFrame> plan(8);
    auto res = eval.run("branchX", plan);

    ASSERT_FALSE(res.hits.empty());
    // Last hit must be EndBattle and run must succeed
    auto last = res.hits.back();
    EXPECT_EQ(last.key, battle::EndBattle);
    EXPECT_TRUE(res.final_success);

    // Now flip memory to cause explicit failure and confirm final_success == false
    host.i = 0;
    host.mem[0xBEEF] = 3;
    auto res2 = eval.run("branchY", plan);
    EXPECT_FALSE(res2.final_success);

    // Missing predicates count as pass: run with no predicates
    RunEvaluator eval2{ host, base, {}, nullptr };
    auto res3 = eval2.run("branchZ", plan);
    EXPECT_TRUE(res3.final_success);
}