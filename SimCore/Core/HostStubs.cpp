// Minimal stubs to satisfy Dolphin Core when running headless.
#include "Core/Host.h"
#include "Core/System.h"
#include <string>


void Host_Message(HostMessageID) {}
void Host_UpdateDisasmDialog() {}
void Host_UpdateMainFrame() {}
void Host_RefreshDSPDebuggerWindow() {}

bool Host_RendererHasFocus() { return true; }
bool Host_RendererHasFullFocus() { return true; }
bool Host_TASInputHasFocus() { return false; }
bool Host_UIBlocksControllerState() { return false; }
void Host_RequestRenderWindowSize(int, int) {}

void Host_PPCSymbolsChanged() {}
void Host_JitCacheInvalidation() {}
void Host_JitProfileDataWiped() {}
void Host_PPCBreakpointsChanged() {}

void Host_UpdateTitle(Core::System&, const std::string&) {}
void Host_UpdateTitle(const std::string&) {}
void Host_TitleChanged() {}
void Host_YieldToUI() {};

std::vector<std::string> Host_GetPreferredLocales() {
	return {};
}

void Host_UpdateDiscordClientID(const std::string&) {}
bool Host_UpdateDiscordPresenceRaw(const std::string&, const std::string&,
	const std::string&, const std::string&,
	const std::string&, const std::string&,
	std::int64_t, std::int64_t) {
	return false;
}
bool Host_UpdateDiscordPresenceRaw(const std::string&, const std::string&,
	const std::string&, const std::string&,
	const std::string&, const std::string&,
	std::int64_t, std::int64_t, int, int) {
	return false;
}
