#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "AutoFireSpinFix.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Globals.hpp"

constexpr char CONFIG_SECTION[] = "FixAutoFireSpin";

constexpr std::uint32_t Animation_BlendOut_Addr = 0x004994F0;
constexpr std::uint32_t PlayerCharacter_GetAnimation_Addr = 0x00950A60;
constexpr std::uint32_t Actor_SetAnimAction_Addr = 0x008A73E0;


static void Hook_StopAutoFireNoAmmo(CommonLib::PlayerCharacter* apPlayer)
{
	CommonLib::Animation* pAnimation1st = ThisStdCall<CommonLib::Animation*>(PlayerCharacter_GetAnimation_Addr, apPlayer, true);
	CommonLib::Animation* pAnimation3rd = ThisStdCall<CommonLib::Animation*>(PlayerCharacter_GetAnimation_Addr, apPlayer, false);

	if (pAnimation1st && pAnimation3rd) {
		ThisStdCall<void>(Actor_SetAnimAction_Addr, apPlayer, CommonLib::ANIM_ACTION_NONE, nullptr);

		ThisStdCall<void>(Animation_BlendOut_Addr, pAnimation1st, CommonLib::AGS_WEAPON, false);
		ThisStdCall<void>(Animation_BlendOut_Addr, pAnimation3rd, CommonLib::AGS_WEAPON, false);
	}
}

__declspec(naked) void Hook_FixAutoFireSpin()
{
	__asm
	{
		mov     edx, [ebp - 18h]
		push    edx
		call    Hook_StopAutoFireNoAmmo
		add     esp, 4

		mov     byte ptr[ebp - 0DDh], 0

		mov     edx, 0x0093FB6D
		jmp     edx
	}
}


void installAutoFireSpinFixHooks() 
{
	if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {
		WriteRelJump(0x0093FB3E, reinterpret_cast<std::uint32_t>(&Hook_FixAutoFireSpin));
		PatchMemoryNop(0x0093FB43, 2);
		WriteRelJump(0x0093FB66, reinterpret_cast<std::uint32_t>(&Hook_FixAutoFireSpin));
		PatchMemoryNop(0x0093FB6B, 2);
	}
}
