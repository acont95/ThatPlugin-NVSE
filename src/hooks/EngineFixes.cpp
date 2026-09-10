#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "EngineFixes.hpp"
#include "Bethesda/Actor.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/BaseProcess.hpp"
#include "Bethesda/TESObjectWEAP.hpp"
#include "Bethesda/ItemChange.hpp"
#include "Bethesda/InventoryChanges.hpp"
#include "Globals.hpp"
#include "BallisticMelee.hpp"

constexpr char CONFIG_SECTION[] = "EngineFixes";

constexpr std::uint32_t ItemChange_HasModEffectActive_Addr = 0x004BDA70;
constexpr std::uint32_t ItemChange_HasModEffectActiveF_Addr = 0x004BD8D0;
constexpr std::uint32_t TESObjectWEAP_GetCurrentAmmo_Addr = 0x00525980;
constexpr std::uint32_t Animation_BlendOut_Addr = 0x004994F0;
constexpr std::uint32_t PlayerCharacter_GetAnimation_Addr = 0x00950A60;
constexpr std::uint32_t Actor_SetAnimAction_Addr = 0x008A73E0;
constexpr std::uint32_t Animation_GetGroup_Addr = 0x004301B0;
constexpr std::uint32_t TESObjectWEAP_GetFormClipRounds_Addr = 0x004FE160;
constexpr std::uint32_t TESObjectWEAP_GetAmmoRegenRate_Addr = 0x00709430;
constexpr std::uint32_t TESObjectWEAP_GetModEffect_Addr = 0x004BD880;
constexpr std::uint32_t ExtraDataList_GetWeaponModSlotActive_Addr = 0x00418C00;



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

__declspec(naked) void Hook_NoAmmoSwapWithoutReload()
{
	__asm
	{
		mov    [ebp - 15Fh], dl
		test   dl, dl
		jz     skip_call

		mov    edx, 0x00940B03
		jmp    edx

	skip_call:
		mov    edx, 0x00940B15
		jmp    edx
	}
}


void installEngineFixes() 
{
	if (Globals::g_Ini.GetBoolValue("FixAutoFireSpin", "bEnabled")) {
		WriteRelJump(0x0093FB3E, reinterpret_cast<std::uint32_t>(&Hook_FixAutoFireSpin));
		PatchMemoryNop(0x0093FB43, 2);
		WriteRelJump(0x0093FB66, reinterpret_cast<std::uint32_t>(&Hook_FixAutoFireSpin));
		PatchMemoryNop(0x0093FB6B, 2);
	}

	if (Globals::g_Ini.GetBoolValue("NoAmmoSwapWithoutReload", "bEnabled")) {
		WriteRelJump(0x00940AFD, reinterpret_cast<std::uint32_t>(&Hook_NoAmmoSwapWithoutReload));
		PatchMemoryNop(0x00940B02, 1);
	}
}
