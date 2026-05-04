#pragma once
#include <optional>

void installGuidedProjectilesHook();
void loadGuidedProjectilesConfig();

struct ConfigEntry {
    std::uint32_t weaponId = 0;
    std::uint32_t weaponModId = 0;
    std::uint32_t ammoId = 0;
    std::uint32_t projectileId = 0;
    float fRayCastRange = 10000.0f;
};
extern std::optional<ConfigEntry> currentGuidedProjConfig;
std::optional<ConfigEntry> getMatchingConfigEntry();
