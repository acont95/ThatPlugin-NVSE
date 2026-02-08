#include "../nvse/PluginAPI.h"

bool NVSEPlugin_Query(const NVSEInterface* nvse, PluginInfo* info) {
	info->infoVersion = PluginInfo::kInfoVersion;
	info->name = "MoonlightNVSE";
	info->version = 210;
	return true;
}
