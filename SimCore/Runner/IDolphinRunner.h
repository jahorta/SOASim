#pragma once
#include <cstdint>
#include <vector>
#include "../Core/Input/InputPlan.h"

struct IDolphinRunner
{
    virtual ~IDolphinRunner() = default;
    virtual bool step_one_frame() = 0;
    virtual uint32_t get_pc() const = 0;

    virtual void set_next_input(const simcore::GCInputFrame& f) = 0;

    virtual bool read_u8(uint32_t addr, uint8_t& out) const = 0;
    virtual bool read_u16(uint32_t addr, uint16_t& out) const = 0;
    virtual bool read_u32(uint32_t addr, uint32_t& out) const = 0;
    virtual bool read_f32(uint32_t addr, float& out) const = 0;
    virtual bool read_f64(uint32_t addr, double& out) const = 0;

    struct RunUntilHitResult { bool hit; uint32_t pc; const char* reason; };
    virtual bool arm_pc_breakpoints(const std::vector<uint32_t>&) { return false; }
    virtual bool disarm_pc_breakpoints(const std::vector<uint32_t>&) { return false; }
    virtual void clear_all_pc_breakpoints() {}
    virtual RunUntilHitResult run_until_breakpoint_blocking(uint32_t) { return { false, 0u, "unsupported" }; }
};

