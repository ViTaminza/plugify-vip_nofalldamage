// vip_nofalldamage unit tests — the engine-independent part: the block decision of the original
// OnTakeDamage (feature flag + DMG_FALL + the 4999 damage limit that keeps the engine's
// out-of-world kill working).
//
// The hook itself (polyhook detour, signature resolution, slot lookup) needs a live server.
//
// Run by ../../test.sh (inside the Steam Runtime image, cwd = build/tests).

#include "damage_logic.hpp"

#include <cstdio>
#include <string>

namespace {

using vip_nofalldamage::kDmgFall;
using vip_nofalldamage::ShouldBlockFallDamage;

int g_checks = 0;
int g_failures = 0;

void CheckEq(bool actual, bool expected, const std::string& what) {
	++g_checks;
	if (actual != expected) {
		++g_failures;
		std::printf("FAIL: %s (expected %d, got %d)\n", what.c_str(), expected ? 1 : 0, actual ? 1 : 0);
	}
}

void TestDamageTypes() {
	// DMG_FALL alone (0x20 in the hl2sdk bitfield).
	CheckEq(kDmgFall, 32, "DMG_FALL is bit 5");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 20.0f), true, "fall damage is blocked");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall | 2 /*DMG_BULLET*/, 20.0f), true,
			"a combined damage type containing DMG_FALL is blocked");
	CheckEq(ShouldBlockFallDamage(true, 2 /*DMG_BULLET*/, 20.0f), false, "bullets are not blocked");
	CheckEq(ShouldBlockFallDamage(true, 0, 20.0f), false, "no damage type is not blocked");
	CheckEq(ShouldBlockFallDamage(false, kDmgFall, 20.0f), false, "the feature off blocks nothing");
	CheckEq(ShouldBlockFallDamage(false, kDmgFall | 0x40000000, 5.0f), false,
			"the feature off blocks nothing (even with other flag bits)");
}

void TestDamageLimit() {
	// The original's `m_flDamage < 4999.0f`: the engine's out-of-world kill (a far larger value)
	// still applies.
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 0.0f), true, "zero fall damage is blocked");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 4998.9f), true, "just below the limit is blocked");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 4999.0f), false, "the limit itself is not blocked");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 10000.0f), false, "the out-of-world kill applies");
}

} // namespace

int main() {
	std::printf("vip_nofalldamage tests\n");
	TestDamageTypes();
	TestDamageLimit();
	std::printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
