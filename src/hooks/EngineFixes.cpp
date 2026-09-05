#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "EngineFixes.hpp"
#include "Bethesda/Actor.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/BaseProcess.hpp"
#include "Bethesda/TESObjectWEAP.hpp"
#include "Bethesda/ItemChange.hpp"
#include "Globals.hpp"
#include "BallisticMelee.hpp"

constexpr char CONFIG_SECTION[] = "EngineFixes";
CallDetour GetAmmoSwapHelperDetour{};

constexpr std::uint32_t ItemChange_HasModEffectActive_Addr = 0x004BD8D0;
constexpr std::uint32_t TESObjectWEAP_GetCurrentAmmo_Addr = 0x00525980;
constexpr std::uint32_t Animation_BlendOut_Addr = 0x004994F0;
constexpr std::uint32_t PlayerCharacter_GetAnimation_Addr = 0x00950A60;
constexpr std::uint32_t Actor_SetAnimAction_Addr = 0x008A73E0;
constexpr std::uint32_t Animation_GetGroup_Addr = 0x004301B0;

int getRegenValue(CommonLib::Actor* apActor, int aiShotCount) 
{
	CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacter::GetPlayerSingleton();

	if (apActor == nullptr) {
		return 0;
	}

	int iRegenSubtract = 0;
	if (apActor == pPlayer) {
		CommonLib::BaseProcess* pCurrentProcess = pPlayer->pCurrentProcess;
		if (pCurrentProcess && pCurrentProcess->GetCurrentWeapon()) {
			float fRegen = 0.0f;
			CommonLib::ItemChange* pCurrentWeapon = pCurrentProcess->GetCurrentWeapon();

			std::uint8_t cModEffect = CommonLib::WEAPON_MOD_EFFECT::WEAPON_MOD_AMMO_REGEN_SHOT;
			if (ThisStdCall<bool>(ItemChange_HasModEffectActive_Addr, pCurrentWeapon, cModEffect, &fRegen)) {
				pPlayer->cShotsSinceLastAmmoRegen += aiShotCount;

				const int iRegen = static_cast<int>(fRegen);
				if (pPlayer->cShotsSinceLastAmmoRegen >= static_cast<int>(fRegen)) {
					pPlayer->cShotsSinceLastAmmoRegen -= iRegen;
					iRegenSubtract = 1;
				}
			}
		}
	}

	return iRegenSubtract;
}

__declspec(naked) void Hook_FixWeaponModAmmoRegen()
{
	__asm
	{
		mov     eax, [ebp - 0x58]
		push    eax
		mov     eax, [ebp - 0x2C]
		push    eax
		call    getRegenValue
		add     esp, 8

		mov     [ebp - 0x1D4], eax

		mov edx, 0x005248D0
		jmp edx
	}
}

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
		test dl, dl
		jz  skip_call

		mov edx, 0x00940B03
		jmp edx

	skip_call :
		mov edx, 0x00940B15
		jmp edx
	}
}

void installEngineFixes() 
{
	if (Globals::g_Ini.GetBoolValue("FixRegenAmmoShots", "bEnabled")) {
		// Fix hardcoded ammo regen (shots) value in TESObjectWEAP::Fire
		WriteRelJump(0x005247E5, reinterpret_cast<std::uint32_t>(&Hook_FixWeaponModAmmoRegen));
		PatchMemoryNop(0x005247EA, 5);
	}

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
