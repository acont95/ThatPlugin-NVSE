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
#include "Gamebryo/NiPoint3.hpp"
#include "BallisticMelee.hpp"
#include "Globals.hpp"

constexpr const char* CONFIG_SECTION = "BallisticMelee";

CallDetour ObjectHitDetour{};
CallDetour CombatHitDetour{};
CallDetour GetCurrentAmmoDetour{};
CallDetour ReduceDamageDetour{};

constexpr uint32_t Actor_UseAmmo_Addr = 0x008A89A0;
constexpr uint32_t Actor_ShouldUseAmmo_Addr = 0x008A8DD0;
constexpr uint32_t Actor_GetCurrentWeapon_Addr = 0x008A1710;
constexpr uint32_t Projectile_Constructor_Addr = 0x009BBEF0;

bool isBallisticMelee(CommonLib::TESObjectWEAP* weapon) {
	return weapon->data.eType <= 2 && weapon->pFormAmmo;
}

bool __fastcall Hook_IsMeleeWeapon(CommonLib::TESObjectWEAP* weapon, void* edx)
{
	if (isBallisticMelee(weapon))
		return false;

	return weapon->data.eType <= 2;
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
				&CommonLib::ZERO,
				0.0,
				0.0
			);
			projectile->cFormType = 0x33;
			hitData->pSourceRef = projectile;
		}
	}

	ThisStdCall<void>(ReduceDamageDetour.GetOverwrittenAddr(), hitData, abIgnoreBlocking);
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
	}
}
