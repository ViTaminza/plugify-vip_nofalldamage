#pragma once
// Engine-free decision of the VIP_NoFallDamage port, the original OnTakeDamage rule: a hit is
// blocked when the victim's slot is flagged, the damage type carries DMG_FALL and the damage is
// below 4999.0.
//
// DMG_FALL is bit 5 of the damage-type bitfield (hl2sdk: 1 << 5). The 4999 limit keeps the
// engine's out-of-world kill working.

#include <cstdint>

namespace vip_nofalldamage {

inline constexpr int32_t kDmgFall = 1 << 5;
inline constexpr float kMaxBlockedDamage = 4999.0f;

inline bool ShouldBlockFallDamage(bool featureEnabled, int32_t bitsDamageType, float damage) {
	return featureEnabled && (bitsDamageType & kDmgFall) != 0 && damage < kMaxBlockedDamage;
}

} // namespace vip_nofalldamage
