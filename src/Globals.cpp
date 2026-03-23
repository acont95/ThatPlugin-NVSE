#include "globals.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Gamebryo/NiPoint3.hpp"

namespace CommonLib {
	NiPoint3 ZERO = { 0.0f, 0.0f, 0.0f };

	PlayerCharacter* PlayerCharacterGetSingleton()
	{
		return *(PlayerCharacter**)0x011DEA3C;
	}
}

namespace Globals {
	ConfigManager g_configManager{};
}
