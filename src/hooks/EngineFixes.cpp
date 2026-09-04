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
CallDetour GetCurrentAmmoSpinDetour{};

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

void testCall() {
	Console_Print("CLEAR");
}

__declspec(naked) void Hook_FixAutoFireSpin()
{
	__asm
	{
		cmp byte ptr[ebp - 0DDh], 0
		jnz skip_call

		push    0
		mov     edx, [ebp - 18h]
		mov     eax, [edx + 68h]
		mov     ecx, [ebp - 18h]
		mov     ecx, [ecx + 68h]
		mov     edx, [eax]
		mov     eax, [edx + 434h]
		call    eax
		call    testCall

	skip_call :
		mov     edx, [ebp - 18h]
		mov     eax, [edx + 68h]

		mov edx, 0x0093FB73
		jmp edx
	}
}

static CommonLib::TESAmmo* __fastcall Hook_GetCurrentAmmo(CommonLib::TESObjectWEAP* weapon, void* edx, CommonLib::PlayerCharacter* apWeaponHolder)
{
	CommonLib::TESAmmo* result = ThisStdCall<CommonLib::TESAmmo*>(GetCurrentAmmoSpinDetour.GetOverwrittenAddr(), weapon, apWeaponHolder);

	CommonLib::ItemChange* pAmmoItemChange = apWeaponHolder->pCurrentProcess->GetCurrentAmmo();
	if (result && pAmmoItemChange) {
		int iAmmoCount = pAmmoItemChange->iNumber;

		if (iAmmoCount < weapon->data.cAmmoPerShot) {

			CommonLib::Animation* pAnimation1st = ThisStdCall<CommonLib::Animation*>(PlayerCharacter_GetAnimation_Addr, apWeaponHolder, true);
			CommonLib::Animation* pAnimation3rd = ThisStdCall<CommonLib::Animation*>(PlayerCharacter_GetAnimation_Addr, apWeaponHolder, false);

			std::uint16_t usAnimGroupWeapon = ThisStdCall<std::uint16_t>(Animation_GetGroup_Addr, 
				apWeaponHolder->b3rdPerson ? pAnimation3rd : pAnimation1st, 
				CommonLib::AGS_WEAPON);

			std::uint8_t usAnimGroupType = static_cast<std::uint8_t>(usAnimGroupWeapon);
			Console_Print("Anim group type: %d", usAnimGroupType);

			CommonLib::ANIM_GROUP_INFO* pAnimGroupInfo = reinterpret_cast<CommonLib::ANIM_GROUP_INFO*>(0x011977D8);
			CommonLib::ANIM_GROUP_ACTION_TYPE eActionType = pAnimGroupInfo[usAnimGroupType].eAction;

			if (eActionType == CommonLib::AGAT_ATTACK_SPIN) {
				ThisStdCall<void>(Actor_SetAnimAction_Addr, apWeaponHolder, CommonLib::ANIM_ACTION_ATTACK_FOLLOW_THROUGH, nullptr);
			}
			else {
				ThisStdCall<void>(Actor_SetAnimAction_Addr, apWeaponHolder, CommonLib::ANIM_ACTION_NONE, nullptr);
			}

			ThisStdCall<void>(Animation_BlendOut_Addr, pAnimation1st, CommonLib::AGS_WEAPON, false);
			ThisStdCall<void>(Animation_BlendOut_Addr, pAnimation3rd, CommonLib::AGS_WEAPON, false);
		}
	}

	return result;
}

void installEngineFixes() 
{
	if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {
		if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bFixRegenAmmoShots")) {
			// Fix hardcoded ammo regen (shots) value in TESObjectWEAP::Fire
			WriteRelJump(0x005247E5, reinterpret_cast<std::uint32_t>(&Hook_FixWeaponModAmmoRegen));
			PatchMemoryNop(0x005247EA, 5);
		}

		//WriteRelJump(0x0093FB6D, reinterpret_cast<std::uint32_t>(&Hook_FixAutoFireSpin));
		//PatchMemoryNop(0x0093FB72, 1);

		GetCurrentAmmoSpinDetour.WriteRelCall(0x0093FB10, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));
	}
}
