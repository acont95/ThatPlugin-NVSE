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
#include "Globals.hpp"

CallDetour GetFormClipRoundsDetour{};

constexpr uint32_t InventoryChanges_GetInventoryChanges_Addr = 0x004BF220;
constexpr uint32_t InventoryChanges_GetObjectCount_Addr = 0x004C8F30;
constexpr uint32_t TESObjectWEAP_GetCurrentAmmo_Addr = 0x00525980;
constexpr uint32_t Actor_GetCurrentWeapon_Addr = 0x008A1710;
constexpr uint32_t TESObjectWEAP_GetModEffectValue_Addr = 0x004BCF60;



int Hook_UIWweaponPrint(char* Buffer, size_t BufferCount, char* Format, ...)
{
	CommonLib::PlayerCharacter* pPlayer = PlayerCharacterGetSingleton();
	CommonLib::TESObjectWEAP* weapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, pPlayer);
	va_list args;
	va_start(args, Format);

	if (weapon && !(weapon->cClipRounds)) {
		//va_arg(args, int);
		return vsprintf_s(Buffer, BufferCount, "%i", args);
	}

	return vsprintf_s(Buffer, BufferCount, Format, args);
}

int getWeaponAmmoCount(CommonLib::TESObjectWEAP* weapon) {
	CommonLib::PlayerCharacter* pPlayer = PlayerCharacterGetSingleton();
	CommonLib::TESAmmo* ammo = ThisStdCall<CommonLib::TESAmmo*>(TESObjectWEAP_GetCurrentAmmo_Addr, weapon, pPlayer);
	CommonLib::InventoryChanges* inventoryChanges = CdeclCall<CommonLib::InventoryChanges*>(InventoryChanges_GetInventoryChanges_Addr, pPlayer);
	int objectCount = ThisStdCall<int>(InventoryChanges_GetObjectCount_Addr, inventoryChanges, ammo);

	return objectCount;
}

int __fastcall Hook_GetFormClipRounds(CommonLib::TESObjectWEAP* weapon, void* edx, bool bModValue)
{
	if (!weapon->cClipRounds) {
		return getWeaponAmmoCount(weapon);
	}

	if (bModValue) {
		float modEffectValue = ThisStdCall<float>(
			TESObjectWEAP_GetModEffectValue_Addr,
			weapon,
			CommonLib::TESObjectWEAP::WEAPON_MOD_EFFECT::WEAPON_MOD_INCREASE_CLIP_SIZE,
			0
		);
		return modEffectValue + weapon->cClipRounds;
	}

	return weapon->cClipRounds;
}

int __fastcall Hook_GetClipRounds(CommonLib::BGSClipRoundsForm* clipRoundsForm)
{
	if (!(clipRoundsForm->cClipRounds)) {
		return uint8_t(1);
	}
	return clipRoundsForm->cClipRounds;
}

void installEndlessMagHooks() {
	// Hook BSsprintf call in HUDMainMenu::UpdateWeaponStatus
	WriteRelCall(0x0077253D, reinterpret_cast<std::uint32_t>(&Hook_UIWweaponPrint));
	// Hook BSsprintf call in VATSMenu::UpdateAmmo
	WriteRelCall(0x007F3019, reinterpret_cast<std::uint32_t>(&Hook_UIWweaponPrint));
	// Jump TESObjectWEAP::GetFormClipRounds function
	//WriteRelJump(0x004FE160, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	// Jump BGSClipRoundsForm::GetClipRounds function
	WriteRelJump(0x00401170, reinterpret_cast<std::uint32_t>(&Hook_GetClipRounds));
	PatchMemoryNop(0x00401175, 2);

	//GetFormClipRoundsDetour.WriteRelCall(0x004FE113, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x00645679, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x00645FC7, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x0070869B, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x00772421, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x007EDB41, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x007EE591, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x007F2A65, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x007F2A9B, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x007F2AB6, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x007F2B8A, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x007F2BDE, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x007F2CAD, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x007F2D10, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x0088CE83, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x00892CE9, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x008A85D5, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x008A86D3, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x008A86E5, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x008A8955, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x008BAAD4, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x008F7F3E, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x00943D65, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x00948C20, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x0095D656, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x0095D690, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x0095DA70, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x0095DABE, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	//GetFormClipRoundsDetour.WriteRelCall(0x0095DBDA, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
}
