#include <cmath>
#include "GuidedProjectiles.hpp"
#include "Globals.hpp"
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "Bethesda/bhkCharacterController.hpp"
#include "Bethesda/CFilter.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/TESObjectREFR.hpp"
#include "Bethesda/bhkWorld.hpp"
#include "Bethesda/bhkShapePhantom.hpp"
#include "Bethesda/Projectile.hpp"
#include "Bethesda/BSReference.hpp"
#include "Bethesda/ProjectileListener.hpp"
#include "Bethesda/MoveData.hpp"
#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiPoint3.hpp"
#include "Gamebryo/NiColorA.hpp"
#include "Gamebryo/NiAVObject.hpp"
#include "Gamebryo/NiNode.hpp"
#include "Gamebryo/NiPointer.hpp"
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
constexpr uint32_t bhkCharacterController_Move_Address = 0x00C73170;

constexpr float fScale = 50000.0f;

void LoadFunctionEAX(UInt32 jumpSrc, UInt32 jumpTgt)
{
    SafeWrite8(jumpSrc, 0xB8);
    SafeWrite32(jumpSrc + 1, jumpTgt);
    PatchMemoryNop(jumpSrc + 5, 1);
}

bool isProjectileGuided(CommonLib::Projectile* apProjectile) {
    CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacterGetSingleton();
    if (apProjectile && apProjectile->pShooter == pPlayer) {
        return true;
    }

    return false;
}

CommonLib::NiPoint3 getRayCastHitPoint(CommonLib::bhkCharacterController* apCharacterController) {
    CommonLib::NiCamera* camera = CdeclCall<CommonLib::NiCamera*>(Main_spWorldRoot_GetCamera_Address);
    const CommonLib::NiPoint3 start = camera->m_kWorld.m_Translate;
    CommonLib::NiPoint3 end{
        start.x + camera->m_kWorld.m_Rotate.m_pEntry[0].x * fScale,
        start.y + camera->m_kWorld.m_Rotate.m_pEntry[1].x * fScale,
        start.z + camera->m_kWorld.m_Rotate.m_pEntry[2].x * fScale
    };

    CommonLib::CFilter filter = CommonLib::CFilter{ 0 };
    filter = *ThisStdCall<CommonLib::CFilter*>(bhkCharacterController_GetCollisionFilter_Address, apCharacterController, &filter);

    CommonLib::hkpWorldRayCastInput rayCastInput = CommonLib::hkpWorldRayCastInput{};
    CommonLib::hkVector4 startVec = CommonLib::hkVector4{ _mm_set1_ps(0.0f) };
    CommonLib::hkVector4 endVec = CommonLib::hkVector4{ _mm_set1_ps(0.0f) };

    rayCastInput.m_from = *CdeclCall<CommonLib::hkVector4*>(hkVector4_FromPoint_Address, &startVec, &start);
    rayCastInput.m_to = *CdeclCall<CommonLib::hkVector4*>(hkVector4_FromPoint_Address, &endVec, &end);
    rayCastInput.m_filterInfo = filter.iFilter;

    CommonLib::hkpWorldRayCastOutput rayCastOutput = CommonLib::hkpWorldRayCastOutput{};

    CommonLib::bhkWorld* pBhkWorld = ThisStdCall<CommonLib::bhkWorld*>(bhkCharacterController_GetWorld_Address, apCharacterController);

    ThisStdCall<void>(hkpWorld_castRay_Address, reinterpret_cast<CommonLib::hkpWorld*>(pBhkWorld->phkObject), &rayCastInput, &rayCastOutput);

    CommonLib::NiPoint3 hitPoint{
        start.x + (end.x - start.x) * rayCastOutput.m_hitFraction,
        start.y + (end.y - start.y) * rayCastOutput.m_hitFraction,
        start.z + (end.z - start.z) * rayCastOutput.m_hitFraction
    };

    return hitPoint;
}

int __fastcall Hook_bhkCharacterController_Move(CommonLib::bhkCharacterController* apCharacterController, void* edx, CommonLib::MoveData* apMoveData)
{
    //if (apCharacterController && apMoveData) {

    CommonLib::ProjectileListener* listener = static_cast<CommonLib::ProjectileListener*>(apCharacterController);
    CommonLib::Projectile* projectile = listener->pProj;
    
    if (isProjectileGuided(projectile)) {
        CommonLib::NiPoint3 location = projectile->data.Location;
        CommonLib::NiPoint3 hitPoint = getRayCastHitPoint(listener);
        CommonLib::NiPoint3 direction{ hitPoint.x - location.x, hitPoint.y - location.y, hitPoint.z - location.z };

        float directionMag = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
        CommonLib::NiPoint3 newVelocity{ direction.x / directionMag, direction.y / directionMag, direction.z / directionMag };

        CommonLib::NiPoint3 currVelocity = apMoveData->Displacement;
        float currVelocityMag = std::sqrt(currVelocity.x * currVelocity.x + currVelocity.y * currVelocity.y + currVelocity.z * currVelocity.z);

        newVelocity.x *= currVelocityMag;
        newVelocity.y *= currVelocityMag;
        newVelocity.z *= currVelocityMag;
        apMoveData->Displacement = newVelocity;
    }
    //}

	return ThisStdCall<int>(0x00C73170, apCharacterController, apMoveData);
}

void installGuidedProjectilesHook() {
	//MoveAsProjectileDetour.WriteRelCall(0x0092FFF0, reinterpret_cast<std::uint32_t>(&Hook_bhkCharacterController_Move));
    LoadFunctionEAX(0x0092FFEA, reinterpret_cast<std::uint32_t>(&Hook_bhkCharacterController_Move));
}
