#pragma once
#include <string>
#include "SimpleIni.h"

class ConfigManager
{
private:
	CSimpleIniA ini{};
public:
	ConfigManager();

	template<typename T>
	T getKey(const char* section, const char* key);

	template<typename T>
	T getKey(const char* section, const char* key, T _default);

    static ConfigManager& getInstance()
    {
        static ConfigManager instance{};
        return instance;
    }
};
