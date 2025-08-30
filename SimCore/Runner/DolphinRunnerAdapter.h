#pragma once
#include "IDolphinRunner.h"

namespace simcore { class DolphinWrapper; }

class DolphinRunnerAdapter final : public IDolphinRunner
{
public:
    explicit DolphinRunnerAdapter(simcore::DolphinWrapper& wrapper);

    bool step_one_frame() override;
    uint32_t get_pc() const override;
    bool load_savestate(std::string sav) override;

    void set_next_input(const simcore::GCInputFrame& f) override;

    bool read_u8(uint32_t addr, uint8_t& out) const override;
    bool read_u16(uint32_t addr, uint16_t& out) const override;
    bool read_u32(uint32_t addr, uint32_t& out) const override;
    bool read_f32(uint32_t addr, float& out) const override;
    bool read_f64(uint32_t addr, double& out) const override;

    bool arm_pc_breakpoints(const std::vector<uint32_t>& pcs) override;
    bool disarm_pc_breakpoints(const std::vector<uint32_t>& pcs) override;
    void clear_all_pc_breakpoints() override;
    RunUntilHitResult run_until_breakpoint_blocking(uint32_t timeout_ms) override;

private:
    simcore::DolphinWrapper& m_w;
};
