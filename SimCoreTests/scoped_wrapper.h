#pragma once

#include "Core/DolphinWrapper.h"
struct ScopedEmu {
	simcore::DolphinWrapper w;
	~ScopedEmu() { w.shutdownAll(); } // guarantees teardown on any exit path
};
