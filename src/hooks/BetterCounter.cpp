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

constexpr char CONFIG_SECTION[] = "BetterCounter";

CallDetour GetFormClipRoundsDetour{};

constexpr std::uint32_t Actor_GetCurrentWeapon_Addr = 0x008A1710;
constexpr std::uint32_t TESObjectWEAP_GetFormClipRounds_Addr = 0x004FE160;
constexpr std::uint32_t Process_GetCurrentWeapon_Addr = 0x008D81E0;
constexpr std::uint32_t ItemChange_HasModEffectActive_Addr = 0x004BDA70;


int Hook_UIAmmoPrint(char* Buffer, size_t BufferCount, char* Format, ...)
{
	va_list args;
	va_start(args, Format);

	int clipCount = va_arg(args, int);
	int reserveCount = va_arg(args, int);

	bool hideCounter = Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bHideCounter");
	bool addClipToTotal = Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bAddClipToTotal");
	bool hideReserve = Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bHideReserve");
	bool showClipSize = Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bShowClipSize");
	bool replaceTotalWithMagCount = Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bReplaceTotalWithMagCount");

	const char* baseDisplay = "%i%s%i";
	const char* baseDisplayS = "%i%s%s";
	const char* noReserveDisplay = "%i";
	const char* clipCountDisplay = "%i%s%i%s%i";
	const char* clipCountDisplayS = "%i%s%s%s%i";

	const char* reserveSeperator = Globals::g_Ini.GetValue(CONFIG_SECTION, "sReserveSeperator", "/");
	const char* clipSeperator = Globals::g_Ini.GetValue(CONFIG_SECTION, "sClipSeperator", "|");

	CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacter::GetPlayerSingleton();
	CommonLib::ItemChange* weaponItemChange = ThisStdCall<CommonLib::ItemChange*>(Process_GetCurrentWeapon_Addr, pPlayer->pCurrentProcess);
	bool hasModEffectActive = ThisStdCall<bool>(
		ItemChange_HasModEffectActive_Addr,
		weaponItemChange,
		CommonLib::WEAPON_MOD_EFFECT::WEAPON_MOD_INCREASE_CLIP_SIZE
	);
	int clipSize = ThisStdCall<int>(TESObjectWEAP_GetFormClipRounds_Addr, weaponItemChange->pContainerObj, hasModEffectActive);

	if (hideCounter) {
		return snprintf(Buffer, BufferCount, "");
	}

	if (addClipToTotal) {
		reserveCount += clipCount;
	}

	if (replaceTotalWithMagCount && clipSize) {
		reserveCount /= clipSize;
	}

	if (hideReserve && !showClipSize) {
		return snprintf(Buffer, BufferCount, noReserveDisplay, clipCount);
	}

	if (hideReserve && showClipSize) {
		if (clipSize == (std::numeric_limits<int>::max)()) {
			return snprintf(Buffer, BufferCount, baseDisplayS, clipCount, clipSeperator, "INF");
		}
		return snprintf(Buffer, BufferCount, baseDisplay, clipCount, clipSeperator, clipSize);
	}

	if (!hideReserve && showClipSize) {
		if (clipSize == (std::numeric_limits<int>::max)()) {
			return snprintf(Buffer, BufferCount, clipCountDisplayS, clipCount, clipSeperator, "INF", reserveSeperator, reserveCount);
		}
		return snprintf(Buffer, BufferCount, clipCountDisplay, clipCount, clipSeperator, clipSize, reserveSeperator, reserveCount);
	}

	return snprintf(Buffer, BufferCount, baseDisplay, clipCount, reserveSeperator, reserveCount);
}


void installBetterCounterHooks() {
	if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {
		// Hook BSsprintf call in HUDMainMenu::UpdateWeaponStatus
		WriteRelCall(0x0077253D, reinterpret_cast<std::uint32_t>(&Hook_UIAmmoPrint));
		// Hook BSsprintf call in VATSMenu::UpdateAmmo
		WriteRelCall(0x007F3019, reinterpret_cast<std::uint32_t>(&Hook_UIAmmoPrint));
	}
}
