#include <cmath>
#include "GuidedProjectiles.hpp"
#include "Globals.hpp"
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "Bethesda/bhkCharacterController.hpp"
#include "Bethesda/bhkCharacterListener.hpp"
#include "Bethesda/CFilter.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/bhkWorld.hpp"
#include "Bethesda/bhkShapePhantom.hpp"
#include "Bethesda/Projectile.hpp"
#include "Bethesda/BSReference.hpp"
#include "Bethesda/ProjectileListener.hpp"
#include "Bethesda/MoveData.hpp"
#include "Bethesda/bhkCharacterStateProjectile.hpp"
#include "Bethesda/VATS.hpp"
#include "Bethesda/BGSProjectile.hpp"
#include "Bethesda/ItemChange.hpp"
#include "Bethesda/TESObjectWEAP.hpp"
#include "Bethesda/TESObjectIMOD.hpp"
#include "Bethesda/TESAmmo.hpp"
#include "Gamebryo/NiCamera.hpp"
#include "Gamebryo/NiPoint3.hpp"
#include "Gamebryo/NiColorA.hpp"
#include "Gamebryo/NiMatrix3.hpp"
#include "Havok/hkVector4.hpp"
#include "Havok/hkpWorldRayCastInput.hpp"
#include "Havok/hkpWorldRayCastOutput.hpp"
#include "Havok/hkpWorld.hpp"
#include "Havok/hkRotation.hpp"
#include "Havok/hkTransform.hpp"
#include "Havok/constants.hpp"

CallDetour GetCharacterStateDetour{};
CallDetour SetLinearVelocityDetour{};
CallDetour SyncWith3dRefDetour{};
CallDetour ClearPostAnimationActionsDetour{};

constexpr std::uint32_t Main_spWorldRoot_GetCamera_Address = 0x00524C90;
constexpr std::uint32_t hkpWorld_castRay_Address = 0x00C92040;
constexpr std::uint32_t bhkCharacterController_GetWorld_Address = 0x00621AD0;
constexpr std::uint32_t bhkCharacterController_GetCollisionFilter_Address = 0x0070C440;
constexpr std::uint32_t bhkCharacterController_FindBSReference_Address = 0x00C6DEC0;
constexpr std::uint32_t TESObjectREFR_IsProjectile_Address = 0x005725B0;
constexpr std::uint32_t bhkCharacterController_GetPosition_Address = 0x00C6E300;
constexpr std::uint32_t TESObjectREFR_SetAngleOnReference_Address = 0x00575700;
constexpr std::uint32_t TESObjectCELL_GetbhkWorld_Address = 0x004543C0;
constexpr std::uint32_t hkpWorldRayCastInput_Constructor_Address = 0x004A3CC0;
constexpr std::uint32_t hkpWorldRayCastOutput_Constructor_Address = 0x004A3D00;
constexpr std::uint32_t TESForm_GetFormByEditorID_Address = 0x00483A00;
constexpr std::uint32_t Process_GetCurrentWeapon_Addr = 0x008D81E0;
constexpr std::uint32_t ItemChange_GetModSlots_Addr = 0x004BD820;
constexpr std::uint32_t TESObjectWEAP_GetCurrentAmmo_Addr = 0x00525980;

constexpr std::uint32_t MakeLine_Address = 0x004B3890;
constexpr std::uint32_t TES_AddTempDebugObject_Address = 0x00458E20;

constexpr bool bDebugRayCast = false;
constexpr float fScale = 50000.0f;
constexpr std::uint32_t iLayerLineOfSight = 37;

constexpr const char* CONFIG_SECTION = "GuidedProjectiles";

struct ConfigEntry {
    std::uint32_t weaponId;
    std::uint32_t weaponModId;
    std::uint32_t ammoId;
    std::uint32_t projectileId;
};

struct WeaponModIds {
    std::uint32_t modSlot0;
    std::uint32_t modSlot1;
    std::uint32_t modSlot2;
};

static bool weaponModIdsMatch(WeaponModIds modIds, std::uint32_t modId) {
    if (modIds.modSlot0 == modId || modIds.modSlot1 == modId || modIds.modSlot2 == modId) {
        return true;
    }

    return false;
}

std::vector<ConfigEntry> entries;

static bool operator==(const ConfigEntry& lhs, const ConfigEntry& rhs)
{
    return lhs.ammoId == rhs.ammoId && lhs.weaponModId == rhs.weaponModId && lhs.ammoId == rhs.ammoId && lhs.projectileId == rhs.projectileId;
}

CommonLib::TESForm* getFormFromConfigEntry(CSimpleIni::Entry& configEntry, const char* configKey) {
    const char* configValue = Globals::g_Ini.GetValue(configEntry.pItem, configKey);
    if (configValue) {
        CommonLib::TESForm* form = CdeclCall<CommonLib::TESForm*>(TESForm_GetFormByEditorID_Address, configValue);
        if (form) {
            return form;
        }
        else {
            Console_Print("ThatPlugin NVSE: Failed to resolve form for editor ID %s", configValue);
        }
    }

    return nullptr;
}

/// <summary>
/// Build configuration entries from editor IDs defined in INI file.
/// </summary>
void loadGuidedProjectilesConfig() {
    CSimpleIni::TNamesDepend sections;
    Globals::g_Ini.GetAllSections(sections);
    for (CSimpleIni::Entry& element : sections) {
        Console_Print(element.pItem);
        if (!strncmp(element.pItem, CONFIG_SECTION, strlen(CONFIG_SECTION)) && strcmp(element.pItem, CONFIG_SECTION)) {
            Console_Print("MATCH");
            ConfigEntry newEntry{0,0,0,0};
            CommonLib::TESForm* form;

            form = getFormFromConfigEntry(element, "Weapon");
            if (form) newEntry.weaponId = form->iFormID;

            form = getFormFromConfigEntry(element, "Mod");
            if (form) newEntry.weaponModId = form->iFormID;

            form = getFormFromConfigEntry(element, "Ammo");
            if (form) newEntry.ammoId = form->iFormID;

            form = getFormFromConfigEntry(element, "Projectile");
            if (form) newEntry.projectileId = form->iFormID;

            if (newEntry.ammoId || newEntry.projectileId || newEntry.weaponId || newEntry.weaponModId) {
                entries.push_back(newEntry);
            }
        }
    }
}


static WeaponModIds getWeaponModIds(
    CommonLib::TESObjectWEAP* apWeapon,
    CommonLib::ItemChange* apItemChange)
{
    std::uint8_t modSlots =
        ThisStdCall<std::uint8_t>(ItemChange_GetModSlots_Addr, apItemChange);

    WeaponModIds modIds{ 0,0,0 };

    CommonLib::TESObjectIMOD* mod = nullptr;

    if (modSlots & 0x01)
    {
        mod = apWeapon->pModObjectOne;
        if (mod) modIds.modSlot0 = mod->iFormID;
    }

    if (modSlots & 0x02)
    {
        mod = apWeapon->pModObjectTwo;
        if (mod) modIds.modSlot1 = mod->iFormID;
    }

    if (modSlots & 0x04)
    {
        mod = apWeapon->pModObjectThree;
        if (mod) modIds.modSlot2 = mod->iFormID;
    }

    return modIds;
}

/// <summary>
/// Determines if the projectile should be guided.
/// </summary>
/// <param name="apProjectile"></param>
/// <returns></returns>
static bool isProjectileGuided(CommonLib::Projectile* apProjectile) {
    CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacter::GetPlayerSingleton();
    if (apProjectile && apProjectile->pShooter == pPlayer) {
        std::uint32_t projectileId = 0;
        std::uint32_t weaponId = 0;
        std::uint32_t ammoId = 0;
        WeaponModIds modIds{ 0,0,0 };
        CommonLib::BGSProjectile* projectileBase = static_cast<CommonLib::BGSProjectile*>(apProjectile->data.pObjectReference);
        if (projectileBase) {
            projectileId = projectileBase->iFormID;
        }
        CommonLib::ItemChange* weaponItemChange = ThisStdCall<CommonLib::ItemChange*>(Process_GetCurrentWeapon_Addr, pPlayer->pCurrentProcess);
        if (weaponItemChange) {
            CommonLib::TESObjectWEAP* weaponBase = static_cast<CommonLib::TESObjectWEAP*>(weaponItemChange->pContainerObj);
            if (weaponBase) {
                weaponId = weaponBase->iFormID;
                modIds = getWeaponModIds(weaponBase, weaponItemChange);
                CommonLib::TESAmmo* currentAmmo = ThisStdCall<CommonLib::TESAmmo*>(TESObjectWEAP_GetCurrentAmmo_Addr, weaponBase, pPlayer);
                if (currentAmmo) {
                    ammoId = currentAmmo->iFormID;
                }
            }
        }

        for (ConfigEntry& entry : entries) {
            if ((!entry.projectileId || entry.projectileId == projectileId) &&
                (!entry.weaponId || entry.weaponId == weaponId) &&
                (!entry.ammoId || entry.ammoId == ammoId) &&
                weaponModIdsMatch(modIds, entry.weaponModId)) {
                
                return true;
            }
        }
    }

    return false;
}

/// <summary>
/// Perform raycast from camera center to forward direction * fScale.
/// Modifies collision filter to change from projectile layer to line of sight.
/// This avoid ray colliding with exploding projectile model.
/// </summary>
/// <param name="apCharacterController"></param>
/// <param name="apInput"></param>
/// <param name="apResult"></param>
void getCameraRayCastOutput(
    CommonLib::bhkCharacterController* apCharacterController, 
    CommonLib::hkpWorldRayCastInput* apInput, 
    CommonLib::hkpWorldRayCastOutput* apResult
) {
    CommonLib::NiCamera* camera = CdeclCall<CommonLib::NiCamera*>(Main_spWorldRoot_GetCamera_Address);
    const CommonLib::NiPoint3 niStart = camera->m_kWorld.m_Translate;
    CommonLib::NiPoint3 niEnd{
        niStart.x + camera->m_kWorld.m_Rotate.m_pEntry[0].x * fScale,
        niStart.y + camera->m_kWorld.m_Rotate.m_pEntry[1].x * fScale,
        niStart.z + camera->m_kWorld.m_Rotate.m_pEntry[2].x * fScale
    };

    CommonLib::CFilter filter = CommonLib::CFilter{ 0 };
    ThisStdCall<CommonLib::CFilter*>(bhkCharacterController_GetCollisionFilter_Address, apCharacterController, &filter);

    apInput->m_from = CommonLib::hkVector4::fromPoint(niStart);
    apInput->m_to = CommonLib::hkVector4::fromPoint(niEnd);
    apInput->m_filterInfo = (filter.iFilter & 0xFFFFFF80) | iLayerLineOfSight;

    CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacter::GetPlayerSingleton();
    CommonLib::VATS* pVats = CommonLib::VATS::GetVATSSingleton();

    CommonLib::TESObjectCELL* parentCell = pPlayer->pParentCell;

    CommonLib::bhkWorld* pBhkWorld = ThisStdCall<CommonLib::bhkWorld*>(TESObjectCELL_GetbhkWorld_Address, parentCell);
    CommonLib::hkpWorld* pHkpWorld = reinterpret_cast<CommonLib::hkpWorld*>(pBhkWorld->phkObject);

    ThisStdCall<void>(hkpWorld_castRay_Address, pHkpWorld, apInput, apResult);
}

/// <summary>
/// Check if the player camera is in reasonable state to perform ray cast.
/// </summary>
/// <returns></returns>
bool isCameraReady() {
    CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacter::GetPlayerSingleton();
    CommonLib::VATS* pVats = CommonLib::VATS::GetVATSSingleton();

    return pVats->eMode == CommonLib::VATS::VATS_MODE_NONE && pPlayer->fTimeInSlowMoCam <= 0.0f && !CommonLib::IsVanityMode();
}


static void debugRayCast(CommonLib::hkVector4 projectileLocation, CommonLib::hkVector4 hitPoint) {
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
    ThisStdCall<void>(TES_AddTempDebugObject_Address, tes, line, 10.0f);
}


static void printVector(const CommonLib::hkVector4 vec, const char* name) {
    Console_Print(
        "%s %.6f %.6f %.6f %.6f",
        name,
        vec.m_quad.m128_f32[0],
        vec.m_quad.m128_f32[1],
        vec.m_quad.m128_f32[2],
        vec.m_quad.m128_f32[3]
    );
}

static void printPoint(const CommonLib::NiPoint3 vec, const char* name) {
    Console_Print(
        "%s %.6f %.6f %.6f",
        name,
        vec.x,
        vec.y,
        vec.z
    );
}

/// <summary>
/// Sets input velocity vector to point to ray cast hit point location. Essentiallly pursuit guidance law.
/// Note the Direction vector is local coordinate velocity vector when hooked at this location.
/// </summary>
/// <param name="apCharacterController"></param>
/// <returns></returns>
CommonLib::bhkCharacterStateProjectile* __fastcall Hook_bhkCharacterController_GetCharacterState(CommonLib::bhkCharacterController* apCharacterController, void* edx)
{
    CommonLib::TESObjectREFR* bsRef = nullptr;
    CommonLib::bhkShapePhantom* shapePhantom = apCharacterController->spShapePhantom.m_pObject;
    if (shapePhantom) {
        CommonLib::hkReferencedObject* referencedObject = shapePhantom->phkObject;
        if (referencedObject) {
            bsRef = CdeclCall<CommonLib::TESObjectREFR*>(
                bhkCharacterController_FindBSReference_Address,
                CommonLib::bhkCharacterController::REFERENCE_SLOTS::MOBOBJECT,
                referencedObject
            );
        }
    }

    if (bsRef && ThisStdCall<bool>(TESObjectREFR_IsProjectile_Address, bsRef) && isCameraReady()) {
        CommonLib::Projectile* projectile = static_cast<CommonLib::Projectile*>(bsRef);

        if (isProjectileGuided(projectile)) {
            CommonLib::hkVector4 projectileLocation{};
            ThisStdCall<void>(bhkCharacterController_GetPosition_Address, apCharacterController, &projectileLocation);

            CommonLib::hkpWorldRayCastInput* rayCastInput = New<CommonLib::hkpWorldRayCastInput, hkpWorldRayCastInput_Constructor_Address>();
            CommonLib::hkpWorldRayCastOutput* rayCastOutput = New<CommonLib::hkpWorldRayCastOutput, hkpWorldRayCastOutput_Constructor_Address>();
            getCameraRayCastOutput(apCharacterController, rayCastInput, rayCastOutput);


            CommonLib::hkVector4 hitPoint = rayCastInput->m_from + (rayCastInput->m_to - rayCastInput->m_from) * rayCastOutput->m_hitFraction;

            if (bDebugRayCast) debugRayCast(projectileLocation, hitPoint);

            // Translation
            CommonLib::hkVector4 forward = apCharacterController->ForwardVec;
            forward.setNeg3(forward); // Forward points backwards, thanks Bethesda
            CommonLib::hkVector4 up = apCharacterController->UpVec;
            CommonLib::hkVector4 right{};
            right.setCross3(forward, up);
            up.setCross3(right, forward);
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

            if (localVelocityNew.isOk3().m_bool) {
                apCharacterController->Direction = localVelocityNew;
            }
        }
    }

    return ThisStdCall<CommonLib::bhkCharacterStateProjectile*>(GetCharacterStateDetour.GetOverwrittenAddr(), apCharacterController);
}

/// <summary>
/// Sets the orientation of the Projectile reference to out velocity direction calculated by Havok. 
/// This velocity may be different from input velocity, as out velocity is constrained by physics engine.
/// </summary>
/// <param name="apCharacterController"></param>
/// <param name="edx"></param>
/// <param name="vel"></param>
/// <returns></returns>
void __fastcall Hook_bhkCharacterController_SetLinearVelocity(CommonLib::bhkCharacterController* apCharacterController, void* edx, CommonLib::hkVector4* vel)
{
    CommonLib::bhkShapePhantom* shapePhantom = apCharacterController->spShapePhantom.m_pObject;
    CommonLib::hkReferencedObject* referencedObject = shapePhantom->phkObject;
    CommonLib::TESObjectREFR* bsRef = CdeclCall<CommonLib::TESObjectREFR*>(
        bhkCharacterController_FindBSReference_Address,
        CommonLib::bhkCharacterController::REFERENCE_SLOTS::MOBOBJECT,
        referencedObject
    );

    if (bsRef && ThisStdCall<bool>(TESObjectREFR_IsProjectile_Address, bsRef) && isCameraReady()) {
        CommonLib::Projectile* projectile = static_cast<CommonLib::Projectile*>(bsRef);
        if (isProjectileGuided(projectile)) {
            CommonLib::NiPoint3 up3d = CommonLib::NiPoint3::UNIT_Z;
            CommonLib::NiPoint3 directionForward{ vel->m_quad.m128_f32[0], vel->m_quad.m128_f32[1] , vel->m_quad.m128_f32[2] };
            CommonLib::NiPoint3 directionRight = directionForward.UnitCross(up3d);
            CommonLib::NiPoint3 directionUp = directionForward.UnitCross(directionRight);
            directionForward.Unitize();

            CommonLib::NiMatrix3 rotationNew3d{directionRight, directionForward, directionUp};
            rotationNew3d = rotationNew3d.Transpose();

            CommonLib::NiPoint3 newReferenceAngle{};
            rotationNew3d.ToEulerAnglesZXY(newReferenceAngle.z, newReferenceAngle.x, newReferenceAngle.y);
            ThisStdCall<void>(TESObjectREFR_SetAngleOnReference_Address, projectile, newReferenceAngle);
        }
    }

    ThisStdCall<void>(SetLinearVelocityDetour.GetOverwrittenAddr(), apCharacterController, vel);
}

/// <summary>
/// Ensure when projectile 3d is synced, sync rotation is set to true if projectile is guided.
/// </summary>
/// <param name="apProjectile"></param>
/// <param name="abSyncRotation"></param>
/// <param name="abDoArcReorientation"></param>
/// <returns></returns>
bool __fastcall Hook_Projectile_Sync3DWithReference(CommonLib::Projectile* apProjectile, void* edx, bool abSyncRotation, bool abDoArcReorientation)
{
    abSyncRotation = isProjectileGuided(apProjectile) && isCameraReady() ? true : abSyncRotation;
    return ThisStdCall<bool>(SyncWith3dRefDetour.GetOverwrittenAddr(), apProjectile, abSyncRotation, abDoArcReorientation);
}


void __fastcall Hook_Actor_ClearPostAnimationActions(CommonLib::Actor* apActor, void* edx)
{
    ThisStdCall<void>(ClearPostAnimationActionsDetour.GetOverwrittenAddr(), apActor);
}


void installGuidedProjectilesHook() {
    if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {
        GetCharacterStateDetour.WriteRelCall(0x00C737DD, reinterpret_cast<std::uint32_t>(&Hook_bhkCharacterController_GetCharacterState));
        SetLinearVelocityDetour.WriteRelCall(0x00C73821, reinterpret_cast<std::uint32_t>(&Hook_bhkCharacterController_SetLinearVelocity));
        SyncWith3dRefDetour.WriteRelCall(0x009B8552, reinterpret_cast<std::uint32_t>(&Hook_Projectile_Sync3DWithReference));
        ClearPostAnimationActionsDetour.WriteRelCall(0x0088C872, reinterpret_cast<std::uint32_t>(&Hook_Actor_ClearPostAnimationActions));
    }
}
