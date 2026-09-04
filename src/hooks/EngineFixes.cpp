#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "EngineFixes.hpp"
#include "Bethesda/Actor.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/BaseProcess.hpp"
#include "Bethesda/TESObjectWEAP.hpp"
#include "Globals.hpp"

constexpr char CONFIG_SECTION[] = "EngineFixes";

constexpr std::uint32_t ItemChange_HasModEffectActive_Addr = 0x004BD8D0;

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

void installEngineFixes() 
{
	if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {
		if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bFixRegenAmmoShots")) {
			// Fix hardcoded ammo regen (shots) value in TESObjectWEAP::Fire
			WriteRelJump(0x005247E5, reinterpret_cast<std::uint32_t>(&Hook_FixWeaponModAmmoRegen));
			PatchMemoryNop(0x005247EA, 5);
		}
	}
}
