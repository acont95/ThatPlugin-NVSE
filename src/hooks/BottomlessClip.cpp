#include <limits>
#include "BottomlessClip.hpp"
#include "Globals.hpp"
#include "Bethesda/TESObjectWEAP.hpp"
#include "nvse/SafeWrite.h"

constexpr char CONFIG_SECTION[] = "BottomlessClip";

constexpr uint32_t TESObjectWEAP_GetModEffectValue_Addr = 0x004BCF60;

int __fastcall Hook_GetFormClipRounds(CommonLib::TESObjectWEAP* weapon, void* edx, bool bModValue)
{
	if (!weapon->cClipRounds) {
		return (std::numeric_limits<int>::max)();
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

void installBottomlessClipHooks() {
	if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {
		// Jump TESObjectWEAP::GetFormClipRounds function
		WriteRelJump(0x004FE160, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	}
}
