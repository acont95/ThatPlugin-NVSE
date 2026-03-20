#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "Bethesda/Actor.hpp"
#include "Bethesda/HitData.hpp"
#include "Bethesda/TESAmmo.hpp"
#include "Bethesda/TESObjectWEAP.hpp"
#include "Bethesda/Projectile.hpp"
#include "Bethesda/Tile.hpp"
#include "Bethesda/PlayerCharacter.hpp"
#include "Bethesda/BaseProcess.hpp"
#include "Bethesda/InventoryChanges.hpp"
#include "Gamebryo/NiPoint3.hpp"


#define EXTERN_DLL_EXPORT extern "C" __declspec(dllexport)

PluginHandle	g_pluginHandle = kPluginHandle_Invalid;

NVSEMessagingInterface* g_messagingInterface{};
NVSEInterface* g_nvseInterface{};

constexpr char g_PluginVersion[] = "0.1.1";

CallDetour ObjectHitDetour{};
CallDetour CombatHitDetour{};
CallDetour GetCurrentAmmoDetour{};
CallDetour HitMeDetour{};
CallDetour InitializeHitDataDetour{};
CallDetour ReduceDamageDetour{};

CallDetour GetFormClipRoundsDetour{};

constexpr uint32_t Actor_UseAmmo_Addr = 0x008A89A0;
constexpr uint32_t Actor_ShouldUseAmmo_Addr = 0x008A8DD0;
constexpr uint32_t Actor_GetCurrentWeapon_Addr = 0x008A1710;
constexpr uint32_t Projectile_Constructor_Addr = 0x009BBEF0;
constexpr uint32_t InventoryChanges_GetInventoryChanges_Addr = 0x004BF220;
constexpr uint32_t InventoryChanges_GetObjectCount_Addr = 0x004C8F30;
constexpr uint32_t TESObjectWEAP_GetCurrentAmmo_Addr = 0x00525980;

CommonLib::PlayerCharacter* PlayerCharacterGetSingleton()
{
	return *(CommonLib::PlayerCharacter**)0x011DEA3C;
}

static CommonLib::NiPoint3 ZERO = { 0.0f, 0.0f, 0.0f };

bool isBallisticMelee(CommonLib::TESObjectWEAP* weapon) {
	return weapon->data.eType <= 2 && weapon->pFormAmmo;
}

bool __fastcall Hook_IsMeleeWeapon(CommonLib::TESObjectWEAP* weapon, void* edx)
{
	if (isBallisticMelee(weapon))
		return false;

	return weapon->data.eType <= 2;
}

CommonLib::Tile* __fastcall Hook_ObjectHit(CommonLib::Actor* actor, void* edx, bool abPowerAttack)
{	
	CommonLib::Tile* result = ThisStdCall<CommonLib::Tile*>(ObjectHitDetour.GetOverwrittenAddr(), actor, abPowerAttack);
	CommonLib::TESObjectWEAP* weapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, actor);
	bool shouldUseAmmo = ThisStdCall<bool>(Actor_ShouldUseAmmo_Addr, actor, weapon);

	if (shouldUseAmmo && weapon && isBallisticMelee(weapon)) {
		bool isAutomatic = (weapon->data.cFlags >> 1) & 1;
		if (result || isAutomatic) {
			ThisStdCall<void>(Actor_UseAmmo_Addr, actor, 1);
		}
	}

	return result;
}

void __fastcall Hook_CombatHit(
	CommonLib::Actor* actor,
	void* edx,
	CommonLib::Actor* apTarget,
	bool abPowerAttack,
	CommonLib::Projectile* apProjectile,
	char cMeleeEffect)
{
	ThisStdCall<void>(
		CombatHitDetour.GetOverwrittenAddr(), 
		actor,
		apTarget, 
		abPowerAttack, 
		apProjectile,
		cMeleeEffect);

	CommonLib::TESObjectWEAP* weapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, actor);
	bool shouldUseAmmo = ThisStdCall<bool>(Actor_ShouldUseAmmo_Addr, actor, weapon);

	if (shouldUseAmmo && weapon && isBallisticMelee(weapon)) {
		ThisStdCall<void>(Actor_UseAmmo_Addr, actor, 1);
	}
}

CommonLib::TESAmmo* __fastcall Hook_GetCurrentAmmo(CommonLib::TESObjectWEAP* weapon, void* edx, CommonLib::Actor* apWeaponHolder)
{
	if (weapon && isBallisticMelee(weapon))
		return nullptr;

	return ThisStdCall<CommonLib::TESAmmo*>(GetCurrentAmmoDetour.GetOverwrittenAddr(), weapon, apWeaponHolder);
}

void __fastcall Hook_ReduceDamage(CommonLib::HitData* hitData, void* edx, bool abIgnoreBlocking)
{
	CommonLib::TESObjectWEAP* apFromWeapon = hitData->pWeapon;
	if (apFromWeapon && isBallisticMelee(apFromWeapon)) {
		CommonLib::BGSProjectile* apProjectileBase = apFromWeapon->data.pProjectile;
		CommonLib::TESObjectREFR* apShooter = hitData->pAggressor;

		if (apProjectileBase && apShooter) {
			CommonLib::Projectile* projectile = New<CommonLib::Projectile, Projectile_Constructor_Addr>(
				apProjectileBase,
				apShooter,
				apFromWeapon,
				&ZERO,
				0.0,
				0.0
			);
			hitData->pSourceRef = projectile;
		}
	}

	return ThisStdCall<void>(ReduceDamageDetour.GetOverwrittenAddr(), hitData, abIgnoreBlocking);
}

int Hook_UIWweaponPrint(char* Buffer, size_t BufferCount, char* Format, ...)
{
	CommonLib::PlayerCharacter* pPlayer = PlayerCharacterGetSingleton();
	CommonLib::TESObjectWEAP* weapon = ThisStdCall<CommonLib::TESObjectWEAP*>(Actor_GetCurrentWeapon_Addr, pPlayer);
	va_list args;
	va_start(args, Format);

	if (weapon && !(weapon->cClipRounds)) {
		//va_arg(args, int);
		return vsprintf_s(Buffer, BufferCount, "%i", args);
	}

	return vsprintf_s(Buffer, BufferCount, Format, args);
}

int __fastcall Hook_GetFormClipRounds(CommonLib::TESObjectWEAP* weapon, void* edx, bool bModValue)
{
	if (weapon && !(weapon->cClipRounds)) {
		CommonLib::PlayerCharacter* pPlayer = PlayerCharacterGetSingleton();
		CommonLib::TESAmmo* ammo = ThisStdCall<CommonLib::TESAmmo*>(TESObjectWEAP_GetCurrentAmmo_Addr, weapon, pPlayer);
		CommonLib::InventoryChanges* inventoryChanges = CdeclCall<CommonLib::InventoryChanges*>(InventoryChanges_GetInventoryChanges_Addr, pPlayer);
		int objectCount = ThisStdCall<int>(InventoryChanges_GetObjectCount_Addr, inventoryChanges, ammo);

		return objectCount;
	}  

	return ThisStdCall<int>(GetFormClipRoundsDetour.GetOverwrittenAddr(), weapon, bModValue);
}

// This is a message handler for nvse events
// With this, plugins can listen to messages such as whenever the game loads
void MessageHandler(NVSEMessagingInterface::Message* msg)
{
	switch (msg->type)
	{
	case NVSEMessagingInterface::kMessage_PostLoad: break;
	case NVSEMessagingInterface::kMessage_ExitGame: break;
	case NVSEMessagingInterface::kMessage_ExitToMainMenu: break;
	case NVSEMessagingInterface::kMessage_LoadGame: break;
	case NVSEMessagingInterface::kMessage_SaveGame: break;
	case NVSEMessagingInterface::kMessage_PreLoadGame: break;
	case NVSEMessagingInterface::kMessage_ExitGame_Console: break;
	case NVSEMessagingInterface::kMessage_PostLoadGame: break;
	case NVSEMessagingInterface::kMessage_PostPostLoad: break;
	case NVSEMessagingInterface::kMessage_RuntimeScriptError: break;
	case NVSEMessagingInterface::kMessage_DeleteGame: break;
	case NVSEMessagingInterface::kMessage_RenameGame: break;
	case NVSEMessagingInterface::kMessage_RenameNewGame: break;
	case NVSEMessagingInterface::kMessage_NewGame: break;
	case NVSEMessagingInterface::kMessage_DeleteGameName: break;
	case NVSEMessagingInterface::kMessage_RenameGameName: break;
	case NVSEMessagingInterface::kMessage_RenameNewGameName: break;
	case NVSEMessagingInterface::kMessage_DeferredInit: 
		Console_Print("That Plugin NVSE version: %s", g_PluginVersion);
		break;
	case NVSEMessagingInterface::kMessage_ClearScriptDataCache: break;
	case NVSEMessagingInterface::kMessage_MainGameLoop: break;
	case NVSEMessagingInterface::kMessage_ScriptCompile: break;
	case NVSEMessagingInterface::kMessage_EventListDestroyed: break;
	case NVSEMessagingInterface::kMessage_PostQueryPlugins: break;
	default: break;
	}
}

EXTERN_DLL_EXPORT bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {

	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "ThatPluginNVSE";
	info->version = 210;

	// version checks
	if (nvse->nvseVersion < PACKED_NVSE_VERSION)
	{
		_ERROR("NVSE version too old (got %08X expected at least %08X)", nvse->nvseVersion, PACKED_NVSE_VERSION);
		return false;
	}

	return true;
}

EXTERN_DLL_EXPORT bool NVSEPlugin_Load(NVSEInterface* nvse) {

	g_pluginHandle = nvse->GetPluginHandle();

	// save the NVSE interface in case we need it later
	g_nvseInterface = nvse;

	// register to receive messages from NVSE
	g_messagingInterface = static_cast<NVSEMessagingInterface*>(nvse->QueryInterface(kInterface_Messaging));
	g_messagingInterface->RegisterListener(g_pluginHandle, "NVSE", MessageHandler);

	if (!nvse->isEditor) {
		// Hook TESObjectWEAP::IsMeleeWeapon call in HUDMainMenu::UpdateWeaponStatus
		WriteRelCall(0x007724CB, reinterpret_cast<std::uint32_t>(&Hook_IsMeleeWeapon));

		// Hook Actor::ObjectHit and Actor::CombatHit calls in Actor::MeleeAttack
		ObjectHitDetour.WriteRelCall(0x008997FE, reinterpret_cast<std::uint32_t>(&Hook_ObjectHit));
		CombatHitDetour.WriteRelCall(0x0089996D, reinterpret_cast<std::uint32_t>(&Hook_CombatHit));

		// Hook TESObjectWEAP::GetCurrentAmmo inside PlayerCharacter::CheckUserInputAttacks
		GetCurrentAmmoDetour.WriteRelCall(0x0094926A, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));
		GetCurrentAmmoDetour.WriteRelCall(0x009492B5, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));
		GetCurrentAmmoDetour.WriteRelCall(0x009492D7, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));
		GetCurrentAmmoDetour.WriteRelCall(0x00948E0E, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));
		GetCurrentAmmoDetour.WriteRelCall(0x00949E39, reinterpret_cast<std::uint32_t>(&Hook_GetCurrentAmmo));

		// Hook HitData::ReduceDamage in HitData::InitializeHitData
		ReduceDamageDetour.WriteRelCall(0x009B5623, reinterpret_cast<std::uint32_t>(&Hook_ReduceDamage));

		// Hook BSsprintf call in HUDMainMenu::UpdateWeaponStatus
		WriteRelCall(0x0077253D, reinterpret_cast<std::uint32_t>(&Hook_UIWweaponPrint));
		// Hook BSsprintf call in VATSMenu::UpdateAmmo
		WriteRelCall(0x007F3019, reinterpret_cast<std::uint32_t>(&Hook_UIWweaponPrint));

		GetFormClipRoundsDetour.WriteRelCall(0x004FE113, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x00645679, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x00645FC7, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x0070869B, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x00772421, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x007EDB41, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x007EE591, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x007F2A65, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x007F2A9B, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x007F2AB6, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x007F2B8A, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x007F2BDE, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x007F2CAD, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x007F2D10, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x0088CE83, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x00892CE9, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x008A85D5, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x008A86D3, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x008A86E5, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x008A8955, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x008BAAD4, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x008F7F3E, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x00943D65, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x00948C20, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x0095D656, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x0095D690, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x0095DA70, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x0095DABE, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
		GetFormClipRoundsDetour.WriteRelCall(0x0095DBDA, reinterpret_cast<std::uint32_t>(&Hook_GetFormClipRounds));
	
	}

	return true;
}
