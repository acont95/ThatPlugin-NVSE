#include "RegenAmmo.hpp"
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "Bethesda/Actor.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/BaseProcess.hpp"
#include "Bethesda/TESObjectWEAP.hpp"
#include "Bethesda/ItemChange.hpp"
#include "Bethesda/InventoryChanges.hpp"
#include "Globals.hpp"

constexpr char CONFIG_SECTION[] = "RegenAmmo";
CallDetour AddWornDetour{};

constexpr std::uint32_t ItemChange_HasModEffectActive_Addr = 0x004BDA70;
constexpr std::uint32_t ItemChange_HasModEffectActiveF_Addr = 0x004BD8D0;
constexpr std::uint32_t TESObjectWEAP_GetFormClipRounds_Addr = 0x004FE160;
constexpr std::uint32_t TESObjectWEAP_GetAmmoRegenRate_Addr = 0x00709430;
constexpr std::uint32_t TESObjectWEAP_GetModEffect_Addr = 0x004BD880;
constexpr std::uint32_t ExtraDataList_GetWeaponModSlotActive_Addr = 0x00418C00;


static int getRegenValue(CommonLib::Actor* apActor, int aiShotCount)
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
			if (ThisStdCall<bool>(ItemChange_HasModEffectActiveF_Addr, pCurrentWeapon, cModEffect, &fRegen)) {
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

static __declspec(naked) void Hook_FixWeaponModAmmoRegen()
{
	__asm
	{
		mov     eax, [ebp - 0x58]
		push    eax
		mov     eax, [ebp - 0x2C]
		push    eax
		call    getRegenValue
		add     esp, 8

		mov[ebp - 0x1D4], eax

		mov edx, 0x005248D0
		jmp edx
	}
}


static bool isWeaponModActive(CommonLib::TESObjectWEAP* apWeapon, CommonLib::ExtraDataList* apExtraData, CommonLib::WEAPON_MOD_EFFECT eModEffect) {
	if (ThisStdCall<CommonLib::WEAPON_MOD_EFFECT>(TESObjectWEAP_GetModEffect_Addr, apWeapon, 1u) == eModEffect
		&& ThisStdCall<bool>(ExtraDataList_GetWeaponModSlotActive_Addr, apExtraData, 1u))
	{
		return true;
	}
	else if (ThisStdCall<CommonLib::WEAPON_MOD_EFFECT>(TESObjectWEAP_GetModEffect_Addr, apWeapon, 2u) == eModEffect
		&& ThisStdCall<bool>(ExtraDataList_GetWeaponModSlotActive_Addr, apExtraData, 2u))
	{
		return true;
	}
	else if (ThisStdCall<CommonLib::WEAPON_MOD_EFFECT>(TESObjectWEAP_GetModEffect_Addr, apWeapon, 4u) == eModEffect
		&& ThisStdCall<bool>(ExtraDataList_GetWeaponModSlotActive_Addr, apExtraData, 4u))
	{
		return true;
	}

	return false;
}

static bool skipReload(
	CommonLib::TESObjectWEAP* apWeapon, CommonLib::ExtraDataList* apExtraData)
{
	if (apWeapon && apExtraData) {
		bool bIsRegenMod = isWeaponModActive(apWeapon, apExtraData, CommonLib::WEAPON_MOD_AMMO_REGEN_SECONDS);
		float fRegenRate = ThisStdCall<float>(TESObjectWEAP_GetAmmoRegenRate_Addr, apWeapon, bIsRegenMod);
		if (fRegenRate > 0.0f) {
			return true;
		}
	}

	return false;
}

static __declspec(naked) void Hook_DeferReload()
{
	__asm
	{
		mov     eax, [ebp + 0x10]
		push    eax
		mov     eax, [ebp - 0x40]
		push    eax
		call    skipReload
		add     esp, 8
		test    al, al
		jnz     skip_call

		push    1
		movzx   edx, [ebp - 0x39]

		mov     eax, 0x0088DF5C
		jmp     eax

		skip_call :
		mov     eax, 0x0088DF73
			jmp     eax
	}
}


static void __fastcall Hook_AddWorn(
	CommonLib::InventoryChanges* apInventoryChanges, void* edx, CommonLib::TESBoundObject* apObject,
	int aiNumber, CommonLib::Actor* apActor, CommonLib::ExtraDataList* apExtraData, bool abLeft, bool abForceEquip)
{
	ThisStdCall<void>(AddWornDetour.GetOverwrittenAddr(), apInventoryChanges, apObject, aiNumber, apActor, apExtraData, abLeft, abForceEquip);

	if (apObject && apExtraData) {
		CommonLib::ENUM_FORM_ID eFormType =
			static_cast<CommonLib::ENUM_FORM_ID>(apObject->cFormType);

		if (eFormType == CommonLib::WEAP_ID) {
			CommonLib::TESObjectWEAP* pWeapon = static_cast<CommonLib::TESObjectWEAP*>(apObject);
			bool bIsRegenMod = isWeaponModActive(pWeapon, apExtraData, CommonLib::WEAPON_MOD_AMMO_REGEN_SECONDS);
			float fRegenRate = ThisStdCall<float>(TESObjectWEAP_GetAmmoRegenRate_Addr, pWeapon, bIsRegenMod);
			if (fRegenRate > 0.0f) {
				std::uint8_t* ebp = GetParentBasePtr(_AddressOfReturnAddress(), false);
				bool bMod = *reinterpret_cast<bool*>(ebp - 0x39);
				apActor->ReloadWeaponNV(pWeapon, 0, bMod, 1);
			}
		}
	}
}


static int Hook_RegenAmmoCount(CommonLib::InventoryChanges* apInventoryChanges, CommonLib::TESBoundObject* apObject, int aiOriginalCount)
{
	CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacter::GetPlayerSingleton();
	CommonLib::TESObjectREFR* pRef = apInventoryChanges->pRef;
	if (apObject && pRef && pRef->IsActor()) {
		CommonLib::Actor* pActor = static_cast<CommonLib::Actor*>(pRef);

		CommonLib::ENUM_FORM_ID eFormType =
			static_cast<CommonLib::ENUM_FORM_ID>(apObject->cFormType);

		if (eFormType == CommonLib::AMMO_ID) {
			CommonLib::BaseProcess* pCurrentProcess = pActor->pCurrentProcess;
			if (pCurrentProcess) {
				CommonLib::ItemChange* pCurrentWeapon = pCurrentProcess->GetCurrentWeapon();
				if (pCurrentWeapon) {
					CommonLib::TESObjectWEAP* pWeapon = static_cast<CommonLib::TESObjectWEAP*>(pCurrentWeapon->pContainerObj);
					bool bIsRegenMod = ThisStdCall<bool>(ItemChange_HasModEffectActive_Addr, pCurrentWeapon, CommonLib::WEAPON_MOD_AMMO_REGEN_SECONDS);

					float fRegenRate = ThisStdCall<float>(TESObjectWEAP_GetAmmoRegenRate_Addr, pWeapon, bIsRegenMod);
					if (fRegenRate > 0.0f) {
						bool bIsClipMod = ThisStdCall<bool>(ItemChange_HasModEffectActive_Addr, pCurrentWeapon, CommonLib::WEAPON_MOD_INCREASE_CLIP_SIZE);
						int iClipSize = ThisStdCall<int>(TESObjectWEAP_GetFormClipRounds_Addr, pWeapon, bIsClipMod);
						return iClipSize;
					}
				}
			}
		}
	}

	return aiOriginalCount;
}

static __declspec(naked) void Hook_GetObjectCount()
{
	__asm
	{
		mov     edx, [ebp - 0xC]
		push    edx
		mov     edx, [ebp + 0x8]
		push    edx
		mov     edx, [ebp - 0x10]
		push    edx
		call    Hook_RegenAmmoCount
		add     esp, 0xC

		mov     esp, ebp

		mov     edx, 0x004C8FC5
		jmp     edx
	}
}

void installRegenAmmoHooks() {
	if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {

		if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bRegenFullChargeOnEquip")) 
		{
			// Defer weapon equip reload until weapon ItemChange is set on Actor process to allow checking for weapon mods
			AddWornDetour.WriteRelCall(0x0088DFFF, reinterpret_cast<std::uint32_t>(&Hook_AddWorn));
			WriteRelJump(0x0088DF56, reinterpret_cast<std::uint32_t>(&Hook_DeferReload));
			PatchMemoryNop(0x0088DF5B, 1);

			WriteRelJump(0x004C8FC0, reinterpret_cast<std::uint32_t>(&Hook_GetObjectCount));
		}

		if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bFixRegenAmmoCount")) 
		{
			// Fix hardcoded ammo regen (shots) value in TESObjectWEAP::Fire
			WriteRelJump(0x005247E5, reinterpret_cast<std::uint32_t>(&Hook_FixWeaponModAmmoRegen));
			PatchMemoryNop(0x005247EA, 5);
		}
	}
}
