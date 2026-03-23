#pragma once
#include "SimpleIni.h"

namespace CommonLib
{
    class NiPoint3;
    class PlayerCharacter;
    PlayerCharacter* PlayerCharacterGetSingleton();
    extern NiPoint3 ZERO;
}

namespace Globals {
    extern CSimpleIniA g_Ini;
}
