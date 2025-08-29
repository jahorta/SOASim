#pragma once
#include <cstdint>
#include "../Core/Input/InputPlan.h" // for GCInputFrame

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
};
