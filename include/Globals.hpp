#pragma once
#include "SimpleIni.h"
#include "nvse/PluginAPI.h"

bool IsVanityMode();

namespace Globals {
    extern CSimpleIniA g_Ini;
    extern NVSEMessagingInterface* g_messagingInterface;
    extern NVSEInterface* g_nvseInterface;
    extern NVSEEventManagerInterface* g_eventInterface;
    extern NVSEDataInterface* g_dataInterface;
}
