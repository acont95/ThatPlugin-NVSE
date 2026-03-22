#include "ConfigManager.hpp"
#include "SimpleIni.h"

ConfigManager::ConfigManager()
{
	SI_Error rc = ini.LoadFile("Data/NVSE/Plugins/ThatPlugin.ini");
	if (rc < 0) { /* handle error */ };
	ini.SetUnicode();;
}

template<typename T>
T ConfigManager::getKey(const char* section, const char* key)
{
	static_assert(sizeof(T) == 0, "Unsupported type for getKey");
}

template<typename T>
T ConfigManager::getKey(const char* section, const char* key, T _default)
{
	static_assert(sizeof(T) == 0, "Unsupported type for getKey with default");
}

template<>
int ConfigManager::getKey<int>(const char* section, const char* key)
{
	const char* pv;
	pv = ini.GetValue(section, key, "");

	return std::stoi(pv);
}

template<>
int ConfigManager::getKey<int>(const char* section, const char* key, const int _default)
{
	const char* pv;
	pv = ini.GetValue(section, key, "");

	if (std::string(pv).empty()) {
		return _default;
	}

	return std::stoi(pv);
}

template<>
bool ConfigManager::getKey<bool>(const char* section, const char* key)
{
	const char* pv;
	pv = ini.GetValue(section, key, "");

	return !strcmp(pv, "1") || !strcmp(pv, "true");
}

template<>
bool ConfigManager::getKey<bool>(const char* section, const char* key, const bool _default)
{
	const char* pv;
	pv = ini.GetValue(section, key, "");

	if (!pv || pv[0] == '\0') {
		return _default;
	}

	return !strcmp(pv, "1") || !strcmp(pv, "true");
}

template<>
const char* ConfigManager::getKey<const char*>(const char* section, const char* key)
{
	const char* pv;
	pv = ini.GetValue(section, key, "");

	return pv;
}

template<>
const char* ConfigManager::getKey<const char*>(const char* section, const char* key, const char* _default)
{
	const char* pv;
	pv = ini.GetValue(section, key, "");

	if (!pv || pv[0] == '\0') {
		return _default;
	}

	return pv;
}

template int ConfigManager::getKey<int>(const char*, const char*);
template int ConfigManager::getKey<int>(const char*, const char*, int);
template bool ConfigManager::getKey<bool>(const char*, const char*);
template bool ConfigManager::getKey<bool>(const char*, const char*, bool);
template const char* ConfigManager::getKey<const char*>(const char*, const char*);
template const char* ConfigManager::getKey<const char*>(const char*, const char*, const char*);
