#include "Bethesda/WeaponData.hpp"

namespace CommonLib {
	bool WeaponData::operator==(const WeaponData& apOther) const = default;
	auto WeaponData::operator<=>(const WeaponData& apOther) const = default;
}
