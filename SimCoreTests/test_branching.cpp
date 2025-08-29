#include "gtest/gtest.h"
#include "Core/Branching/Branching.h"
#include "Core/Input/InputPlan.h"

using namespace simcore;

static GCInputFrame btn(uint16_t b) { GCInputFrame f{}; f.buttons = b; return f; }

TEST(Branching, EnumeratesCartesianProduct) {
    BranchSpec spec;
    spec.total_frames = 3;
    spec.default_frame = GCInputFrame{};
    spec.decisions = {
        DecisionPoint{0, { btn(GC_A), btn(GC_B) }},
        DecisionPoint{2, { btn(GC_L_BTN), btn(GC_R_BTN), btn(GC_Z) }},
    };

    uint32_t max_frame = 0;
    for (auto& d : spec.decisions) max_frame = std::max(max_frame, d.frame_index);
    ASSERT_GT(spec.total_frames, max_frame) << "total_frames must exceed last decision frame";

    BranchExplorer ex(spec);
    size_t count = 0;
    while (auto inst = ex.next()) {
        ASSERT_EQ(inst->plan.size(), 3u);
        ++count;
    }
    EXPECT_EQ(count, 2u * 3u);
}
