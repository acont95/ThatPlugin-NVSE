#include <cstdint>
#include <cstdarg>
#include <cstdio>
#include <filesystem>
#include <iostream>
#include <algorithm>
#include <string>
#include <cctype>

#include "nvse/PluginAPI.h"
#include "BallisticMelee.hpp"
#include "BetterCounter.hpp"
#include "BottomlessClip.hpp"
#include "GuidedProjectiles.hpp"
#include "PersistReferenceScale.hpp"
#include "SimpleIni.h"
#include "Globals.hpp"


#define EXTERN_DLL_EXPORT extern "C" __declspec(dllexport)

IDebugLog		gLog("logs\\ThatPlugin-NVSE.log");
PluginHandle	g_pluginHandle = kPluginHandle_Invalid;

NVSEMessagingInterface* g_messagingInterface{};
NVSEInterface* g_nvseInterface{};

constexpr std::uint32_t PLUGIN_VERSION = 45;
constexpr char g_PluginVersion[] = "0.4.5";

const std::filesystem::path g_dataPath = "Data";
const std::filesystem::path g_pluginsPath = g_dataPath / "NVSE/Plugins";
const std::filesystem::path g_IniFile = "ThatPlugin.ini";

CSimpleIniA g_Ini;
bool configMissing;

static std::wstring normalizePath(const std::filesystem::path& path) {
	std::wstring pathStr = path.lexically_normal().wstring();
	std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(),
		[](wchar_t c) { return std::tolower(c); });
	return pathStr;
}

static void loadAdditionalIniFiles() {
	try {
		std::wstring pluginsPathStr = normalizePath(g_pluginsPath);
		std::wstring iniFileStr = normalizePath(g_IniFile);

		for (std::filesystem::recursive_directory_iterator it(g_dataPath), end; it != end; ++it) {
			std::wstring currPathStr = normalizePath(it->path());
			std::wstring currFilenameStr = normalizePath(it->path().filename());

			if (it->is_directory() && currPathStr == pluginsPathStr) {
				it.disable_recursion_pending();
			}
			else if (it->is_regular_file() && currFilenameStr == iniFileStr) {
				try {
					Globals::g_Ini.LoadFile(it->path().c_str());

					std::u8string path = it->path().u8string();
					std::string pathStr(path.begin(), path.end());
					_MESSAGE("Loaded additional INI file: %s", pathStr.c_str());
				}
				catch (const std::exception& e) {
					_ERROR("Failed to load INI: %s", e.what());
				}
			}
		}
	}
	catch (const std::exception& e) {
		_ERROR("Failed to search for additional INI files: %s", e.what());
	}
}

// This is a message handler for nvse events
// With this, plugins can listen to messages such as whenever the game loads
void MessageHandler(NVSEMessagingInterface::Message* msg)
{
	switch (msg->type)
	{
	case NVSEMessagingInterface::kMessage_PostLoad: break; // Not working
	case NVSEMessagingInterface::kMessage_ExitGame: break;
	case NVSEMessagingInterface::kMessage_ExitToMainMenu: break;
	case NVSEMessagingInterface::kMessage_LoadGame: break; 
	case NVSEMessagingInterface::kMessage_SaveGame: break;
	case NVSEMessagingInterface::kMessage_PreLoadGame: break;
	case NVSEMessagingInterface::kMessage_ExitGame_Console: break;
	case NVSEMessagingInterface::kMessage_PostLoadGame: break; 
	case NVSEMessagingInterface::kMessage_PostPostLoad: break; // Not working
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
		if (configMissing) {
			Console_Print("That Plugin NVSE config file not found!");
		}
		loadGuidedProjectilesConfig();
		break;
	case NVSEMessagingInterface::kMessage_ClearScriptDataCache: break;
	case NVSEMessagingInterface::kMessage_MainGameLoop: break;
	case NVSEMessagingInterface::kMessage_ScriptCompile: break;
	case NVSEMessagingInterface::kMessage_EventListDestroyed: break;
	case NVSEMessagingInterface::kMessage_PostQueryPlugins: break; // Not working
	default: break;
	}
}

EXTERN_DLL_EXPORT bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {

	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "ThatPluginNVSE";
	info->version = PLUGIN_VERSION;

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

	Globals::g_Ini.SetQuotes();
	Globals::g_Ini.SetUnicode();
	SI_Error rc = Globals::g_Ini.LoadFile((g_pluginsPath / g_IniFile).c_str());
	if (rc < 0) {
		configMissing = true;
		return true;
	};

	loadAdditionalIniFiles();

	if (!nvse->isEditor) {
		installBallisticMeleeHooks();
		installBetterCounterHooks();
		installBottomlessClipHooks();
		installGuidedProjectilesHook();
		installPersistReferenceScaleHooks();
	}

	return true;
}
