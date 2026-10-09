#pragma once
// Engine-free decision of the VIP_NoFallDamage port (the original OnTakeDamage body):
//
//   if (g_bNoFallDamage[slot] && (pDamageInfo->m_bitsDamageType & DMG_FALL)
//       && pDamageInfo->m_flDamage < 4999.0f) return false; // block
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
