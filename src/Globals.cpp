#include "globals.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/VATS.hpp"
#include "Gamebryo/NiPoint3.hpp"

namespace CommonLib {
	NiPoint3 ZERO = { 0.0f, 0.0f, 0.0f };

	PlayerCharacter* PlayerCharacterGetSingleton()
	{
		return *(PlayerCharacter**)0x011DEA3C;
	}

	VATS* VATSGetSingleton()
	{
		return (VATS*)0x011F2250;
	}

	bool IsVanityMode() {
		return *(bool*)0x011E07B8;
	}
}

namespace Globals {
	CSimpleIniA g_Ini;
}
