#include "RegenAmmo.hpp"
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "nvse/GameObjects.h"
#include "Bethesda/Actor.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/BaseProcess.hpp"
#include "Bethesda/TESObjectWEAP.hpp"
#include "Bethesda/ItemChange.hpp"
#include "Bethesda/InventoryChanges.hpp"
#include "Bethesda/BSTimer.hpp"
#include "Globals.hpp"

constexpr char CONFIG_SECTION[] = "BetterRegenAmmo";

CallDetour GetBestAmmoForWeaponDetour{};
CallDetour AddWornDetour{};

constexpr std::uint32_t ItemChange_HasModEffectActive_Addr = 0x004BDA70;
constexpr std::uint32_t ItemChange_HasModEffectActiveF_Addr = 0x004BD8D0;
constexpr std::uint32_t TESObjectWEAP_GetFormClipRounds_Addr = 0x004FE160;
constexpr std::uint32_t TESObjectWEAP_GetAmmoRegenRate_Addr = 0x00709430;
constexpr std::uint32_t ItemChange_Constructor_Addr = 0x004BC550;
constexpr std::uint32_t BGSAmmoForm_GetAmmoHelper_Addr = 0x00474920;
constexpr std::uint32_t TESObjectWEAP_GetCurrentAmmo_Addr = 0x00525980;
constexpr std::uint32_t ExtraDataList_GetWeaponModSlotActive_Addr = 0x00418C00;
constexpr std::uint32_t TESObjectWEAP_GetModEffect_Addr = 0x004BD880;

static bool g_bRegenFullChargeOnEquip = false;

static int getRegenValue(CommonLib::Actor* apActor, int aiShotCount)
{
	CommonLib::PlayerCharacter* pPlayer = CommonLib::PlayerCharacter::GetPlayerSingleton();

	if (apActor == nullptr) {
		return 0;
	}

	int iRegenSubtract = 0;
	if (apActor == pPlayer) {
		CommonLib::BaseProcess* pCurrentProcess = pPlayer->pCurrentProcess;
		if (pCurrentProcess && pCurrentProcess->GetCurrentWeapon()) {
			float fRegen = 0.0f;
			CommonLib::ItemChange* pCurrentWeapon = pCurrentProcess->GetCurrentWeapon();

			std::uint8_t cModEffect = CommonLib::WEAPON_MOD_EFFECT::WEAPON_MOD_AMMO_REGEN_SHOT;
			if (ThisStdCall<bool>(ItemChange_HasModEffectActiveF_Addr, pCurrentWeapon, cModEffect, &fRegen)) {
				pPlayer->cShotsSinceLastAmmoRegen += aiShotCount;

				const int iRegen = static_cast<int>(fRegen);
				if (pPlayer->cShotsSinceLastAmmoRegen >= static_cast<int>(fRegen)) {
					pPlayer->cShotsSinceLastAmmoRegen -= iRegen;
					iRegenSubtract = 1;
				}
			}
		}
	}

	return iRegenSubtract;
}

static __declspec(naked) void Hook_FixWeaponModAmmoRegen()
{
	__asm
	{
		mov     eax, [ebp - 0x58]
		push    eax
		mov     eax, [ebp - 0x2C]
		push    eax
		call    getRegenValue
		add     esp, 8

		mov[ebp - 0x1D4], eax

		mov edx, 0x005248D0
		jmp edx
	}
}


bool isWeaponItemChangeRegen(CommonLib::ItemChange* apWeapon) {
	if (apWeapon) {
		CommonLib::TESObjectWEAP* pWeapon = static_cast<CommonLib::TESObjectWEAP*>(apWeapon->pContainerObj);
		if (pWeapon) {
			bool bIsRegenMod = ThisStdCall<bool>(ItemChange_HasModEffectActive_Addr, apWeapon, CommonLib::WEAPON_MOD_AMMO_REGEN_SECONDS);

			float fRegenRate = ThisStdCall<float>(TESObjectWEAP_GetAmmoRegenRate_Addr, pWeapon, bIsRegenMod);
			if (fRegenRate > 0.0f) {
				return true;
			}
		}
	}

	return false;
}


static bool isWeaponModActive(CommonLib::TESObjectWEAP* apWeapon, CommonLib::ExtraDataList* apExtraData, CommonLib::WEAPON_MOD_EFFECT eModEffect) {
	if (ThisStdCall<CommonLib::WEAPON_MOD_EFFECT>(TESObjectWEAP_GetModEffect_Addr, apWeapon, 1u) == eModEffect
		&& ThisStdCall<bool>(ExtraDataList_GetWeaponModSlotActive_Addr, apExtraData, 1u))
	{
		return true;
	}
	else if (ThisStdCall<CommonLib::WEAPON_MOD_EFFECT>(TESObjectWEAP_GetModEffect_Addr, apWeapon, 2u) == eModEffect
		&& ThisStdCall<bool>(ExtraDataList_GetWeaponModSlotActive_Addr, apExtraData, 2u))
	{
		return true;
	}
	else if (ThisStdCall<CommonLib::WEAPON_MOD_EFFECT>(TESObjectWEAP_GetModEffect_Addr, apWeapon, 4u) == eModEffect
		&& ThisStdCall<bool>(ExtraDataList_GetWeaponModSlotActive_Addr, apExtraData, 4u))
	{
		return true;
	}

	return false;
}


static int Hook_RegenAmmoCount(CommonLib::InventoryChanges* apInventoryChanges, CommonLib::TESBoundObject* apObject, int aiOriginalCount)
{
	CommonLib::TESObjectREFR* pRef = apInventoryChanges->pRef;
	if (apObject && pRef && pRef->IsActor()) {
		CommonLib::Actor* pActor = static_cast<CommonLib::Actor*>(pRef);
		CommonLib::ENUM_FORM_ID eFormType =
			static_cast<CommonLib::ENUM_FORM_ID>(apObject->cFormType);

		if (eFormType == CommonLib::AMMO_ID) {
			CommonLib::BaseProcess* pCurrentProcess = pActor->pCurrentProcess;
			if (pCurrentProcess) {
				CommonLib::ItemChange* pCurrentWeapon = pCurrentProcess->GetCurrentWeapon();
				if (pCurrentWeapon) {
					if (isWeaponItemChangeRegen(pCurrentWeapon)) {
						CommonLib::TESObjectWEAP* pWeapon = static_cast<CommonLib::TESObjectWEAP*>(pCurrentWeapon->pContainerObj);
						bool bIsClipMod = ThisStdCall<bool>(ItemChange_HasModEffectActive_Addr, pCurrentWeapon, CommonLib::WEAPON_MOD_INCREASE_CLIP_SIZE);
						int iClipSize = ThisStdCall<int>(TESObjectWEAP_GetFormClipRounds_Addr, pWeapon, bIsClipMod);
						return iClipSize;
					}
				}
			}
		}
	}

	return aiOriginalCount;
}

static __declspec(naked) void Hook_GetObjectCount()
{
	__asm
	{
		mov     edx, [ebp - 0xC]
		push    edx
		mov     edx, [ebp + 0x8]
		push    edx
		mov     edx, [ebp - 0x10]
		push    edx
		call    Hook_RegenAmmoCount
		add     esp, 0xC

		mov     esp, ebp

		mov     edx, 0x004C8FC5
		jmp     edx
	}
}

static int isActorUsingRegenWeapon(CommonLib::Actor* apActor)
{
	CommonLib::BaseProcess* pCurrentProcess = apActor->pCurrentProcess;
	if (pCurrentProcess) {
		CommonLib::ItemChange* pCurrentWeapon = pCurrentProcess->GetCurrentWeapon();
		if (pCurrentWeapon) {
			return isWeaponItemChangeRegen(pCurrentWeapon);
		}
	}

	return false;
}

static __declspec(naked) void Hook_NPCRegen()
{
	__asm
	{
		mov     edx, [ebp - 0x1C]
		push    edx
		call    isActorUsingRegenWeapon
		add     esp, 0x4

		test    al, al
		jnz     skip_branch

		cmp[ebp - 0x10], 0
		jnz     skip_branch

		mov     edx, 0x008A8D69
		jmp     edx

	skip_branch:
		mov     edx, 0x008A8DBF
		jmp     edx
	}
}


static CommonLib::TESAmmo* __fastcall Hook_GetBestAmmoForWeapon_InventoryChanges(
	CommonLib::InventoryChanges* apInventoryChanges, void* edx, CommonLib::TESObjectWEAP* apWeapon, bool* bNeedsAmmo)
{
	std::uint8_t* ebp = GetParentBasePtr(_AddressOfReturnAddress(), false);
	CommonLib::ItemChange* pCurrentWeapon = *reinterpret_cast<CommonLib::ItemChange**>(ebp - 0x84);
	if (pCurrentWeapon) {
		if (isWeaponItemChangeRegen(pCurrentWeapon)) {
			CommonLib::TESObjectWEAP* pWeapon = static_cast<CommonLib::TESObjectWEAP*>(pCurrentWeapon->pContainerObj);
			return ThisStdCall<CommonLib::TESAmmo*>(BGSAmmoForm_GetAmmoHelper_Addr, static_cast<CommonLib::BGSAmmoForm*>(pWeapon));
		}
	}

	return ThisStdCall<CommonLib::TESAmmo*>(GetBestAmmoForWeaponDetour.GetOverwrittenAddr(), apInventoryChanges, apWeapon, bNeedsAmmo);
}

static CommonLib::TESAmmo* __fastcall Hook_GetBestAmmoForWeapon_OriginalContainer(
	CommonLib::InventoryChanges* apInventoryChanges, void* edx, CommonLib::TESObjectWEAP* apWeapon, bool* bNeedsAmmo)
{
	std::uint8_t* ebp = GetParentBasePtr(_AddressOfReturnAddress(), false);
	CommonLib::ItemChange* pCurrentWeapon = *reinterpret_cast<CommonLib::ItemChange**>(ebp - 0x54);
	if (pCurrentWeapon) {
		if (isWeaponItemChangeRegen(pCurrentWeapon)) {
			CommonLib::TESObjectWEAP* pWeapon = static_cast<CommonLib::TESObjectWEAP*>(pCurrentWeapon->pContainerObj);
			return ThisStdCall<CommonLib::TESAmmo*>(BGSAmmoForm_GetAmmoHelper_Addr, static_cast<CommonLib::BGSAmmoForm*>(pWeapon));
		}
	}

	return ThisStdCall<CommonLib::TESAmmo*>(GetBestAmmoForWeaponDetour.GetOverwrittenAddr(), apInventoryChanges, apWeapon, bNeedsAmmo);
}


static void actorRegenAmmo(CommonLib::Actor* apActor) {
	CommonLib::BaseProcess* pCurrentProcess = apActor->pCurrentProcess;
	if (pCurrentProcess && !apActor->IsDead(0)) {
		CommonLib::ItemChange* pCurrentWeapon = pCurrentProcess->GetCurrentWeapon();
		if (pCurrentWeapon) {
			CommonLib::TESObjectWEAP* pWeapon = static_cast<CommonLib::TESObjectWEAP*>(pCurrentWeapon->pContainerObj);
			bool bIsRegenMod = ThisStdCall<bool>(ItemChange_HasModEffectActive_Addr, pCurrentWeapon, CommonLib::WEAPON_MOD_AMMO_REGEN_SECONDS);
			float fRegenRate = ThisStdCall<float>(TESObjectWEAP_GetAmmoRegenRate_Addr, pWeapon, bIsRegenMod);
			if (fRegenRate > 0.0f) {
				CommonLib::BSTimer* pAppTimer = CommonLib::BSTimer::GetAppTimerSingleton();
				Actor* pActorForm = reinterpret_cast<Actor*>(apActor);
				NiPointer<ActorRegenTickExtraData> pExtraRegenData =
					(ActorRegenTickExtraData*) PluginFormExtraData::Get(
						Globals::g_dataInterface,
						pActorForm,
						ActorRegenTickExtraData::GetName()
					);

				if (pExtraRegenData == nullptr) {
					ActorRegenTickExtraData* pNew = New<ActorRegenTickExtraData>();
					new (pNew) ActorRegenTickExtraData();

					pExtraRegenData = pNew;
					PluginFormExtraData::Add(Globals::g_dataInterface, pActorForm, pExtraRegenData);
				}

				float fTimeSinceLastAmmoRegenTick = pExtraRegenData->fTimeSinceLastAmmoRegenTick;
				fTimeSinceLastAmmoRegenTick -= pAppTimer->fDelta;
				pExtraRegenData->fTimeSinceLastAmmoRegenTick = fTimeSinceLastAmmoRegenTick;

				CommonLib::ItemChange* pCurrentAmmo = pCurrentProcess->GetCurrentAmmo();
				if (pCurrentAmmo && fTimeSinceLastAmmoRegenTick <= 0.0f) {
					pExtraRegenData->fTimeSinceLastAmmoRegenTick = fRegenRate;
					int iAmmoCount = pCurrentAmmo->iNumber;
					bool bIsClipMod = ThisStdCall<bool>(ItemChange_HasModEffectActive_Addr, pCurrentWeapon, CommonLib::WEAPON_MOD_INCREASE_CLIP_SIZE);
					int iClipSize = ThisStdCall<int>(TESObjectWEAP_GetFormClipRounds_Addr, pWeapon, bIsClipMod);

					if (iAmmoCount < iClipSize) {
						pCurrentAmmo->iNumber += 1;
					}
				}
			}
		}
	}
}


static CommonLib::BaseProcess* __fastcall Hook_GetCurrentProcess(CommonLib::Actor* apActor, void* edx)
{
	actorRegenAmmo(apActor);
	return apActor->pCurrentProcess;
}

static __declspec(naked) void Hook_SkipUpdateInventoryDataRegenReload()
{
	__asm
	{
		mov     edx, [ebp - 0x2C]
		push    edx
		call    isWeaponItemChangeRegen
		add     esp, 0x4

		test    al, al
		jnz     skip_branch

		mov     ecx, [ebp - 0xCC]
		mov     edx, 0x00525980
		call    edx
		mov     edx, 0x009998FA
		jmp     edx

	skip_branch :
		mov     edx, 0x00999C49
		jmp     edx
	}
}


static void setAmmoItemChange(CommonLib::Actor* apActor)
{
	if (apActor) {
		CommonLib::BaseProcess* pCurrentProcess = apActor->pCurrentProcess;
		if (pCurrentProcess) {
			CommonLib::ItemChange* pCurrentWeapon = pCurrentProcess->GetCurrentWeapon();
			CommonLib::ItemChange* pCurrentAmmo = pCurrentProcess->GetCurrentAmmo();
			if (pCurrentWeapon && !pCurrentAmmo) {
				CommonLib::TESObjectWEAP* pWeapon = static_cast<CommonLib::TESObjectWEAP*>(pCurrentWeapon->pContainerObj);
				CommonLib::TESAmmo* pAmmo = ThisStdCall<CommonLib::TESAmmo*>(TESObjectWEAP_GetCurrentAmmo_Addr, pWeapon, apActor);
				if (pAmmo) {
					int iClipSize = 0;
					if (g_bRegenFullChargeOnEquip) {
						bool bIsClipMod = ThisStdCall<bool>(ItemChange_HasModEffectActive_Addr, pCurrentWeapon, CommonLib::WEAPON_MOD_INCREASE_CLIP_SIZE);
						iClipSize = ThisStdCall<int>(TESObjectWEAP_GetFormClipRounds_Addr, pWeapon, bIsClipMod);
					}

					CommonLib::ItemChange* pNewAmmoItemChange = New<CommonLib::ItemChange, ItemChange_Constructor_Addr>(pAmmo, iClipSize);
					pCurrentProcess->SetCurrentAmmo(pNewAmmoItemChange);
				}
			}
		}
	}
}

static __declspec(naked) void Hook_ActorReloadWeapon()
{
	__asm
	{
		mov     edx, [ebp - 0x58]
		push    edx
		call    isActorUsingRegenWeapon
		add     esp, 0x4

		test    al, al
		jnz     regen

		cmp     [ebp + 0x8], 0
		jnz     vanilla_continue

		mov     edx, 0x008A844F
		jmp     edx

	regen :
		mov     edx, [ebp - 0x58]
		push    edx
		call    setAmmoItemChange
		add     esp, 0x4
		mov     al, 1
		mov     edx, 0x008A8807
		jmp     edx

	vanilla_continue:
		mov     edx, 0x008A8456
		jmp     edx
	}
}


static __declspec(naked) void Hook_PlayerReloadWeapon()
{
	__asm
	{
		mov     edx, [ebp - 0x5C]
		push    edx
		call    isActorUsingRegenWeapon
		add     esp, 0x4

		test    al, al
		jnz     regen

		mov     eax, dword ptr ds : [0x011DEA3C]

		mov     edx, 0x0095D87E
		jmp     edx

	regen :
		mov     edx, [ebp - 0x5C]
		push    edx
		call    setAmmoItemChange
		add     esp, 0x4
		mov     al, 1
		mov     edx, 0x0095DDA0
		jmp     edx		
	}
}


static void __fastcall Hook_AddWorn(
	CommonLib::InventoryChanges* apInventoryChanges, 
	void* edx, 
	CommonLib::TESBoundObject* apObject, 
	int aiNumber, 
	CommonLib::Actor* apActor, 
	CommonLib::ExtraDataList* apExtra, 
	bool abLeft, 
	bool abForceEquip
)
{
	ThisStdCall(AddWornDetour.GetOverwrittenAddr(), apInventoryChanges, apObject, aiNumber, apActor, apExtra, abLeft, abForceEquip);
	if (apActor && apObject && apObject->cFormType == CommonLib::ENUM_FORM_ID::WEAP_ID) {
		CommonLib::TESObjectWEAP* pWeapon = static_cast<CommonLib::TESObjectWEAP*>(apObject);
		bool bIsRegenMod = isWeaponModActive(pWeapon, apExtra, CommonLib::WEAPON_MOD_AMMO_REGEN_SECONDS);
		float fRegenRate = ThisStdCall<float>(TESObjectWEAP_GetAmmoRegenRate_Addr, pWeapon, bIsRegenMod);

		if (fRegenRate > 0.0f) {
			std::uint8_t* ebp = GetParentBasePtr(_AddressOfReturnAddress(), false);
			bool bMod = *reinterpret_cast<bool*>(ebp - 0x39);

			CommonLib::BaseProcess* pCurrentProcess = apActor->pCurrentProcess;
			if (pCurrentProcess) {
				pCurrentProcess->SetCurrentAmmo(nullptr);
			}

			apActor->ReloadWeaponNV(static_cast<CommonLib::TESObjectWEAP*>(apObject), 0, bMod, 1);
		}
	}
}


static __declspec(naked) void Hook_SkipLoadGameAmmoCheck()
{
	__asm
	{
		mov     edx, [ebp - 0x14]
		push    edx
		call    isActorUsingRegenWeapon
		add     esp, 0x4

		test    al, al
		jnz     regen_skip

		cmp     dword ptr[ecx + 0x118], 0

		mov     edx, 0x0092763A
		jmp     edx

	regen_skip :
		mov     edx, 0x0092777F
		jmp     edx
	}
}


void installRegenAmmoHooks() {
	if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bEnabled")) {

		// Call ReloadWeaponNV again after InventoryChanges::AddWorn to allow checking for weapon mods
		AddWornDetour.WriteRelCall(0x0088DFFF, reinterpret_cast<std::uint32_t>(&Hook_AddWorn));

		// Hook PlayerCharacter::ReloadWeapon to set current ammo for regen weapons
		WriteRelJump(0x0095D879, reinterpret_cast<std::uint32_t>(&Hook_PlayerReloadWeapon));

		// Hook InventoryChanges::GetBestAmmoForWeapon calls, in InventoryChanges::GetBestWeapon
		// Checks weapon ItemChange from loop in calling function stack for regen rate and calls BGSAmmoForm::GetAmmoHelper if regen
		// Allows Actors to choose regen weapons without ammo in inventory
		GetBestAmmoForWeaponDetour.WriteRelCall(0x004C773B, reinterpret_cast<std::uint32_t>(&Hook_GetBestAmmoForWeapon_OriginalContainer));
		GetBestAmmoForWeaponDetour.WriteRelCall(0x004C7C48, reinterpret_cast<std::uint32_t>(&Hook_GetBestAmmoForWeapon_InventoryChanges));

		// Hook MiddleHighProcess::LoadGame to skip ammo inventory check after reload
		// Not needed with GetObjectCount hook for PC, but needed with NPC due to vanilla bug
		WriteRelJump(0x00927633, reinterpret_cast<std::uint32_t>(&Hook_SkipLoadGameAmmoCheck));
		PatchMemoryNop(0x00927638, 2);

		// Hook Actor::UseAmmo to skip unequip when out of ammo for regen weapons
		// TODO - factor waiting for regen into DPS calculations for weapon switch
		WriteRelJump(0x008A8D63, reinterpret_cast<std::uint32_t>(&Hook_NPCRegen));
		PatchMemoryNop(0x008A8D68, 1);

		// Hook GetCurrentProcess call in Actor::Update to regenerate ammo 
		// Called when actor is not player and current process is not null
		// Use NVSE Extra Data for storing float timer
		WriteRelCall(0x008895EE, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentProcess));

		// Hook CombatState::UpdateInventoryData to skip NPC periodic and out of ammo reload for regen weapons
		// Also skips removing weapon from consideration if no ammo in inventory
		WriteRelJump(0x009998F5, reinterpret_cast<std::uint32_t>(&Hook_SkipUpdateInventoryDataRegenReload));

		// Hook Actor::ReloadWeapon to set current ammo for regen weapons
		WriteRelJump(0x008A8449, reinterpret_cast<std::uint32_t>(&Hook_ActorReloadWeapon));
		PatchMemoryNop(0x008A844E, 1);


		if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bRegenFullChargeOnEquip"))
		{
			g_bRegenFullChargeOnEquip = true;

			// Hook InventoryChanges::GetObjectCount to return the clip size if weapon is regen
			// Needed for Stewies Tweaks compatibility with remember ammo count
			WriteRelJump(0x004C8FC0, reinterpret_cast<std::uint32_t>(&Hook_GetObjectCount));
		}

		if (Globals::g_Ini.GetBoolValue(CONFIG_SECTION, "bFixRegenAmmoCount")) 
		{
			// Fix hardcoded ammo regen (shots) value in TESObjectWEAP::Fire
			WriteRelJump(0x005247E5, reinterpret_cast<std::uint32_t>(&Hook_FixWeaponModAmmoRegen));
			PatchMemoryNop(0x005247EA, 5);
		}
	}
}
