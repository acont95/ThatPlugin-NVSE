#include "nvse/PluginAPI.h"
#include "nvse/SafeWrite.h"
#include "nvse/GameObjects.h"
#include "Bethesda/ItemChange.hpp"

constexpr std::uint32_t ItemChange_ConstructorNoArgs_Addr = 0x0076B630;
constexpr std::uint32_t ItemChange_Destructor_Addr = 0x004BC5F0;

namespace CommonLib {
	ItemChange::ItemChange() : pExtraObjectList(nullptr), iNumber(0), pContainerObj(nullptr) {
		ThisStdCall<void>(ItemChange_ConstructorNoArgs_Addr, this);
	}

	ItemChange::~ItemChange() {
		ThisStdCall<void>(ItemChange_Destructor_Addr, this);
	}

	bool ItemChange::operator==(const ItemChange& apOther) const = default;
	auto ItemChange::operator<=>(const ItemChange& apOther) const = default;
}
