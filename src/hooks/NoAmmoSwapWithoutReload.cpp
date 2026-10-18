#include "NoAmmoSwapWithoutReload.hpp"
#include "Globals.hpp"
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"

constexpr char CONFIG_SECTION[] = "NoAmmoSwapWithoutReload";

static __declspec(naked) void Hook_NoAmmoSwapWithoutReload()
{
	__asm
	{
		mov[ebp - 15Fh], dl
		test   dl, dl
		jz     skip_call

		mov    edx, 0x00940B03
		jmp    edx

	skip_call :
		mov    edx, 0x00940B15
		jmp    edx
	}
}


void installNoAmmoSwapWithoutReloadHooks()
{
	if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {
		WriteRelJump(0x00940AFD, reinterpret_cast<std::uint32_t>(&Hook_NoAmmoSwapWithoutReload));
		PatchMemoryNop(0x00940B02, 1);
	}
}
