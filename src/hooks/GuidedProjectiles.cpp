#include "GuidedProjectiles.hpp"
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "Bethesda/bhkCharacterController.hpp"
#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiPoint3.hpp"
#include "Gamebryo/NiColorA.hpp"

CallDetour MoveAsProjectileDetour{};

constexpr uint32_t Main_spWorldRoot_GetCamera_Address = 0x00524C90;
constexpr uint32_t MakeLine_Address = 0x004B3890;
constexpr uint32_t TES_AddTempDebugObject_Address = 0x00458E20;

void __fastcall Hook_MoveAsProjectile(CommonLib::bhkCharacterController* apCharacterController, void* edx, float afDeltaTime)
{
    float fScale = 5000.0f;
	CommonLib::NiCamera* camera = CdeclCall<CommonLib::NiCamera*>(Main_spWorldRoot_GetCamera_Address);
    const CommonLib::NiPoint3 start = camera->m_kWorld.m_Translate;
    CommonLib::NiPoint3 end{};
    end.x = start.x + camera->m_kWorld.m_Rotate.m_pEntry[0].x * fScale;
    end.y = start.y + camera->m_kWorld.m_Rotate.m_pEntry[1].x * fScale;
    end.z = start.z + camera->m_kWorld.m_Rotate.m_pEntry[2].x * fScale;

    const CommonLib::NiColorA color{ 0.0f, 1.0f, 0.0f, 1.0f };

    Console_Print(
        "%.6f %.6f %.6f | %.6f %.6f %.6f | %.6f %.6f %.6f",
        camera->m_kWorld.m_Rotate.m_pEntry[0].x,
        camera->m_kWorld.m_Rotate.m_pEntry[0].y,
        camera->m_kWorld.m_Rotate.m_pEntry[0].z,
        camera->m_kWorld.m_Rotate.m_pEntry[1].x,
        camera->m_kWorld.m_Rotate.m_pEntry[1].y,
        camera->m_kWorld.m_Rotate.m_pEntry[1].z,
        camera->m_kWorld.m_Rotate.m_pEntry[2].x,
        camera->m_kWorld.m_Rotate.m_pEntry[2].y,
        camera->m_kWorld.m_Rotate.m_pEntry[2].z
    );

    void* line = CdeclCall<void*>(MakeLine_Address, &start, &color, &end, &color, true);
    void* tes = *(void**)0x11DEA10;
    ThisStdCall<void>(TES_AddTempDebugObject_Address, tes, line, 1.0f);

	ThisStdCall<void>(MoveAsProjectileDetour.GetOverwrittenAddr(), apCharacterController, afDeltaTime);
}

void installGuidedProjectilesHook() {
	MoveAsProjectileDetour.WriteRelCall(0x00C73832, reinterpret_cast<std::uint32_t>(&Hook_MoveAsProjectile));
}
