#include "DolphinRunnerAdapter.h"
#include "../Core/Input/InputPlan.h"
#include "../Core/DolphinWrapper.h"

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

bool DolphinRunnerAdapter::load_savestate(std::string sav) {
    return m_w.loadSavestate(sav);
}

void DolphinRunnerAdapter::set_next_input(const simcore::GCInputFrame& f)
{
    m_w.setInputPlan(simcore::InputPlan{ f });
    m_w.applyNextInputFrame();
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

bool DolphinRunnerAdapter::arm_pc_breakpoints(const std::vector<uint32_t>& pcs)
{
    return m_w.armPcBreakpoints(pcs);
}

bool DolphinRunnerAdapter::disarm_pc_breakpoints(const std::vector<uint32_t>& pcs)
{
    return m_w.disarmPcBreakpoints(pcs);
}

void DolphinRunnerAdapter::clear_all_pc_breakpoints()
{
    m_w.clearAllPcBreakpoints();
}

IDolphinRunner::RunUntilHitResult DolphinRunnerAdapter::run_until_breakpoint_blocking(uint32_t timeout_ms)
{
    auto r = m_w.runUntilBreakpointBlocking(timeout_ms);
    return { r.hit, r.pc, r.reason };
}
