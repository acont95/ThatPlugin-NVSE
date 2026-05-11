#include <cstdint>
#include "Globals.hpp"
#include "PersistReferenceScale.hpp"
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"

constexpr char CONFIG_SECTION[] = "PersistReferenceScale";

void installPersistReferenceScaleHooks() {
	if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {
		PatchMemoryNop(0x004C5D5F, 5);
		PatchMemoryNop(0x004C5C13, 5);
		PatchMemoryNop(0x004C46C5, 5);
		PatchMemoryNop(0x004C44CD, 5);
	}
}
