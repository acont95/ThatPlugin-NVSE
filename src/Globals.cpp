#include "globals.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/VATS.hpp"
#include "Gamebryo/NiPoint3.hpp"

bool IsVanityMode() {
	return *(bool*)0x011E07B8;
}

namespace Globals {
	CSimpleIniA g_Ini;
    NVSEMessagingInterface* g_messagingInterface{};
    NVSEInterface* g_nvseInterface{};
    NVSEEventManagerInterface* g_eventInterface{};
    NVSEDataInterface* g_dataInterface{};
}
