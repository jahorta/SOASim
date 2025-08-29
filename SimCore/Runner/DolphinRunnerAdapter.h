#pragma once
#include "IDolphinRunner.h"

// Forward-declare your existing wrapper
namespace simcore { class DolphinWrapper; }

class DolphinRunnerAdapter final : public IDolphinRunner
{
public:
    explicit DolphinRunnerAdapter(simcore::DolphinWrapper& wrapper);

    bool step_one_frame() override;
    uint32_t get_pc() const override;

    void set_next_input(const GCInputFrame& f) override;

    bool read_u8(uint32_t addr, uint8_t& out) const override;
    bool read_u16(uint32_t addr, uint16_t& out) const override;
    bool read_u32(uint32_t addr, uint32_t& out) const override;
    bool read_f32(uint32_t addr, float& out) const override;
    bool read_f64(uint32_t addr, double& out) const override;

private:
    simcore::DolphinWrapper& m_w;
};
