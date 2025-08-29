#include "Adapters/DolphinRunnerAdapter.h"
#include "Core/InputPlan.h"
#include <cstdint>

// Include your wrapper header here
#include "Core/DolphinWrapper.h"

using simcore::DolphinWrapper;

DolphinRunnerAdapter::DolphinRunnerAdapter(DolphinWrapper& wrapper) : m_w(wrapper) {}

bool DolphinRunnerAdapter::step_one_frame()
{
    return m_w.stepOneFrameBlocking();
}

uint32_t DolphinRunnerAdapter::get_pc() const
{
    return m_w.getPC();
}

void DolphinRunnerAdapter::set_next_input(const GCInputFrame& f)
{
    m_w.setNextInputFrame(f);
}

bool DolphinRunnerAdapter::read_u8(uint32_t addr, uint8_t& out) const
{
    return m_w.readU8(addr, out);
}

bool DolphinRunnerAdapter::read_u16(uint32_t addr, uint16_t& out) const
{
    return m_w.readU16(addr, out);
}

bool DolphinRunnerAdapter::read_u32(uint32_t addr, uint32_t& out) const
{
    return m_w.readU32(addr, out);
}

bool DolphinRunnerAdapter::read_f32(uint32_t addr, float& out) const
{
    return m_w.readF32(addr, out);
}

bool DolphinRunnerAdapter::read_f64(uint32_t addr, double& out) const
{
    return m_w.readF64(addr, out);
}
