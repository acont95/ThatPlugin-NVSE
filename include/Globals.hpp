#pragma once
#include "ConfigManager.hpp"

namespace CommonLib
{
    class NiPoint3;
    class PlayerCharacter;
    PlayerCharacter* PlayerCharacterGetSingleton();
    extern NiPoint3 ZERO;
}

namespace Globals {
    extern ConfigManager g_configManager;
}
