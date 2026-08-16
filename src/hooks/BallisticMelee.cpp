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
#include "Gamebryo/NiPoint3.hpp"
#include "BallisticMelee.hpp"
#include "Globals.hpp"

constexpr char CONFIG_SECTION[] = "BallisticMelee";

CallDetour ObjectHitDetour{};
CallDetour CombatHitDetour{};
CallDetour GetCurrentAmmoDetour{};
CallDetour ReduceDamageDetour{};
CallDetour GetOffsetDetour{};

constexpr std::uint32_t Actor_UseAmmo_Addr = 0x008A89A0;
constexpr std::uint32_t Actor_ShouldUseAmmo_Addr = 0x008A8DD0;
constexpr std::uint32_t Actor_GetCurrentWeapon_Addr = 0x008A1710;
constexpr std::uint32_t Projectile_Constructor_Addr = 0x009BBEF0;
constexpr std::uint32_t TESForm_SetTemporary_Addr = 0x00484490;
constexpr std::uint32_t Animation_PlayGroup_Addr = 0x00494740;
constexpr std::uint32_t PlayerCharacter_StartAnimOn1stPerson_Addr = 0x009520F0;
constexpr std::uint32_t Actor_SetAnimAction_Addr = 0x008A73E0;
constexpr std::uint32_t TESAnimGroup_GetTime_Addr = 0x005F3780;
constexpr std::uint32_t Animation_GetSequence_Addr = 0x00491040;
constexpr std::uint32_t BSAnimGroupSequence_GetAnimGroup_Addr = 0x0048F7F0;

bool isBallisticMelee(CommonLib::TESObjectWEAP* weapon) {
	return weapon->data.eType <= CommonLib::WEAPON_TYPE::WEAPON_TWO_HAND_MELEE && weapon->pFormAmmo;
}

bool __fastcall Hook_IsMeleeWeapon(CommonLib::TESObjectWEAP* weapon, void* edx)
{
	if (isBallisticMelee(weapon))
		return false;

	return weapon->data.eType <= CommonLib::WEAPON_TYPE::WEAPON_TWO_HAND_MELEE;
}

CommonLib::Tile* __fastcall Hook_ObjectHit(CommonLib::Actor* actor, void* edx, bool abPowerAttack)
{
	CommonLib::Tile* result = ThisStdCall<CommonLib::Tile*>(ObjectHitDetour.GetOverwrittenAddr(), actor, abPowerAttack);
	CommonLib::TESObjectWEAP* weapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, actor);
	bool shouldUseAmmo = ThisStdCall<bool>(Actor_ShouldUseAmmo_Addr, actor, weapon);

	if (shouldUseAmmo && weapon && isBallisticMelee(weapon)) {
		bool isAutomatic = (weapon->data.cFlags >> 1) & 1;
		if (result || isAutomatic) {
			ThisStdCall<void>(Actor_UseAmmo_Addr, actor, 1);
		}
	}

	return result;
}

void __fastcall Hook_CombatHit(
	CommonLib::Actor* actor,
	void* edx,
	CommonLib::Actor* apTarget,
	bool abPowerAttack,
	CommonLib::Projectile* apProjectile,
	char cMeleeEffect)
{
	ThisStdCall<void>(
		CombatHitDetour.GetOverwrittenAddr(),
		actor,
		apTarget,
		abPowerAttack,
		apProjectile,
		cMeleeEffect);

	CommonLib::TESObjectWEAP* weapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, actor);
	bool shouldUseAmmo = ThisStdCall<bool>(Actor_ShouldUseAmmo_Addr, actor, weapon);

	if (shouldUseAmmo && weapon && isBallisticMelee(weapon)) {
		ThisStdCall<void>(Actor_UseAmmo_Addr, actor, 1);
	}
}

CommonLib::TESAmmo* __fastcall Hook_GetCurrentAmmo(CommonLib::TESObjectWEAP* weapon, void* edx, CommonLib::Actor* apWeaponHolder)
{
	if (weapon && isBallisticMelee(weapon))
		return nullptr;

	return ThisStdCall<CommonLib::TESAmmo*>(GetCurrentAmmoDetour.GetOverwrittenAddr(), weapon, apWeaponHolder);
}

void __fastcall Hook_ReduceDamage(CommonLib::HitData* hitData, void* edx, bool abIgnoreBlocking)
{
	CommonLib::TESObjectWEAP* apFromWeapon = hitData->pWeapon;
	if (apFromWeapon && isBallisticMelee(apFromWeapon)) {
		CommonLib::BGSProjectile* apProjectileBase = apFromWeapon->data.pProjectile;
		CommonLib::TESObjectREFR* apShooter = hitData->pAggressor;

		if (apProjectileBase && apShooter) {
			CommonLib::Projectile* projectile = New<CommonLib::Projectile, Projectile_Constructor_Addr>(
				apProjectileBase,
				apShooter,
				apFromWeapon,
				&apShooter->data.Location, // Set projectile location to aggressor location for blocking calculations
				0.0,
				0.0
			);
			projectile->cFormType = CommonLib::ENUM_FORM_ID::PROJ_ID;
			// Set the refrenece as temporary to avoid crashing when saving base projectile
			ThisStdCall<void>(TESForm_SetTemporary_Addr, projectile);
			
			// Set projectile on HitData for JIP compatability, needed for ammo scripts and effects to work
			hitData->pSourceRef = projectile;
		}
	}

	ThisStdCall<void>(ReduceDamageDetour.GetOverwrittenAddr(), hitData, abIgnoreBlocking);
}


float __fastcall Hook_GetOffset(CommonLib::Animation* pAnimation, void* edx, CommonLib::BSAnimGroupSequence* apSequence)
{
	float fOffset = ThisStdCall<float>(GetOffsetDetour.GetOverwrittenAddr(), pAnimation, apSequence);

	std::uint8_t* ebp = GetParentBasePtr(_AddressOfReturnAddress(), false);
	CommonLib::TESObjectREFR* pOwnerObject = *reinterpret_cast<CommonLib::TESObjectREFR**>(ebp + 0x8);
	CommonLib::ANIM_GROUP_INFO* pAnimGroupInfo = reinterpret_cast<CommonLib::ANIM_GROUP_INFO*>(0x011977D8);

	CommonLib::ANIM_GROUP_SECTION eSection = *reinterpret_cast<CommonLib::ANIM_GROUP_SECTION*>(ebp - 0x28);

	std::uint8_t usAnimGroup = static_cast<std::uint8_t>(pAnimation->group[eSection]);
	CommonLib::ANIM_GROUP_INFO animGroupInfo = pAnimGroupInfo[usAnimGroup];

	if (pAnimation &&
		eSection == CommonLib::ANIM_GROUP_SECTION::AGS_WEAPON &&
		animGroupInfo.eAction == CommonLib::ANIM_GROUP_ACTION_TYPE::AGAT_ATTACK_POWER &&
		pAnimation->sQueuedReloadGroup != CommonLib::ANIM_GROUP_ENUM::ANIM_GROUP_NONE &&
		pAnimation->action[eSection] == CommonLib::ANIM_GROUP_ACTION::AGA_ATTACK_HIT
		)
	{
		CommonLib::BSAnimGroupSequence* pAnimGroupSequence = ThisStdCall<CommonLib::BSAnimGroupSequence*>(Animation_GetSequence_Addr, pAnimation, eSection);
		CommonLib::TESAnimGroup* pAnimGroup = ThisStdCall<CommonLib::TESAnimGroup*>(BSAnimGroupSequence_GetAnimGroup_Addr, pAnimGroupSequence);

		float fCurrActionTime = ThisStdCall<float>(TESAnimGroup_GetTime_Addr, pAnimGroup, pAnimation->action[eSection]);
		float fNextActionTime = ThisStdCall<float>(TESAnimGroup_GetTime_Addr, pAnimGroup, pAnimation->action[eSection] + 1);

		if (fOffset >= fCurrActionTime + (fNextActionTime - fCurrActionTime) * 0.5f) {
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

			CommonLib::TESObjectWEAP* pCurrentWeapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, pOwnerObject);
			if (pCurrentWeapon->bIsLoopingReload) {
				ThisStdCall<void>(Actor_SetAnimAction_Addr, pOwnerObject, CommonLib::ANIMATION_ACTION::ANIM_ACTION_RELOAD_LOOP, pAnimation->pCurrentSequence[eSection]);
			}
			else {
				ThisStdCall<void>(Actor_SetAnimAction_Addr, pOwnerObject, CommonLib::ANIMATION_ACTION::ANIM_ACTION_RELOAD, pAnimation->pCurrentSequence[eSection]);
			}
			
		}
	}

	return fOffset;
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

		// Hook HitData::ReduceDamage in HitData::InitializeHitData
		ReduceDamageDetour.WriteRelCall(0x009B5623, reinterpret_cast<std::uint32_t>(&Hook_ReduceDamage));

		// Hook TESObjectWEAP::IsMeleeWeapon call in HitData::ReduceDamage
		WriteRelCall(0x009B5F81, reinterpret_cast<std::uint32_t>(&Hook_IsMeleeWeapon));

		// Hook Animation::GetOffset call in Animation::Update
		// Resolves reloads not triggering after power attack
		GetOffsetDetour.WriteRelCall(0x00491E05, reinterpret_cast<std::uint32_t>(&Hook_GetOffset));
	}
}
