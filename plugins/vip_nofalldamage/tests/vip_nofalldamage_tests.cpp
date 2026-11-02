// vip_nofalldamage unit tests — the engine-independent part of the plugin:
//
//   * the block decision of the original OnTakeDamage (feature flag + DMG_FALL + the 4999 damage
//     limit that keeps the engine's out-of-world kill working), damage_logic.hpp;
//   * the map-gated schema-offset cache (PITFALLS C8): -1 while the map gate is closed and no
//     lookup, only offsets >= 0 memoised, a failed lookup retried, Clear() (OnPluginEnd) forgetting
//     everything, schema_cache.hpp.
//
// The cache is driven through an injected fake lookup, so the whole policy is exercised without
// the engine, without the s2sdk bindings and without pisexcompat (the plugin no longer links it;
// the tests never did).
//
// The hook itself (polyhook detour, signature resolution, slot lookup) needs a running game server.
//
// Run by ../../test.sh (inside the Steam Runtime image, cwd = build/tests).

#include "damage_logic.hpp"
#include "schema_cache.hpp"

#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <string>

namespace {

using vip_nofalldamage::kDmgFall;
using vip_nofalldamage::SchemaOffsetCache;
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

void CheckInt(long long actual, long long expected, const std::string& what) {
	++g_checks;
	if (actual != expected) {
		++g_failures;
		std::printf("FAIL: %s (expected %lld, got %lld)\n", what.c_str(), expected, actual);
	}
}

void CheckStr(const std::string& actual, const std::string& expected, const std::string& what) {
	++g_checks;
	if (actual != expected) {
		++g_failures;
		std::printf("FAIL: %s (expected \"%s\", got \"%s\")\n", what.c_str(), expected.c_str(),
					actual.c_str());
	}
}

// --- the fake s2sdk::GetSchemaOffset the cache is driven with -------------------

int32_t g_lookupAnswer = -1;
int g_lookupCalls = 0;
std::string g_lastClass;
std::string g_lastField;

int32_t FakeLookup(const char* className, const char* fieldName) {
	++g_lookupCalls;
	g_lastClass = className == nullptr ? "<null>" : className;
	g_lastField = fieldName == nullptr ? "<null>" : fieldName;
	return g_lookupAnswer;
}

void ResetFake() {
	g_lookupAnswer = -1;
	g_lookupCalls = 0;
	g_lastClass.clear();
	g_lastField.clear();
}

constexpr const char* kInfoClass = "CTakeDamageInfo";

// --- the block decision ---------------------------------------------------------

void TestDamageTypes() {
	// DMG_FALL alone (0x20 in the hl2sdk bitfield).
	CheckEq(kDmgFall, 32, "DMG_FALL is bit 5");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 20.0f), true, "fall damage is blocked");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall | 2 /*DMG_BULLET*/, 20.0f), true,
			"a combined damage type containing DMG_FALL is blocked");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall | 0x40000000 /*a high bit*/, 5.0f), true,
			"the high bits of the bitfield do not mask DMG_FALL");
	CheckEq(ShouldBlockFallDamage(true, 0x7fffffff /*every bit*/, 5.0f), true,
			"all bits set contains DMG_FALL");
	CheckEq(ShouldBlockFallDamage(true, 2 /*DMG_BULLET*/, 20.0f), false, "bullets are not blocked");
	CheckEq(ShouldBlockFallDamage(true, 0, 20.0f), false, "no damage type is not blocked");
	// DMG_FALL is bit 5: bit 4 (DMG_BLAST) and bit 6 (DMG_BURN) must not be mistaken for it.
	CheckEq(ShouldBlockFallDamage(true, 1 << 4, 20.0f), false, "DMG_BLAST is not fall damage");
	CheckEq(ShouldBlockFallDamage(true, 1 << 6, 20.0f), false, "DMG_BURN is not fall damage");
	CheckEq(ShouldBlockFallDamage(false, kDmgFall, 20.0f), false, "the feature off blocks nothing");
	CheckEq(ShouldBlockFallDamage(false, kDmgFall | 0x40000000, 5.0f), false,
			"the feature off blocks nothing (even with other flag bits)");
}

void TestDamageLimit() {
	// The original's 4999.0 damage limit: the engine's out-of-world kill (a far larger value)
	// still applies.
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 0.0f), true, "zero fall damage is blocked");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 4998.9f), true, "just below the limit is blocked");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 4999.0f), false, "the limit itself is not blocked");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 4999.1f), false, "just above the limit is not blocked");
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, 10000.0f), false, "the out-of-world kill applies");
	// A negative float would be "below the limit" in C++, and the original's comparison had the
	// same shape: the guard is the damage type, not the sign.
	CheckEq(ShouldBlockFallDamage(true, kDmgFall, -1.0f), true, "the original's < limit comparison");
}

// --- the map-gated schema-offset cache (PITFALLS C8) ----------------------------

void TestSchemaCacheGateComesFirst() {
	// 1. No map: -1 and s2sdk is never asked (GetSchemaOffset would dereference the null serializer
	//    map, PITFALLS C8) - not even for a field another instance already knows.
	SchemaOffsetCache cache;
	ResetFake();
	g_lookupAnswer = 8; // a hint that must be ignored while the gate is closed
	CheckInt(cache.Offset(/*mapReady=*/false, kInfoClass, "m_bitsDamageType", &FakeLookup), -1,
			 "Offset without a map is -1");
	CheckInt(g_lookupCalls, 0, "Offset without a map does not call s2sdk");
	CheckInt(static_cast<long long>(cache.CachedCount()), 0, "nothing was cached without a map");

	// 2. The gate opens: the lookup runs once and the offset is memoised.
	ResetFake();
	g_lookupAnswer = 16;
	CheckInt(cache.Offset(/*mapReady=*/true, kInfoClass, "m_bitsDamageType", &FakeLookup), 16,
			 "a field offset");
	CheckInt(g_lookupCalls, 1, "the first lookup ran");
	CheckStr(g_lastClass, kInfoClass, "the lookup got the class name");
	CheckStr(g_lastField, "m_bitsDamageType", "the lookup got the field name");
	CheckInt(static_cast<long long>(cache.CachedCount()), 1, "one cached field offset");
	CheckInt(cache.Offset(/*mapReady=*/true, kInfoClass, "m_bitsDamageType", &FakeLookup), 16,
			 "the cached offset");
	CheckInt(g_lookupCalls, 1, "a cached offset is not looked up again");

	// 3. The gate closes again (OnMapEnd): -1 for every caller, without a lookup; the entry is
	//    kept, so the next map serves it again for free.
	CheckInt(cache.Offset(/*mapReady=*/false, kInfoClass, "m_bitsDamageType", &FakeLookup), -1,
			 "a remembered offset is not handed out without a map");
	CheckInt(g_lookupCalls, 1, "no lookup while the gate is closed");
	CheckInt(static_cast<long long>(cache.CachedCount()), 1, "the closed gate does not drop the entry");
	CheckInt(cache.Offset(/*mapReady=*/true, kInfoClass, "m_bitsDamageType", &FakeLookup), 16,
			 "the next map serves the remembered offset");
	CheckInt(g_lookupCalls, 1, "and does not look it up again");
}

void TestSchemaCacheMemoisation() {
	SchemaOffsetCache cache;
	ResetFake();

	// Offset 0 is a real offset (the first schema member) and is cached like any success.
	g_lookupAnswer = 0;
	CheckInt(cache.Offset(true, kInfoClass, "m_bitsDamageType", &FakeLookup), 0, "offset 0 is valid");
	CheckInt(cache.Offset(true, kInfoClass, "m_bitsDamageType", &FakeLookup), 0, "offset 0 is cached");
	CheckInt(g_lookupCalls, 1, "offset 0 is cached (>= 0)");

	// The two fields the plugin reads are independent entries.
	g_lookupAnswer = 68;
	CheckInt(cache.Offset(true, kInfoClass, "m_flDamage", &FakeLookup), 68, "the second field");
	CheckInt(g_lookupCalls, 2, "the second field was looked up");
	CheckInt(static_cast<long long>(cache.CachedCount()), 2, "two cached field offsets");
	CheckInt(cache.Offset(true, kInfoClass, "m_flDamage", &FakeLookup), 68, "the second field is cached");
	CheckInt(g_lookupCalls, 2, "no extra lookup for the cached second field");

	// A different class with the same field name is a different key.
	g_lookupAnswer = 12;
	CheckInt(cache.Offset(true, "CBaseEntity", "m_flDamage", &FakeLookup), 12,
			 "the class is part of the key");
	CheckInt(g_lookupCalls, 3, "a different class is looked up");
}

void TestSchemaCacheRetriesAndClear() {
	SchemaOffsetCache cache;
	ResetFake();

	// A failed lookup is retried on the next call instead of poisoning the field for the session.
	g_lookupAnswer = -1;
	CheckInt(cache.Offset(true, kInfoClass, "m_flDamage", &FakeLookup), -1, "a missing field");
	CheckInt(g_lookupCalls, 1, "the failed lookup ran once");
	CheckInt(static_cast<long long>(cache.CachedCount()), 0, "a -1 is not cached");
	g_lookupAnswer = 68; // e.g. the serializer came up in the meantime
	CheckInt(cache.Offset(true, kInfoClass, "m_flDamage", &FakeLookup), 68, "the retry succeeds");
	CheckInt(g_lookupCalls, 2, "the failed field was looked up again");
	CheckInt(static_cast<long long>(cache.CachedCount()), 1, "the success is cached");

	// Clear() is OnPluginEnd: the offsets of the previous instance are forgotten and the fields are
	// resolved again instead of being reused blindly.
	cache.Clear();
	CheckInt(static_cast<long long>(cache.CachedCount()), 0, "Clear() forgets every offset");
	CheckInt(cache.Offset(true, kInfoClass, "m_flDamage", &FakeLookup), 68, "after Clear() the field works");
	CheckInt(g_lookupCalls, 3, "after Clear() the field is looked up again");
}

void TestSchemaCacheKeyHandling() {
	SchemaOffsetCache cache;
	ResetFake();
	g_lookupAnswer = 4;

	// An empty or missing name is not a schema key: -1, no lookup, nothing cached.
	CheckInt(cache.Offset(true, "", "m_flDamage", &FakeLookup), -1, "an empty class name is -1");
	CheckInt(cache.Offset(true, kInfoClass, "", &FakeLookup), -1, "an empty field name is -1");
	CheckInt(cache.Offset(true, nullptr, "m_flDamage", &FakeLookup), -1, "a null class name is -1");
	CheckInt(cache.Offset(true, kInfoClass, nullptr, &FakeLookup), -1, "a null field name is -1");
	CheckInt(cache.Offset(true, kInfoClass, "m_flDamage", nullptr), -1, "a null lookup is -1");
	CheckInt(g_lookupCalls, 0, "an invalid key never reaches s2sdk");
	CheckInt(static_cast<long long>(cache.CachedCount()), 0, "an invalid key is not cached");
}

} // namespace

int main() {
	std::printf("vip_nofalldamage tests\n");
	TestDamageTypes();
	TestDamageLimit();
	TestSchemaCacheGateComesFirst();
	TestSchemaCacheMemoisation();
	TestSchemaCacheRetriesAndClear();
	TestSchemaCacheKeyHandling();
	std::printf("%d checks, %d failures\n", g_checks, g_failures);
	return g_failures == 0 ? 0 : 1;
}
