#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <limits>
#include <string>
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "nvse/GameRTTI.h"
#include "Bethesda/Actor.hpp"
#include "Bethesda/HitData.hpp"
#include "Bethesda/TESAmmo.hpp"
#include "Bethesda/TESObjectWEAP.hpp"
#include "Bethesda/Projectile.hpp"
#include "Bethesda/Tile.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/BaseProcess.hpp"
#include "Bethesda/InventoryChanges.hpp"
#include "Bethesda/ItemChange.hpp"
#include "Gamebryo/NiPoint3.hpp"
#include "Globals.hpp"
#include "ConfigManager.hpp"

extern 

constexpr const char* CONFIG_SECTION = "BetterCounter";

CallDetour GetFormClipRoundsDetour{};

constexpr uint32_t InventoryChanges_GetInventoryChanges_Addr = 0x004BF220;
constexpr uint32_t InventoryChanges_GetObjectCount_Addr = 0x004C8F30;
constexpr uint32_t TESObjectWEAP_GetCurrentAmmo_Addr = 0x00525980;
constexpr uint32_t Actor_GetCurrentWeapon_Addr = 0x008A1710;
constexpr uint32_t TESObjectWEAP_GetModEffectValue_Addr = 0x004BCF60;
constexpr uint32_t Process_GetCurrentAmmo_Addr = 0x005F7590;


int Hook_UIAmmoPrint(char* Buffer, size_t BufferCount, char* Format, ...)
{
	ConfigManager cm = Globals::g_configManager;

	CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacterGetSingleton();
	CommonLib::TESObjectWEAP* weapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, pPlayer);

	va_list args;
	va_start(args, Format);

	int clipCount = va_arg(args, int);
	int reserveCount = va_arg(args, int);

	Console_Print("%i", BufferCount);

	const char* seperator = cm.getKey<const char*>(CONFIG_SECTION, "cSeperator", "/");

	if (cm.getKey<bool>(CONFIG_SECTION, "bHideCounter")) {
		return snprintf(Buffer, BufferCount, "");
	}

	if (cm.getKey<bool>(CONFIG_SECTION, "bHideReserve")) {
		return snprintf(Buffer, BufferCount, "%i", clipCount);
	}

	if (cm.getKey<bool>(CONFIG_SECTION, "bCurrentTotalStyle")) {
		reserveCount += clipCount;
	}

	return snprintf(Buffer, BufferCount, "%i%s%i", clipCount, seperator, reserveCount);
}


void installBetterCounterHooks() {
	if (Globals::g_configManager.getKey<bool>(CONFIG_SECTION, "bEnabled")) {
		// Hook BSsprintf call in HUDMainMenu::UpdateWeaponStatus
		WriteRelCall(0x0077253D, reinterpret_cast<std::uint32_t>(&Hook_UIAmmoPrint));
		// Hook BSsprintf call in VATSMenu::UpdateAmmo
		WriteRelCall(0x007F3019, reinterpret_cast<std::uint32_t>(&Hook_UIAmmoPrint));
	}
}
