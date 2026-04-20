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
#include "Havok/hkTransform.hpp"
#include "Havok/constants.hpp"

CallDetour GetCharacterStateDetour{};
CallDetour HasFlagDetour{};
CallDetour SyncWith3dRefDetour{};

CommonLib::hkpWorldRayCastInput::hkpWorldRayCastInput() : 
    m_enableShapeCollectionFilter(false),
    m_filterInfo(0)
{
    m_from = CommonLib::hkVector4{};
    m_to = CommonLib::hkVector4{};
}
CommonLib::hkpWorldRayCastInput::~hkpWorldRayCastInput() = default;

CommonLib::hkpShapeRayCastCollectorOutput::hkpShapeRayCastCollectorOutput()
    : m_hitFraction(1.0),
    m_extraInfo(-1),
    m_pad{ 0, 0 }
{
    m_normal = CommonLib::hkVector4{};
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
constexpr uint32_t bhkCharacterController_GetCollisionFilter_Address = 0x0070C440;
constexpr uint32_t bhkCharacterController_FindBSReference_Address = 0x00C6DEC0;
constexpr uint32_t TESObjectREFR_IsProjectile_Address = 0x005725B0;
constexpr uint32_t bhkCharacterController_GetPosition_Address = 0x00C6E300;
constexpr uint32_t NiMatrix3_FromEulerAnglesZXY_Address = 0x00A59660;
constexpr uint32_t NiMatrix3_ToEulerAnglesZXY_Address = 0x00A59400;
constexpr uint32_t TESObjectREFR_SetAngleOnReference_Address = 0x00575700;

constexpr uint32_t MakeLine_Address = 0x004B3890;
constexpr uint32_t TES_AddTempDebugObject_Address = 0x00458E20;

constexpr bool bDebugRayCast = true;
constexpr float fScale = 50000.0f;

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
    CommonLib::hkVector4 startVec = CommonLib::hkVector4{};
    CommonLib::hkVector4 endVec = CommonLib::hkVector4{};

    rayCastInput.m_from = CommonLib::hkVector4::fromPoint(startVec, niStart);
    rayCastInput.m_to = CommonLib::hkVector4::fromPoint(endVec, niEnd);
    rayCastInput.m_filterInfo = filter.iFilter;

    CommonLib::hkpWorldRayCastOutput rayCastOutput = CommonLib::hkpWorldRayCastOutput{};

    CommonLib::bhkWorld* pBhkWorld = ThisStdCall<CommonLib::bhkWorld*>(bhkCharacterController_GetWorld_Address, apCharacterController);

    ThisStdCall<void>(hkpWorld_castRay_Address, reinterpret_cast<CommonLib::hkpWorld*>(pBhkWorld->phkObject), &rayCastInput, &rayCastOutput);

    CommonLib::hkVector4 hitPoint = startVec + (endVec - startVec) * rayCastOutput.m_hitFraction;
    return hitPoint;
}


void debugRayCast(CommonLib::hkVector4 projectileLocation, CommonLib::hkVector4 hitPoint) {
    const CommonLib::NiColorA color{ 0.0f, 1.0f, 0.0f, 1.0f };
    CommonLib::NiPoint3 niLoc{
        projectileLocation.m_quad.m128_f32[0] * CommonLib::fHk2BSScaleSC_639,
        projectileLocation.m_quad.m128_f32[1] * CommonLib::fHk2BSScaleSC_639,
        projectileLocation.m_quad.m128_f32[2] * CommonLib::fHk2BSScaleSC_639
    };
    CommonLib::NiPoint3 niHit{
        hitPoint.m_quad.m128_f32[0] * CommonLib::fHk2BSScaleSC_639,
        hitPoint.m_quad.m128_f32[1] * CommonLib::fHk2BSScaleSC_639,
        hitPoint.m_quad.m128_f32[2] * CommonLib::fHk2BSScaleSC_639
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

void printPoint(CommonLib::NiPoint3 vec, const char* name) {
    Console_Print(
        "%s %.6f %.6f %.6f",
        name,
        vec.x,
        vec.y,
        vec.z
    );
}


CommonLib::bhkCharacterStateProjectile* __fastcall Hook_bhkCharacterController_GetCharacterState(CommonLib::bhkCharacterController* apCharacterController)
{
    CommonLib::bhkShapePhantom* shapePhantom = apCharacterController->spShapePhantom.m_pObject;
    CommonLib::hkReferencedObject* referencedObject = shapePhantom->phkObject;
    CommonLib::TESObjectREFR* bsRef = CdeclCall<CommonLib::TESObjectREFR*>(
        bhkCharacterController_FindBSReference_Address,
        1000u,
        referencedObject
    );

    if (bsRef && ThisStdCall<bool>(TESObjectREFR_IsProjectile_Address, bsRef)) {
        CommonLib::Projectile* projectile = static_cast<CommonLib::Projectile*>(bsRef);

        if (isProjectileGuided(projectile)) {
            CommonLib::hkVector4 projectileLocation{};
            ThisStdCall<void>(bhkCharacterController_GetPosition_Address, apCharacterController, &projectileLocation);

            CommonLib::hkVector4 hitPoint = getCameraRayCastHitPoint(apCharacterController);
            if (bDebugRayCast) debugRayCast(projectileLocation, hitPoint);

            // Translation
            // Forward points backwards... must flip
            CommonLib::hkVector4 forward = apCharacterController->ForwardVec;
            forward.setNeg3(forward);
            CommonLib::hkVector4 up = apCharacterController->UpVec;
            CommonLib::hkVector4 right = forward.cross3(up);
            forward.normalize3();
            up.normalize3();
            right.normalize3();

            CommonLib::hkRotation rotationMatrix = { right, forward, up };
            CommonLib::hkTransform transform{
                rotationMatrix,
                projectileLocation
            };

            CommonLib::hkVector4 localVelocityNew{};
            localVelocityNew.setTransformedInversePos(transform, hitPoint);
            localVelocityNew.normalize3();
            localVelocityNew *= apCharacterController->Direction.length3();
            if (localVelocityNew.isNormalized3(0.000001f).m_bool) {
                apCharacterController->Direction = localVelocityNew;
            }

            // Rotation
            CommonLib::NiMatrix3 rotation3d {};
            ThisStdCall<void>(NiMatrix3_FromEulerAnglesZXY_Address, &rotation3d, projectile->data.Angle.z, projectile->data.Angle.x, projectile->data.Angle.y);

            CommonLib::hkVector4 up3d = CommonLib::hkVector4{ 0.0f, 0.0f, 1.0f, 0.0f };
            CommonLib::hkVector4 directionForward = hitPoint - projectileLocation;
            CommonLib::hkVector4 directionRight = directionForward.cross3(up3d);
            CommonLib::hkVector4 directionUp = directionForward.cross3(directionRight);
            directionForward.normalize3();
            directionRight.normalize3();
            directionUp.normalize3();

            CommonLib::NiMatrix3 rotationNew3d{};

            rotationNew3d.m_pEntry[0] = CommonLib::NiPoint3{ 
                directionRight.m_quad.m128_f32[0], 
                directionForward.m_quad.m128_f32[0],
                directionUp.m_quad.m128_f32[0]
            };
            rotationNew3d.m_pEntry[1] = CommonLib::NiPoint3{
                directionRight.m_quad.m128_f32[1],
                directionForward.m_quad.m128_f32[1],
                directionUp.m_quad.m128_f32[1]
            };
            rotationNew3d.m_pEntry[2] = CommonLib::NiPoint3{
                directionRight.m_quad.m128_f32[2],
                directionForward.m_quad.m128_f32[2],
                directionUp.m_quad.m128_f32[2]
            };

            CommonLib::NiPoint3 newReferenceAngle{};
            ThisStdCall<void>(NiMatrix3_ToEulerAnglesZXY_Address, &rotationNew3d, &newReferenceAngle.z, &newReferenceAngle.x, &newReferenceAngle.y);
            ThisStdCall<void>(TESObjectREFR_SetAngleOnReference_Address, projectile, newReferenceAngle);
        }
    }

    return ThisStdCall<CommonLib::bhkCharacterStateProjectile*>(GetCharacterStateDetour.GetOverwrittenAddr(), apCharacterController);
}

bool __fastcall Hook_Projectile_Sync3DWithReference(CommonLib::Projectile* apProjectile, bool abSyncRotation, bool abDoArcReorientation)
{
    abSyncRotation = isProjectileGuided(apProjectile) ? true : abSyncRotation;
    return ThisStdCall<bool>(SyncWith3dRefDetour.GetOverwrittenAddr(), apProjectile, abSyncRotation, abDoArcReorientation);
}


void installGuidedProjectilesHook() {
	GetCharacterStateDetour.WriteRelCall(0x00C737DD, reinterpret_cast<std::uint32_t>(&Hook_bhkCharacterController_GetCharacterState));
    SyncWith3dRefDetour.WriteRelCall(0x009B8552, reinterpret_cast<std::uint32_t>(&Hook_Projectile_Sync3DWithReference));
}
