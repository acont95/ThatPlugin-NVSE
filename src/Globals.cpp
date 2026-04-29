#include "globals.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/VATS.hpp"
#include "Gamebryo/NiPoint3.hpp"

namespace CommonLib {
	bool IsVanityMode() {
		return *(bool*)0x011E07B8;
	}
}

namespace Globals {
	CSimpleIniA g_Ini;
}
