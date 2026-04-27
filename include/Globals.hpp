#pragma once
#include "SimpleIni.h"

namespace CommonLib
{
    class NiPoint3;
    class PlayerCharacter;
    class VATS;

    PlayerCharacter* PlayerCharacterGetSingleton();
    VATS* VATSGetSingleton();
    bool IsVanityMode();
    extern NiPoint3 ZERO;
}

namespace Globals {
    extern CSimpleIniA g_Ini;
}
