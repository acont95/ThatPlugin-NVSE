#include "GuidedProjectiles.hpp"
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "Bethesda/bhkCharacterController.hpp"
#include "Bethesda/CFilter.hpp"
#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiPoint3.hpp"
#include "Gamebryo/NiColorA.hpp"
#include "Havok/hkVector4.hpp"
#include "Havok/hkBool.hpp"
#include "Havok/hkpWorldRayCastInput.hpp"
#include "Havok/hkpWorldRayCastOutput.hpp"
#include "Havok/hkpWorld.hpp"

CallDetour MoveAsProjectileDetour{};

CommonLib::hkpWorldRayCastInput::hkpWorldRayCastInput() : 
    m_enableShapeCollectionFilter(false),
    m_filterInfo(0)
{
    m_from.m_quad = _mm_setzero_ps();
    m_to.m_quad = _mm_setzero_ps();
}
CommonLib::hkpWorldRayCastInput::~hkpWorldRayCastInput() = default;

CommonLib::hkpShapeRayCastCollectorOutput::hkpShapeRayCastCollectorOutput()
    : m_hitFraction(1.0),
    m_extraInfo(-1),
    m_pad{ 0, 0 }
{
    m_normal.m_quad = _mm_setzero_ps();
}

CommonLib::hkpShapeRayCastCollectorOutput::~hkpShapeRayCastCollectorOutput() = default;

CommonLib::hkpShapeRayCastOutput::hkpShapeRayCastOutput()
    : hkpShapeRayCastCollectorOutput(),
    m_shapeKeys{
        static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1),
        static_cast<uint32_t>(-1)
    },
    m_shapeKeyIndex(0) {}

CommonLib::hkpShapeRayCastOutput::~hkpShapeRayCastOutput() = default;

CommonLib::hkpWorldRayCastOutput::hkpWorldRayCastOutput()
    : hkpShapeRayCastOutput(), 
    m_rootCollidable(nullptr) {}
CommonLib::hkpWorldRayCastOutput::~hkpWorldRayCastOutput() = default;

constexpr uint32_t Main_spWorldRoot_GetCamera_Address = 0x00524C90;
constexpr uint32_t hkpWorld_castRay_Address = 0x00C92040;
constexpr uint32_t bhkCharacterController_GetWorld_Address = 0x00621AD0;
constexpr uint32_t hkVector4_FromPoint_Address = 0x004A3E00;
constexpr uint32_t bhkCharacterController_GetCollisionFilter_Address = 0x0070C440;

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

    CommonLib::CFilter filter = CommonLib::CFilter{ 0 };
    filter = *ThisStdCall<CommonLib::CFilter*>(bhkCharacterController_GetCollisionFilter_Address, apCharacterController, &filter);

    CommonLib::hkpWorldRayCastInput rayCastInput = CommonLib::hkpWorldRayCastInput{};
    rayCastInput.m_from = *CdeclCall<CommonLib::hkVector4*>(hkVector4_FromPoint_Address, &start);
    rayCastInput.m_to = *CdeclCall<CommonLib::hkVector4*>(hkVector4_FromPoint_Address, &end);
    rayCastInput.m_filterInfo = filter.iFilter;

    CommonLib::hkpWorldRayCastOutput rayCastOutput = CommonLib::hkpWorldRayCastOutput{};

    CommonLib::hkpWorld* pHkpWorld = ThisStdCall<CommonLib::hkpWorld*>(bhkCharacterController_GetWorld_Address, apCharacterController);

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
