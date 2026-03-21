#include "globals.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Gamebryo/NiPoint3.hpp"

CommonLib::NiPoint3 ZERO = { 0.0f, 0.0f, 0.0f };

CommonLib::PlayerCharacter* PlayerCharacterGetSingleton()
{
	return *(CommonLib::PlayerCharacter**)0x011DEA3C;
}
