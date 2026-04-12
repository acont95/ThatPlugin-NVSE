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
#include "Gamebryo/NiMatrix3.hpp"
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

CommonLib::NiMatrix3::NiMatrix3() {
    m_pEntry[0] = NiPoint3{ 0.0f, 0.0f, 0.0f };
    m_pEntry[1] = NiPoint3{ 0.0f, 0.0f, 0.0f };
    m_pEntry[2] = NiPoint3{ 0.0f, 0.0f, 0.0f };
}

CommonLib::NiMatrix3::~NiMatrix3() = default;

constexpr uint32_t Main_spWorldRoot_GetCamera_Address = 0x00524C90;
constexpr uint32_t hkpWorld_castRay_Address = 0x00C92040;
constexpr uint32_t bhkCharacterController_GetWorld_Address = 0x00621AD0;
constexpr uint32_t hkVector4_FromPoint_Address = 0x004A3E00;
constexpr uint32_t bhkCharacterController_GetCollisionFilter_Address = 0x0070C440;
constexpr uint32_t bhkCharacterController_Move_Address = 0x00C73170;
constexpr uint32_t bhkCharacterController_FindBSReference_Address = 0x00C6DEC0;
constexpr uint32_t TESObjectREFR_IsProjectile_Address = 0x005725B0;
constexpr uint32_t NiMatrix3_MakeXRotation_Address = 0x00524AC0;
constexpr uint32_t NiMatrix3_MakeYRotation_Address = 0x0043F850;
constexpr uint32_t NiMatrix3_MakeZRotation_Address = 0x004A0C90;
constexpr uint32_t TESObjectREFR_Get3D_Address = 0x0043FCD0;

constexpr uint32_t MakeLine_Address = 0x004B3890;
constexpr uint32_t TES_AddTempDebugObject_Address = 0x00458E20;

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
    CommonLib::TESObjectREFR* bsRef = CdeclCall<CommonLib::TESObjectREFR*>(
        bhkCharacterController_FindBSReference_Address, 
        1000u, 
        apCharacterController->spShapePhantom.m_pObject->phkObject
    );

    if (bsRef && ThisStdCall<bool>(TESObjectREFR_IsProjectile_Address, bsRef)) {
        CommonLib::Projectile* projectile = static_cast<CommonLib::Projectile*>(bsRef);

        if (isProjectileGuided(projectile)) {
            CommonLib::NiPoint3 projectileLocation = projectile->data.Location;
            CommonLib::NiAVObject* projectile3d = ThisStdCall<CommonLib::NiAVObject*>(TESObjectREFR_Get3D_Address, projectile);

            CommonLib::NiPoint3 hitPoint = getRayCastHitPoint(apCharacterController);
            CommonLib::NiPoint3 direction{ hitPoint.x - projectileLocation.x, hitPoint.y - projectileLocation.y, hitPoint.z - projectileLocation.z };

            float directionMag = std::sqrt(direction.x * direction.x + direction.y * direction.y + direction.z * direction.z);
            CommonLib::NiPoint3 directionNormal{ direction.x / directionMag, direction.y / directionMag, direction.z / directionMag };

            CommonLib::NiMatrix3 rotation = projectile3d->m_kLocal.m_Rotate;


            Console_Print("ROT MATRIX:");

            Console_Print(
                "%.6f %.6f %.6f",
                rotation.m_pEntry[0].x, rotation.m_pEntry[0].y, rotation.m_pEntry[0].z
            );

            Console_Print(
                "%.6f %.6f %.6f",
                rotation.m_pEntry[1].x, rotation.m_pEntry[1].y, rotation.m_pEntry[1].z
            );

            Console_Print(
                "%.6f %.6f %.6f",
                rotation.m_pEntry[2].x, rotation.m_pEntry[2].y, rotation.m_pEntry[2].z
            );

            CommonLib::NiPoint3 directionLocal
            {
                // X
                rotation.m_pEntry[0].x * directionNormal.x +
                rotation.m_pEntry[1].x * directionNormal.y +
                rotation.m_pEntry[2].x * directionNormal.z,

                // Y
                rotation.m_pEntry[0].y * directionNormal.x +
                rotation.m_pEntry[1].y * directionNormal.y +
                rotation.m_pEntry[2].y * directionNormal.z,

                // Z
                rotation.m_pEntry[0].z * directionNormal.x +
                rotation.m_pEntry[1].z * directionNormal.y +
                rotation.m_pEntry[2].z * directionNormal.z
            };


            CommonLib::NiPoint3 angleLocal
            {
                // X
                rotation.m_pEntry[0].x * apMoveData->Angle.x +
                rotation.m_pEntry[1].x * apMoveData->Angle.y +
                rotation.m_pEntry[2].x * apMoveData->Angle.z,

                // Y
                rotation.m_pEntry[0].y * apMoveData->Angle.x +
                rotation.m_pEntry[1].y * apMoveData->Angle.y +
                rotation.m_pEntry[2].y * apMoveData->Angle.z,

                // Z
                rotation.m_pEntry[0].z * apMoveData->Angle.x +
                rotation.m_pEntry[1].z * apMoveData->Angle.y +
                rotation.m_pEntry[2].z * apMoveData->Angle.z
            };


            CommonLib::NiPoint3 currDisplacement = apMoveData->Displacement;
            float currDisplacementMag = std::sqrt(currDisplacement.x * currDisplacement.x + currDisplacement.y * currDisplacement.y + currDisplacement.z * currDisplacement.z);

            directionLocal.x *= currDisplacementMag;
            directionLocal.y *= currDisplacementMag;
            directionLocal.z *= currDisplacementMag;

            Console_Print("CURRENT MAG %.6f", currDisplacementMag);
            Console_Print("BEFORE %.6f %.6f %.6f", apMoveData->Displacement.x, apMoveData->Displacement.y, apMoveData->Displacement.z);
            apMoveData->Displacement.x = directionLocal.x;
            apMoveData->Displacement.y = directionLocal.y;
            apMoveData->Displacement.z = directionLocal.z;
            Console_Print("Angle %.6f %.6f %.6f", apMoveData->Angle.x, apMoveData->Angle.y, apMoveData->Angle.z);
            apMoveData->Angle = angleLocal;
            Console_Print("Angle New %.6f %.6f %.6f", apMoveData->Angle.x, apMoveData->Angle.y, apMoveData->Angle.z);


            const CommonLib::NiColorA color{ 0.0f, 1.0f, 0.0f, 1.0f };
            void* line = CdeclCall<void*>(MakeLine_Address, &projectileLocation, &color, &hitPoint, &color, true);
            void* tes = *(void**)0x11DEA10;
            ThisStdCall<void>(TES_AddTempDebugObject_Address, tes, line, 1.0f);
        }
    }

	return ThisStdCall<int>(0x00C73170, apCharacterController, apMoveData);
}


void installGuidedProjectilesHook() {
	//MoveAsProjectileDetour.WriteRelCall(0x0092FFF0, reinterpret_cast<std::uint32_t>(&Hook_bhkCharacterController_Move));
    LoadFunctionEAX(0x0092FFEA, reinterpret_cast<std::uint32_t>(&Hook_bhkCharacterController_Move));
}
