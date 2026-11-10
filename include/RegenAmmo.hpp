#pragma once
#include <cstdint>
#include "NVSE/PluginAPI.h"
#include "NVSE/NiTypes.h"

void installRegenAmmoHooks();

class ActorRegenTickExtraData: public PluginFormExtraData { 
public:
    ActorRegenTickExtraData() : PluginFormExtraData(ActorRegenTickExtraData::GetName()) {}
    virtual ~ActorRegenTickExtraData() override = default;
    float fTimeSinceLastAmmoRegenTick = 0.0f;
    std::uint8_t cShotsSinceLastAmmoRegen = 0;
    static const NiFixedString& GetName() { static NiFixedString name = "MyFormExtraData"; return name; }
};
