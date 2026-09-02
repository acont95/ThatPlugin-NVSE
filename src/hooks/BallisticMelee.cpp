#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "Bethesda/Actor.hpp"
#include "Bethesda/HitData.hpp"
#include "Bethesda/TESAmmo.hpp"
#include "Bethesda/TESObjectWEAP.hpp"
#include "Bethesda/Projectile.hpp"
#include "Bethesda/Tile.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/BaseProcess.hpp"
#include "Bethesda/InventoryChanges.hpp"
#include "Bethesda/TESAnimGroup.hpp"
#include "Bethesda/Animation.hpp"
#include "Bethesda/VATS.hpp"
#include "Bethesda/VATS_COMMAND.hpp"
#include "Bethesda/BGSEntryPoint.hpp"
#include "Gamebryo/NiPoint3.hpp"
#include "Gamebryo/NiTextKeyExtraData.hpp"
#include "Gamebryo/NiTextKey.hpp"
#include "BallisticMelee.hpp"
#include "Globals.hpp"

constexpr char CONFIG_SECTION[] = "BallisticMelee";

CallDetour ObjectHitDetour{};
CallDetour CombatHitDetour{};
CallDetour GetCurrentAmmoDetour{};
CallDetour GetOffsetDetour{};
CallDetour GetExtraDataDetour{};
CallDetour IsAutomaticDetour{};

constexpr std::uint32_t Actor_UseAmmo_Addr = 0x008A89A0;
constexpr std::uint32_t Actor_ShouldUseAmmo_Addr = 0x008A8DD0;
constexpr std::uint32_t Actor_GetCurrentWeapon_Addr = 0x008A1710;
constexpr std::uint32_t Projectile_Constructor_Addr = 0x009BBEF0;
constexpr std::uint32_t Projectile_Destructor_Addr = 0x009BC470;
constexpr std::uint32_t TESForm_SetTemporary_Addr = 0x00484490;
constexpr std::uint32_t Animation_PlayGroup_Addr = 0x00494740;
constexpr std::uint32_t PlayerCharacter_StartAnimOn1stPerson_Addr = 0x009520F0;
constexpr std::uint32_t Actor_SetAnimAction_Addr = 0x008A73E0;
constexpr std::uint32_t TESAnimGroup_GetTime_Addr = 0x005F3780;
constexpr std::uint32_t BSAnimGroupSequence_GetAnimGroup_Addr = 0x0048F7F0;
constexpr std::uint32_t ActionPoints_IsAttackAction_Addr = 0x0066DDE0;
constexpr std::uint32_t ItemChange_HasModEffectActive_Addr = 0x004BD8D0;
constexpr std::uint32_t VATS_GetCurrentAction_Addr = 0x009C71C0;
constexpr std::uint32_t TESObjectWEAP_GetCurrentAmmo_Addr = 0x00525980;
constexpr std::uint32_t BGSEntryPoint_HandleEntryPoint_Addr = 0x005E58F0;
constexpr std::uint32_t BSRandom_RandomUnsignedInt_Addr = 0x00487F50;
constexpr std::uint32_t NiControllerSequence_GetTextKeys_Addr = 0x00700300;
constexpr std::uint32_t NiTextKeyExtraData_GetKeys_Addr = 0x00495F30;

bool isBallisticMelee(CommonLib::TESObjectWEAP* weapon) {
	return weapon->data.eType <= CommonLib::WEAPON_TYPE::WEAPON_TWO_HAND_MELEE && weapon->pFormAmmo;
}

bool __fastcall Hook_IsMeleeWeapon(CommonLib::TESObjectWEAP* weapon, void* edx)
{
	if (isBallisticMelee(weapon))
		return false;

	return weapon->data.eType <= CommonLib::WEAPON_TYPE::WEAPON_TWO_HAND_MELEE;
}

int getShotCount(CommonLib::Actor* apActor) {
	CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacter::GetPlayerSingleton();

	if (apActor == nullptr) {
		return 0;
	}

	int iShotCount = 1;
	CommonLib::BaseProcess* pCurrentProcess = apActor->pCurrentProcess;
	if (pCurrentProcess) {
		iShotCount = pCurrentProcess->GetShotsToFire();
	}

	int iRegenSubtract = 0;
	if (apActor == pPlayer) {
		CommonLib::BaseProcess* pCurrentProcess = pPlayer->pCurrentProcess;
		if (pCurrentProcess && pCurrentProcess->GetCurrentWeapon()) {
			float fRegen = 0.0f;
			CommonLib::ItemChange* pCurrentWeapon = pCurrentProcess->GetCurrentWeapon();

			std::uint8_t cModEffect = CommonLib::WEAPON_MOD_EFFECT::WEAPON_MOD_AMMO_REGEN_SHOT;
			if (ThisStdCall<bool>(ItemChange_HasModEffectActive_Addr, pCurrentWeapon, cModEffect, &fRegen)) {
				pPlayer->cShotsSinceLastAmmoRegen += iShotCount;

				std::uint8_t iRegen = static_cast<std::uint8_t>(fRegen);
				if (pPlayer->cShotsSinceLastAmmoRegen >= static_cast<int>(fRegen)) {
					pPlayer->cShotsSinceLastAmmoRegen -= iRegen;
					iRegenSubtract = 1;
				}
			}
		}
	}

	return iShotCount - iRegenSubtract;
}

void spawnRandomShellCasing(CommonLib::Actor* apActor) {
	CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacter::GetPlayerSingleton();
	CommonLib::TESObjectWEAP* pCurrentWeapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, apActor);

	if (apActor == pPlayer && pCurrentWeapon) {
		CommonLib::TESAmmo* pCurrentAmmo = ThisStdCall<CommonLib::TESAmmo*>(TESObjectWEAP_GetCurrentAmmo_Addr, pCurrentWeapon, pPlayer);
		if (pCurrentAmmo) {
			CommonLib::TESForm* pConsumedObject = pCurrentAmmo->NVdata.pAmmoConsumedObject;
			float fConsumedChance = pCurrentAmmo->NVdata.fAmmoConsumedChance;
			CdeclCall<void>(
				BGSEntryPoint_HandleEntryPoint_Addr, 
				CommonLib::BGSEntryPoint::ENTRY_POINT::MODIFY_CHANCE_FOR_AMMO_ITEM, pPlayer, pCurrentWeapon, &fConsumedChance);

			std::uint32_t iRandom = CdeclCall<std::uint32_t>(BSRandom_RandomUnsignedInt_Addr);
			float fRandom = static_cast<float>(iRandom % 0x64);

			if (pConsumedObject && fConsumedChance >= fRandom && pConsumedObject->IsBoundObject()) {
				CommonLib::TESBoundObject* pBoundObject = static_cast<CommonLib::TESBoundObject*>(pConsumedObject);
				if (pBoundObject) {
					pPlayer->AddObjecttoContainer(pBoundObject, nullptr, 1);
				}
			}
		}
	}
}

CommonLib::Tile* __fastcall Hook_ObjectHit(CommonLib::Actor* apActor, void* edx, bool abPowerAttack)
{
	CommonLib::Tile* result = ThisStdCall<CommonLib::Tile*>(ObjectHitDetour.GetOverwrittenAddr(), apActor, abPowerAttack);
	CommonLib::TESObjectWEAP* weapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, apActor);
	bool shouldUseAmmo = ThisStdCall<bool>(Actor_ShouldUseAmmo_Addr, apActor, weapon);

	if (shouldUseAmmo && weapon && isBallisticMelee(weapon)) {
		bool isAutomatic = (weapon->data.cFlags >> 1) & 1;
		if (result || isAutomatic) {
			int iShotCount = getShotCount(apActor);
			apActor->UseAmmo(iShotCount);
			spawnRandomShellCasing(apActor);
		}
	}

	return result;
}

void __fastcall Hook_CombatHit(
	CommonLib::Actor* apActor,
	void* edx,
	CommonLib::Actor* apTarget,
	bool abPowerAttack,
	CommonLib::Projectile* apProjectile,
	char cMeleeEffect)
{
	ThisStdCall<void>(
		CombatHitDetour.GetOverwrittenAddr(),
		apActor,
		apTarget,
		abPowerAttack,
		apProjectile,
		cMeleeEffect);

	CommonLib::TESObjectWEAP* weapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, apActor);
	bool shouldUseAmmo = ThisStdCall<bool>(Actor_ShouldUseAmmo_Addr, apActor, weapon);

	if (shouldUseAmmo && weapon && isBallisticMelee(weapon)) {
		int iShotCount = getShotCount(apActor);
		apActor->UseAmmo(iShotCount);
		spawnRandomShellCasing(apActor);
	}
}

CommonLib::TESAmmo* __fastcall Hook_GetCurrentAmmo(CommonLib::TESObjectWEAP* weapon, void* edx, CommonLib::Actor* apWeaponHolder)
{
	if (weapon && isBallisticMelee(weapon))
		return nullptr;

	return ThisStdCall<CommonLib::TESAmmo*>(GetCurrentAmmoDetour.GetOverwrittenAddr(), weapon, apWeaponHolder);
}

CommonLib::ExtraDataList* __fastcall Hook_GetExtraData_JIP(CommonLib::TESObjectREFR* apReference, void* edx)
{
	// Actor::HitMe uses ebx as base pointer, not ebp
	std::uintptr_t pEbx;
	__asm mov pEbx, ebx;

	CommonLib::HitData* pHitData = *reinterpret_cast<CommonLib::HitData**>(pEbx + 0x8);
	CommonLib::TESObjectWEAP* apFromWeapon = pHitData->pWeapon;

	if (apFromWeapon && isBallisticMelee(apFromWeapon)) {
		CommonLib::BGSProjectile* apProjectileBase = apFromWeapon->data.pProjectile;
		CommonLib::Actor* apShooter = pHitData->pAggressor;
		if (apProjectileBase && apShooter) {
			CommonLib::Projectile* projectile = New<CommonLib::Projectile, Projectile_Constructor_Addr>(
				apProjectileBase,
				apShooter,
				apFromWeapon,
				CommonLib::NiPoint3::ZERO,
				0.0f,
				0.0f
			);

			projectile->cFormType = CommonLib::ENUM_FORM_ID::PROJ_ID;
			ThisStdCall<void>(TESForm_SetTemporary_Addr, projectile);
			pHitData->pSourceRef = projectile;
		}
	}


	return ThisStdCall<CommonLib::ExtraDataList*>(GetExtraDataDetour.GetOverwrittenAddr(), apReference);
}


void cleanupHitDataProjectile(CommonLib::HitData* apHitData) 
{
	if (apHitData) {
		CommonLib::TESObjectREFR* pSourceRef = apHitData->pSourceRef;
		CommonLib::TESObjectWEAP* apFromWeapon = apHitData->pWeapon;

		if (apFromWeapon && isBallisticMelee(apFromWeapon) && pSourceRef && pSourceRef->IsProjectile()) {
			Delete<CommonLib::Projectile, Projectile_Destructor_Addr>(static_cast<CommonLib::Projectile*>(pSourceRef));
			apHitData->pSourceRef = nullptr;
		}
	}
}


__declspec(naked) void RemoveHitDataProjectileHook_JIP()
{
	__asm
	{
		mov     eax, [ebx + 8]
		push    eax
		call    cleanupHitDataProjectile
		add     esp, 4

		mov     byte ptr[ebp - 61h], 0
		mov     eax, [ebp - 18h]

		mov edx, 0x0089AC62
		jmp edx
	}
}


static bool findPowerAttackReloadKeyTime(CommonLib::BSAnimGroupSequence* apSequence, float& afTime) {
	const char* pcKey = "PowerAttackReload";

	CommonLib::NiTextKeyExtraData* pTextKeyExtraData = ThisStdCall<CommonLib::NiTextKeyExtraData*>(NiControllerSequence_GetTextKeys_Addr, apSequence);
	unsigned int iNumKeys = 0;
	CommonLib::NiTextKey* pTextKey = ThisStdCall<CommonLib::NiTextKey*>(NiTextKeyExtraData_GetKeys_Addr, pTextKeyExtraData, &iNumKeys);
	

	for (unsigned int i = 0; i < iNumKeys; i++) {
		const char* pcKeyString = pTextKey[i].m_kText.m_kHandle;
		if (pcKeyString && _stricmp(pcKeyString, pcKey) == 0) {
			afTime = pTextKey[i].m_fTime;
			return true;
		}
	}

	return false;
}


float __fastcall Hook_GetOffset(CommonLib::Animation* pAnimation, void* edx, CommonLib::BSAnimGroupSequence* apSequence)
{
	float fOffset = ThisStdCall<float>(GetOffsetDetour.GetOverwrittenAddr(), pAnimation, apSequence);

	if (pAnimation == nullptr) {
		return fOffset;
	}

	std::uint8_t* ebp = GetParentBasePtr(_AddressOfReturnAddress(), false);
	CommonLib::TESObjectREFR* pOwnerObject = *reinterpret_cast<CommonLib::TESObjectREFR**>(ebp + 0x8);
	CommonLib::ANIM_GROUP_INFO* pAnimGroupInfo = reinterpret_cast<CommonLib::ANIM_GROUP_INFO*>(0x011977D8);

	CommonLib::ANIM_GROUP_SECTION eSection = *reinterpret_cast<CommonLib::ANIM_GROUP_SECTION*>(ebp - 0x28);

	std::uint8_t usAnimGroup = static_cast<std::uint8_t>(pAnimation->group[eSection]);
	CommonLib::ANIM_GROUP_INFO animGroupInfo = pAnimGroupInfo[usAnimGroup];

	if (apSequence &&
		eSection == CommonLib::ANIM_GROUP_SECTION::AGS_WEAPON &&
		animGroupInfo.eAction == CommonLib::ANIM_GROUP_ACTION_TYPE::AGAT_ATTACK_POWER &&
		pAnimation->sQueuedReloadGroup != CommonLib::ANIM_GROUP_ENUM::ANIM_GROUP_NONE &&
		pAnimation->action[eSection] == CommonLib::ANIM_GROUP_ACTION::AGA_ATTACK_HIT)
	{
		CommonLib::TESAnimGroup* pAnimGroup = ThisStdCall<CommonLib::TESAnimGroup*>(BSAnimGroupSequence_GetAnimGroup_Addr, apSequence);
		if (pAnimGroup == nullptr) {
			return fOffset;
		}

		float fReloadKeyTime = 0.0f;
		bool bReloadKeyMatch = findPowerAttackReloadKeyTime(apSequence, fReloadKeyTime);

		float fCurrActionTime = ThisStdCall<float>(TESAnimGroup_GetTime_Addr, pAnimGroup, pAnimation->action[eSection]);
		float fNextActionTime = ThisStdCall<float>(TESAnimGroup_GetTime_Addr, pAnimGroup, pAnimation->action[eSection] + 1);

		const float fThresholdTime = bReloadKeyMatch
			? fReloadKeyTime
			: fCurrActionTime + (fNextActionTime - fCurrActionTime) * 0.5f;


		if (fOffset >= fThresholdTime)
		{
			ThisStdCall<CommonLib::BSAnimGroupSequence*>(
				Animation_PlayGroup_Addr,
				pAnimation,
				pAnimation->sQueuedReloadGroup,
				CommonLib::ACTION_FLAGS::ACTION_START,
				-1,
				CommonLib::ANIM_GROUP_SECTION::AGS_NONE
			);

			CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacter::GetPlayerSingleton();
			if (pOwnerObject && pOwnerObject == pPlayer) {
				ThisStdCall<void>(PlayerCharacter_StartAnimOn1stPerson_Addr, pPlayer, pAnimation->sQueuedReloadGroup, CommonLib::ACTION_FLAGS::ACTION_START);
			}

			if (pOwnerObject && pOwnerObject->IsActor()) {
				CommonLib::TESObjectWEAP* pCurrentWeapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, pOwnerObject);
				if (pCurrentWeapon && pCurrentWeapon->bIsLoopingReload) {
					ThisStdCall<void>(Actor_SetAnimAction_Addr, pOwnerObject, CommonLib::ANIMATION_ACTION::ANIM_ACTION_RELOAD_LOOP, apSequence);
				}
				else {
					ThisStdCall<void>(Actor_SetAnimAction_Addr, pOwnerObject, CommonLib::ANIMATION_ACTION::ANIM_ACTION_RELOAD, apSequence);
				}
			}
		}
	}

	return fOffset;
}


void decrementVATSFireShots(CommonLib::VATS_COMMAND* apVATSCommand, CommonLib::TESObjectWEAP* apWeapon, CommonLib::Actor* apActor)
{
	if (apWeapon && isBallisticMelee(apWeapon)) {
		CommonLib::BaseProcess* pCurrentProcess = nullptr;
		if (apActor) {
			pCurrentProcess = apActor->pCurrentProcess;
		}

		std::uint8_t ucShotsToFire = 1;
		if (pCurrentProcess) {
			ucShotsToFire = pCurrentProcess->GetShotsToFire();
		}
		while (ucShotsToFire) {
			std::uint8_t ucSubAmount = apWeapon->data.cAmmoPerShot > 0 ? apWeapon->data.cAmmoPerShot : 1;

			if (apVATSCommand->cFireShots >= ucSubAmount) {
				apVATSCommand->cFireShots -= ucSubAmount;
			}
			else {
				apVATSCommand->cFireShots = 0;
			}

			--ucShotsToFire;
		}
	}
	else
	{
		--apVATSCommand->cFireShots;
	}
}


__declspec(naked) void DecrementFireShotsNonAutoHook()
{
	__asm
	{
		mov     eax, [ebp - 0x9C]
		push    eax
		mov     eax, [ebp - 0x24]
		push    eax
		mov     eax, [ebp - 0x4]
		push    eax

		call    decrementVATSFireShots
		add     esp, 0xC

		mov edx, 0x00948643
		jmp edx
	}
}


__declspec(naked) void DecrementFireShotsAutoHook()
{
	__asm
	{
		mov     eax, [ebp - 0x12C]
		push    eax
		mov     eax, [ebp - 0x2C]
		push    eax
		mov     eax, [ebp - 0x28]
		push    eax

		call    decrementVATSFireShots
		add     esp, 0xC

		mov edx, 0x0089931A
		jmp edx
	}
}


void installBallisticMeleeHooks() {
	if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {
		// Hook TESObjectWEAP::IsMeleeWeapon call in HUDMainMenu::UpdateWeaponStatus
		WriteRelCall(0x007724CB, reinterpret_cast<std::uint32_t>(&Hook_IsMeleeWeapon));

		// Hook Actor::ObjectHit and Actor::CombatHit calls in Actor::MeleeAttack
		ObjectHitDetour.WriteRelCall(0x008997FE, reinterpret_cast<std::uint32_t>(&Hook_ObjectHit));
		CombatHitDetour.WriteRelCall(0x0089996D, reinterpret_cast<std::uint32_t>(&Hook_CombatHit));

		// Hook TESObjectWEAP::GetCurrentAmmo inside PlayerCharacter::CheckUserInputAttacks
		GetCurrentAmmoDetour.WriteRelCall(0x0094926A, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));
		GetCurrentAmmoDetour.WriteRelCall(0x009492B5, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));
		GetCurrentAmmoDetour.WriteRelCall(0x009492D7, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));
		GetCurrentAmmoDetour.WriteRelCall(0x00948E0E, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));
		GetCurrentAmmoDetour.WriteRelCall(0x00949E39, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));

		// Hook Animation::GetOffset call in Animation::Update
		// Resolves reloads not triggering after power attack
		GetOffsetDetour.WriteRelCall(0x00491E05, reinterpret_cast<std::uint32_t>(&Hook_GetOffset));

		// Hook Actor::HitMe to inject dummy projectile for JIP compatibility
		// Enables script effects/scripts when using JIP
		GetExtraDataDetour.WriteRelCall(0x0089AB06, reinterpret_cast<std::uint32_t>(&Hook_GetExtraData_JIP));
		WriteRelJump(0x0089AC5B, reinterpret_cast<std::uint32_t>(&RemoveHitDataProjectileHook_JIP));

		// Hook Actor::MeleeAttack and PlayerCharacter::CheckUserInputAttacks to decrease 
		// current VATS_COMMAND->cFireShots using weapon form cAmmoPerShot. Vanilla game will
		// attack for each melee/unarmed cAmmoPerShot in VATS.
		WriteRelJump(0x00948634, reinterpret_cast<std::uint32_t>(&DecrementFireShotsNonAutoHook));
		WriteRelJump(0x0089930B, reinterpret_cast<std::uint32_t>(&DecrementFireShotsAutoHook));
	}
}
