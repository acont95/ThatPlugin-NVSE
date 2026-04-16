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
#include "Bethesda/bhkCharacterStateProjectile.hpp"
#include "Bethesda/bhkShapePhantom.hpp"
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
#include "Havok/hkRotation.hpp"
#include "Havok/hkpShapePhantom.hpp"

CallDetour GetCharacterStateDetour{};
CallDetour QUseZDetour{};

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
constexpr uint32_t bhkCharacterController_FindBSReference_Address = 0x00C6DEC0;
constexpr uint32_t TESObjectREFR_IsProjectile_Address = 0x005725B0;
constexpr uint32_t bhkCharacterController_GetPosition_Address = 0x00C6E300;
constexpr uint32_t hkVector4_setRotatedDir_Address = 0x00C7F700;
constexpr uint32_t hkMatrix3_invert_Address = 0x00CD4F10;

constexpr uint32_t MakeLine_Address = 0x004B3890;
constexpr uint32_t TES_AddTempDebugObject_Address = 0x00458E20;

constexpr bool bDebugRayCast = true;
constexpr float fScale = 50000.0f;
constexpr float fHk2BSScaleSC_639 = 69.99125671386719 / 10.0;

bool isProjectileGuided(CommonLib::Projectile* apProjectile) {
    CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacterGetSingleton();
    if (apProjectile && apProjectile->pShooter == pPlayer) {
        return true;
    }

    return false;
}

CommonLib::hkVector4 getCameraRayCastHitPoint(CommonLib::bhkCharacterController* apCharacterController) {
    CommonLib::NiCamera* camera = CdeclCall<CommonLib::NiCamera*>(Main_spWorldRoot_GetCamera_Address);
    const CommonLib::NiPoint3 niStart = camera->m_kWorld.m_Translate;
    CommonLib::NiPoint3 niEnd{
        niStart.x + camera->m_kWorld.m_Rotate.m_pEntry[0].x * fScale,
        niStart.y + camera->m_kWorld.m_Rotate.m_pEntry[1].x * fScale,
        niStart.z + camera->m_kWorld.m_Rotate.m_pEntry[2].x * fScale
    };

    CommonLib::CFilter filter = CommonLib::CFilter{ 0 };
    filter = *ThisStdCall<CommonLib::CFilter*>(bhkCharacterController_GetCollisionFilter_Address, apCharacterController, &filter);

    CommonLib::hkpWorldRayCastInput rayCastInput = CommonLib::hkpWorldRayCastInput{};
    CommonLib::hkVector4 startVec = CommonLib::hkVector4{ _mm_set1_ps(0.0f) };
    CommonLib::hkVector4 endVec = CommonLib::hkVector4{ _mm_set1_ps(0.0f) };

    rayCastInput.m_from = *CdeclCall<CommonLib::hkVector4*>(hkVector4_FromPoint_Address, &startVec, &niStart);
    rayCastInput.m_to = *CdeclCall<CommonLib::hkVector4*>(hkVector4_FromPoint_Address, &endVec, &niEnd);
    rayCastInput.m_filterInfo = filter.iFilter;

    CommonLib::hkpWorldRayCastOutput rayCastOutput = CommonLib::hkpWorldRayCastOutput{};

    CommonLib::bhkWorld* pBhkWorld = ThisStdCall<CommonLib::bhkWorld*>(bhkCharacterController_GetWorld_Address, apCharacterController);

    ThisStdCall<void>(hkpWorld_castRay_Address, reinterpret_cast<CommonLib::hkpWorld*>(pBhkWorld->phkObject), &rayCastInput, &rayCastOutput);

    CommonLib::hkVector4 hitPoint{
        startVec.m_quad.m128_f32[0] + (endVec.m_quad.m128_f32[0] - startVec.m_quad.m128_f32[0]) * rayCastOutput.m_hitFraction,
        startVec.m_quad.m128_f32[1] + (endVec.m_quad.m128_f32[1] - startVec.m_quad.m128_f32[1]) * rayCastOutput.m_hitFraction,
        startVec.m_quad.m128_f32[2] + (endVec.m_quad.m128_f32[2] - startVec.m_quad.m128_f32[2]) * rayCastOutput.m_hitFraction,
        0.0f
    };

    return hitPoint;
}

void debugRayCast(CommonLib::hkVector4 projectileLocation, CommonLib::hkVector4 hitPoint) {
    const CommonLib::NiColorA color{ 0.0f, 1.0f, 0.0f, 1.0f };
    CommonLib::NiPoint3 niLoc{
        projectileLocation.m_quad.m128_f32[0] * fHk2BSScaleSC_639,
        projectileLocation.m_quad.m128_f32[1] * fHk2BSScaleSC_639,
        projectileLocation.m_quad.m128_f32[2] * fHk2BSScaleSC_639
    };
    CommonLib::NiPoint3 niHit{
        hitPoint.m_quad.m128_f32[0] * fHk2BSScaleSC_639,
        hitPoint.m_quad.m128_f32[1] * fHk2BSScaleSC_639,
        hitPoint.m_quad.m128_f32[2] * fHk2BSScaleSC_639
    };
    void* line = CdeclCall<void*>(MakeLine_Address, &niLoc, &color, &niHit, &color, true);
    void* tes = *(void**)0x11DEA10;
    ThisStdCall<void>(TES_AddTempDebugObject_Address, tes, line, 1.0f);
}

void printVector(CommonLib::hkVector4 vec, const char* name) {
    Console_Print(
        "%s %.6f %.6f %.6f %.6f",
        name,
        vec.m_quad.m128_f32[0],
        vec.m_quad.m128_f32[1],
        vec.m_quad.m128_f32[2],
        vec.m_quad.m128_f32[3]
    );
}

CommonLib::hkVector4 hkVector4Cross(const CommonLib::hkVector4 vec1, const CommonLib::hkVector4 vec2) {
    CommonLib::hkVector4 result{ 
        vec1.m_quad.m128_f32[1] * vec2.m_quad.m128_f32[2] - vec1.m_quad.m128_f32[2] * vec2.m_quad.m128_f32[1],
        vec1.m_quad.m128_f32[2] * vec2.m_quad.m128_f32[0] - vec1.m_quad.m128_f32[0] * vec2.m_quad.m128_f32[2],
        vec1.m_quad.m128_f32[0] * vec2.m_quad.m128_f32[1] - vec1.m_quad.m128_f32[1] * vec2.m_quad.m128_f32[0],
        0.0f
    };

    return result;
}


static inline CommonLib::hkMatrix3 TransposeMatrix3x3(const CommonLib::hkMatrix3& m)
{
    CommonLib::hkMatrix3 out;

    // row 0 becomes col 0
    out.m_col0.m_quad.m128_f32[0] = m.m_col0.m_quad.m128_f32[0];
    out.m_col0.m_quad.m128_f32[1] = m.m_col1.m_quad.m128_f32[0];
    out.m_col0.m_quad.m128_f32[2] = m.m_col2.m_quad.m128_f32[0];
    out.m_col0.m_quad.m128_f32[3] = 0.0f;

    // row 1 becomes col 1
    out.m_col1.m_quad.m128_f32[0] = m.m_col0.m_quad.m128_f32[1];
    out.m_col1.m_quad.m128_f32[1] = m.m_col1.m_quad.m128_f32[1];
    out.m_col1.m_quad.m128_f32[2] = m.m_col2.m_quad.m128_f32[1];
    out.m_col1.m_quad.m128_f32[3] = 0.0f;

    // row 2 becomes col 2
    out.m_col2.m_quad.m128_f32[0] = m.m_col0.m_quad.m128_f32[2];
    out.m_col2.m_quad.m128_f32[1] = m.m_col1.m_quad.m128_f32[2];
    out.m_col2.m_quad.m128_f32[2] = m.m_col2.m_quad.m128_f32[2];
    out.m_col2.m_quad.m128_f32[3] = 0.0f;

    return out;
}


CommonLib::bhkCharacterStateProjectile* __fastcall Hook_bhkCharacterController_GetCharacterState(CommonLib::bhkCharacterController* apCharacterController)
{
    CommonLib::TESObjectREFR* bsRef = CdeclCall<CommonLib::TESObjectREFR*>(
        bhkCharacterController_FindBSReference_Address,
        1000u,
        apCharacterController->spShapePhantom.m_pObject->phkObject
    );

    if (bsRef && ThisStdCall<bool>(TESObjectREFR_IsProjectile_Address, bsRef)) {
        CommonLib::Projectile* projectile = static_cast<CommonLib::Projectile*>(bsRef);

        if (isProjectileGuided(projectile)) {
            CommonLib::hkVector4 projectileLocation{ _mm_setzero_ps() };
            ThisStdCall<void>(bhkCharacterController_GetPosition_Address, apCharacterController, &projectileLocation);

            CommonLib::hkVector4 hitPoint = getCameraRayCastHitPoint(apCharacterController);

            if (bDebugRayCast) debugRayCast(projectileLocation, hitPoint);

            CommonLib::hkVector4 forward = apCharacterController->ForwardVec;
            forward.m_quad.m128_f32[0] *= -1.0f;
            forward.m_quad.m128_f32[1] *= -1.0f;
            forward.m_quad.m128_f32[2] *= -1.0f;
            forward.m_quad.m128_f32[3] *= -1.0f;
            CommonLib::hkVector4 up = apCharacterController->UpVec;
            CommonLib::hkVector4 right = hkVector4Cross(forward, up);

            CommonLib::hkRotation rotationMatrix = { right, forward, up};
            ThisStdCall<void>(hkMatrix3_invert_Address, &rotationMatrix, 0.00000011920929f);

            CommonLib::hkVector4 localVelocityOld = apCharacterController->Direction;
            float localVelocityOldMag = std::sqrt(
                localVelocityOld.m_quad.m128_f32[0] * localVelocityOld.m_quad.m128_f32[0]
                + localVelocityOld.m_quad.m128_f32[1] * localVelocityOld.m_quad.m128_f32[1]
                + localVelocityOld.m_quad.m128_f32[2] * localVelocityOld.m_quad.m128_f32[2]
                + localVelocityOld.m_quad.m128_f32[3] * localVelocityOld.m_quad.m128_f32[3]
            );

            CommonLib::hkVector4 globalVelocityNew{
                hitPoint.m_quad.m128_f32[0] - projectileLocation.m_quad.m128_f32[0],
                hitPoint.m_quad.m128_f32[1] - projectileLocation.m_quad.m128_f32[1],
                hitPoint.m_quad.m128_f32[2] - projectileLocation.m_quad.m128_f32[2],
                0.0f
            };

            float globalVelocityMag = std::sqrt(
                globalVelocityNew.m_quad.m128_f32[0] * globalVelocityNew.m_quad.m128_f32[0]
                + globalVelocityNew.m_quad.m128_f32[1] * globalVelocityNew.m_quad.m128_f32[1]
                + globalVelocityNew.m_quad.m128_f32[2] * globalVelocityNew.m_quad.m128_f32[2]
            );

            globalVelocityNew.m_quad.m128_f32[0] /= globalVelocityMag;
            globalVelocityNew.m_quad.m128_f32[1] /= globalVelocityMag;
            globalVelocityNew.m_quad.m128_f32[2] /= globalVelocityMag;
            globalVelocityNew.m_quad.m128_f32[3] = 0.0f;

            CommonLib::hkVector4 localVelocityNew{ _mm_set1_ps(0.0f) };
            ThisStdCall<void>(hkVector4_setRotatedDir_Address, &localVelocityNew, &rotationMatrix, &globalVelocityNew);

            localVelocityNew.m_quad.m128_f32[0] *= localVelocityOldMag;
            localVelocityNew.m_quad.m128_f32[1] *= localVelocityOldMag;
            localVelocityNew.m_quad.m128_f32[2] *= localVelocityOldMag;
            localVelocityNew.m_quad.m128_f32[3] = 0.0f;

            apCharacterController->Direction = localVelocityNew;

        }
    }

    return ThisStdCall<CommonLib::bhkCharacterStateProjectile*>(GetCharacterStateDetour.GetOverwrittenAddr(), apCharacterController);
}


void installGuidedProjectilesHook() {
	GetCharacterStateDetour.WriteRelCall(0x00C737DD, reinterpret_cast<std::uint32_t>(&Hook_bhkCharacterController_GetCharacterState));
}
