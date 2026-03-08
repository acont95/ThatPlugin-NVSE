#include "PluginAPI.h"
#include "SafeWrite.h"
#include "GameForms.h"
#include "GameProcess.h"
#include <cstdint>

#define EXTERN_DLL_EXPORT extern "C" __declspec(dllexport)

PluginHandle	g_pluginHandle = kPluginHandle_Invalid;

NVSEMessagingInterface* g_messagingInterface{};
NVSEInterface* g_nvseInterface{};

constexpr uint32_t g_PluginVersion = 100;

CallDetour ObjectHitDetour{};
CallDetour CombatHitDetour{};

auto Actor_UseAmmo = (void(__thiscall*)(Actor*, int))0x008A89A0;

bool __fastcall Hook_IsMeleeWeapon(TESObjectWEAP* weapon, void* edx)
{
	if (weapon->eWeaponType <= 2 && weapon->ammo.ammo)
		return false;

	return weapon->eWeaponType <= 2;
}

int __fastcall Hook_ObjectHit(Actor* actor, void* edx, bool abPowerAttack)
{	
	Actor* actorVar = actor;
	auto Original_ObjectHit =
		(int(__thiscall*)(Actor*, bool))ObjectHitDetour.GetOverwrittenAddr();

	int result = Original_ObjectHit(actorVar, abPowerAttack);

	ThisStdCall(0x008A89A0, actorVar, 1);
	// Actor_UseAmmo(actorVar, 1);

	return result;
}

void __fastcall Hook_CombatHit(
	Actor* actor,
	void* edx,
	Actor* apTarget,
	bool abPowerAttack,
	Projectile* apProjectile,
	char cMeleeEffect,
	int a6,
	int a7,
	int a8)
{
	Actor* actorVar = actor;
	auto Original_CombatHit =
		(void(__thiscall*)(Actor*, Actor*, bool, Projectile*, char, int, int, int))
		CombatHitDetour.GetOverwrittenAddr();

	Actor_UseAmmo(actorVar, 1);
	Original_CombatHit(actorVar, apTarget, abPowerAttack, apProjectile, cMeleeEffect, a6, a7, a8);
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
		Console_Print("That Plugin NVSE version: %.2f", (g_PluginVersion / 100.0F));
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
		// FNV
		WriteRelCall(0x007724CB, UInt32(&Hook_IsMeleeWeapon));
		ObjectHitDetour.WriteRelCall(0x008997FE, UInt32(&Hook_ObjectHit));
		CombatHitDetour.WriteRelCall(0x0089996D, UInt32(&Hook_CombatHit));
	}

	return true;
}
