#pragma once
#include <mutex>
#include <string_view>
#include <optional>

#include "InputPlan.h"   // GCInputFrame
#include "../../Utils/Log.h"

namespace simcore {

    // Provides a Dolphin controller override for one GC port.
    // You call install() once after Pad::Initialize(), and then setFrame(...) each frame.
    // Thread-safe: setFrame can be called from your runner while Dolphin polls on its thread.
    class GCPadOverride {
    public:
        explicit GCPadOverride(int port = 0) : m_port(port) {}
        ~GCPadOverride() = default;

        // Install the override lambda on the Dolphin controller for this port.
        // Safe to call multiple times; it is a no-op after the first successful install.
        void install();

        // Update the currently “held” controller state.
        void setFrame(const GCInputFrame& f);

        // Convenience: centered sticks, no buttons.
        static GCInputFrame NeutralFrame();

        // For diagnostics
        bool isInstalled() const { return m_installed; }
        int  port()        const { return m_port; }

    private:
        int m_port{ 0 };
        bool m_installed{ false };

        GCInputFrame m_cur{};     // guarded by m_mtx
        mutable std::mutex m_mtx;
    };

} // namespace simcore
