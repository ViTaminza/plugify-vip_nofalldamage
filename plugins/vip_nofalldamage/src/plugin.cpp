// vip_nofalldamage — Plugify port of the ABKAM module "[VIP] No Fall Damage"
// (ref/src/cs2-vip-modules/VIP_NoFallDamage/source/vip_nofalldamage.cpp, the module behind the
// production vip_nofalldamage.vdf).
//
// Behaviour kept identical:
//   * the feature is registered as "nofalldamage" (VIP_BOOL, TOGGLABLE, toggle callback).
//   * a per-slot flag is set from the feature on VIP_OnClientLoaded (the original's
//     OnClientAuthorized) and from the toggle callback; the original never cleared it on
//     disconnect, so a slot that once held a VIP with the feature keeps the flag - the port does
//     the same.
//   * the damage check blocks a hit when the victim's flag is set, the damage type contains
//     DMG_FALL (1 << 5) and the damage is below 4999.0 (the original's `m_flDamage < 4999.0f`;
//     the engine's "fell out of the world" kill uses a far larger value and still applies).
//
// How the damage hook works:
//   * the original used IUtilsApi::HookOnTakeDamagePre, which the Pisex utils plugin implements by
//     detouring the engine function found with the "OnTakeDamagePre" pattern of
//     addons/configs/signatures.ini and blocking a hit by returning 1 without calling the original.
//   * the port detours the same function through the official polyhook plugin
//     (polyhook::HookDetour on the s2sdk-resolved signature, a Pre callback that returns
//     Supercede with return value 1 to block, Ignored to let the engine continue).
//   * the callback receives (CCSPlayer_DamageReactServices* service, CTakeDamageInfo* info) - the
//     order is verified against the live binary (the detour's code reads [rdi+0x38], the service's
//     pawn chainer, and the damage float at [rsi+0x44]). The victim slot comes from
//     s2sdk::PlayerServicesToPlayerSlot, the documented component -> pawn -> controller -> slot
//     accessor (no hand-rolled chain read), and the damage is read through
//     CTakeDamageInfo::m_bitsDamageType / m_flDamage.
//   * polyhook indexes the declared argument list, so 0 = service and 1 = info - the earlier
//     two-candidate probe is gone with the raw chain read it needed.
//   * no exception and no s2sdk throw may leave the callback: the engine's damage path runs
//     inside it.

#include <pisex/pisex.hpp>
#include <plg/plugin.hpp>
#include <plg/string.hpp>
#include <plugin_export.h>
#include <polyhook/polyhook.hpp>
#include <s2sdk/s2sdk.hpp>

#include "damage_logic.hpp"
#include "vip_api_compat.hpp"

#include <array>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <string>
#include <string_view>
#include <vector>

namespace {

// The ported modules keep the original code's unqualified spellings (VIP_INT, TOGGLABLE,
// DISABLED, ...): the consumer wrapper declares them as plain enums in namespace vip.
using namespace vip;

IVIPApi* g_pVIPCore = &IVIPApi::Get();

constexpr int kMaxSlots = 64;
constexpr const char* kFeature = "nofalldamage";
constexpr const char* kHookName = "vip_nofalldamage_OnTakeDamagePre";

void LogInfo(std::string_view message) {
	if (plg::Log) {
		plg::Log(message, plg::Severity::Info, plg::source_location::current());
	}
}

void LogError(std::string_view message) {
	if (plg::Log) {
		plg::Log(message, plg::Severity::Error, plg::source_location::current());
	}
}

// --- schema offsets (resolved lazily, cached only when found) -------------------
int DamageTypeOffset() {
	static int offset = -1;
	if (offset < 0) {
		offset = pisex::SchemaOffset("CTakeDamageInfo", "m_bitsDamageType");
	}
	return offset;
}

int DamageOffset() {
	static int offset = -1;
	if (offset < 0) {
		offset = pisex::SchemaOffset("CTakeDamageInfo", "m_flDamage");
	}
	return offset;
}

class NoFallDamagePlugin final : public plg::Plugin {
public:
	plg::PluginResult OnPluginStart() override;
	plg::PluginResult OnPluginUpdate(std::chrono::milliseconds dt) override;
	plg::PluginResult OnPluginEnd() override;

	// Called from the polyhook callback; true = the hit is fall damage that must be blocked.
	bool ShouldBlockDamage(void* parameters, int32_t count) noexcept;
	// Returns the victim slot of a service pointer, -1 when the pointer is not a player service.
	int SlotFromServicePointer(void* service) const noexcept;

private:
	bool LoadGamedata(s2sdk::ConfigId& config, std::string& error) const;
	bool InstallHook();
	// The hook install is retried every second until it succeeds (polyhook/s2sdk may load after
	// this plugin), so a persistent failure is logged once - not once per second, which would be
	// console spam for as long as the server runs. A *different* failure is logged again.
	void LogInstallError(const std::string& message);

	std::array<bool, kMaxSlots> m_noFallDamage{};
	polyhook::HookHandle m_hook = nullptr;
	void* m_hookAddress = nullptr;
	std::chrono::milliseconds m_sinceRetry{0};
	std::string m_lastInstallError;
	bool m_installed = false;
};

NoFallDamagePlugin g_noFallDamagePlugin;

// --- polyhook callback --------------------------------------------------------
polyhook::ResultType OnTakeDamagePre(polyhook::HookHandle /*hook*/, polyhook::ParametersHandle params,
									 int32_t count, polyhook::ReturnHandle ret, polyhook::CallbackType /*type*/) {
	// The engine's damage path runs inside this callback: nothing may throw, log or allocate
	// beyond what is strictly needed.
	try {
		if (g_noFallDamagePlugin.ShouldBlockDamage(params, count)) {
			if (__polyhook_SetReturnInt32) {
				// The Pisex utils detour blocked a hit by returning 1 without calling the
				// original; the value the engine expects is the original's "applied" int.
				polyhook::SetReturnInt32(ret, 1);
			}
			return polyhook::ResultType::Supercede;
		}
	} catch (...) {
	}
	return polyhook::ResultType::Ignored;
}

plg::PluginResult NoFallDamagePlugin::OnPluginStart() {
	try {
		// Never fatal: only logs when the s2sdk bindings are missing.
		pisex::Init("vip_nofalldamage");

		g_pVIPCore->VIP_RegisterFeature(kFeature, VIP_BOOL, TOGGLABLE, nullptr,
										[this](int iSlot, const char* /*szFeature*/, VIP_ToggleState /*eOldStatus*/,
											   VIP_ToggleState& eNewStatus) {
											if (iSlot >= 0 && iSlot < kMaxSlots) {
												m_noFallDamage[static_cast<std::size_t>(iSlot)] =
													(eNewStatus == ENABLED);
											}
											return false;
										});
		g_pVIPCore->VIP_OnClientLoaded([this](int iSlot, bool bIsVIP) {
			if (!bIsVIP || iSlot < 0 || iSlot >= kMaxSlots) {
				return;
			}
			m_noFallDamage[static_cast<std::size_t>(iSlot)] =
				g_pVIPCore->VIP_GetClientFeatureBool(iSlot, kFeature);
		});

		// The detour only changes damage behaviour: when it cannot be installed the module logs it
		// and the VIP menu entry simply has no effect (fail soft, never Failed).
		if (!InstallHook()) {
			LogError("vip_nofalldamage: the damage hook is not installed - the feature has no effect "
					 "(it is retried every second)");
		}
		LogInfo("vip_nofalldamage: ready");
	} catch (const std::exception& ex) {
		LogError(std::string("vip_nofalldamage: OnPluginStart failed: ") + ex.what());
	} catch (...) {
		LogError("vip_nofalldamage: OnPluginStart failed with an unknown exception");
	}
	return {};
}

plg::PluginResult NoFallDamagePlugin::OnPluginUpdate(std::chrono::milliseconds dt) {
	pisex::Poll();
	if (m_installed) {
		return {};
	}
	m_sinceRetry += dt;
	if (m_sinceRetry < std::chrono::seconds(1)) {
		return {};
	}
	m_sinceRetry = std::chrono::milliseconds(0);
	try {
		if (InstallHook()) {
			LogInfo("vip_nofalldamage: the damage hook is installed");
		}
	} catch (...) {
	}
	return {};
}

plg::PluginResult NoFallDamagePlugin::OnPluginEnd() {
	try {
		if (m_hook != nullptr) {
			// polyhook keeps ONE detour per target function, shared by every plugin (HookDetour on
			// an already-hooked target hands back the existing callback), and UnhookDetour removes
			// it for everyone - s2sdk's own hooks included. So: drop our handler, then unhook only
			// when we were the last one (AreCallbacksRegistered). Never UnhookAll, never the
			// trampoline as the key.
			if (__polyhook_RemoveCallback) {
				try {
					polyhook::RemoveCallback(m_hook, polyhook::CallbackType::Pre, &OnTakeDamagePre);
				} catch (...) {
				}
			}
			if (m_hookAddress != nullptr && __polyhook_UnhookDetour && __polyhook_AreCallbacksRegistered &&
				!polyhook::AreCallbacksRegistered(m_hook)) {
				try {
					polyhook::UnhookDetour(m_hookAddress);
				} catch (...) {
				}
			}
			m_hook = nullptr;
			m_hookAddress = nullptr;
			m_installed = false;
		}
	} catch (...) {
	}
	g_pVIPCore->VIP_OnClientLoadedUnregister();
	// The vip API has no feature unregister: re-registering the feature without callbacks
	// replaces the core's entry with null delegates, so the core keeps no pointer into this
	// library after it is unloaded (same value/feature type: the menu entry is unchanged).
	g_pVIPCore->VIP_RegisterFeature(kFeature, VIP_BOOL, TOGGLABLE);
	pisex::Shutdown();
	return {};
}

bool NoFallDamagePlugin::LoadGamedata(s2sdk::ConfigId& config, std::string& error) const {
	if (!__s2sdk_LoadGameConfigFile) {
		error = "s2sdk LoadGameConfigFile binding is not available";
		return false;
	}
	// GetLocation is the package root or the bin/ directory holding the .so; package.sh ships
	// gamedata.jsonc in both places (like hudmenu).
	const plg::string locationPlg = GetLocation();
	const std::string location(locationPlg.data(), locationPlg.size());
	std::vector<std::filesystem::path> candidates;
	if (!location.empty()) {
		const std::filesystem::path loc(location);
		candidates.push_back(loc / "gamedata.jsonc");
		candidates.push_back(loc.parent_path() / "gamedata.jsonc");
		candidates.push_back(loc.parent_path().parent_path() / "gamedata.jsonc");
	}
	plg::vector<plg::string> paths;
	std::string checked;
	for (const std::filesystem::path& candidate : candidates) {
		std::error_code existsError;
		if (std::filesystem::exists(candidate, existsError)) {
			paths.push_back(plg::string(candidate.string()));
		}
		if (!checked.empty()) {
			checked += ", ";
		}
		checked += candidate.string();
	}
	if (paths.empty()) {
		error = "gamedata.jsonc not found (looked for: " + checked + ")";
		return false;
	}
	config = s2sdk::LoadGameConfigFile(paths);
	if (config == static_cast<s2sdk::ConfigId>(-1)) {
		error = "s2sdk could not load the vip_nofalldamage gamedata";
		return false;
	}
	return true;
}

bool NoFallDamagePlugin::InstallHook() {
	if (m_installed) {
		return true;
	}
	if (__polyhook_HookDetour == nullptr || __polyhook_AddCallback == nullptr || __polyhook_GetArgumentPointer == nullptr) {
		LogInstallError("vip_nofalldamage: polyhook bindings are not available (is the polyhook plugin installed?)");
		return false;
	}
	if (__s2sdk_GetGameConfigSignature == nullptr) {
		LogInstallError("vip_nofalldamage: s2sdk GetGameConfigSignature binding is not available");
		return false;
	}

	s2sdk::ConfigId config = static_cast<s2sdk::ConfigId>(-1);
	std::string error;
	if (!LoadGamedata(config, error)) {
		LogInstallError("vip_nofalldamage: " + error);
		return false;
	}
	void* address = nullptr;
	try {
		address = s2sdk::GetGameConfigSignature(config, plg::string("OnTakeDamagePre"));
	} catch (...) {
		address = nullptr;
	}
	// The s2sdk wrapper calls the binding slot unchecked (a try/catch does not catch a null call).
	if (__s2sdk_CloseGameConfigFile) {
		try {
			s2sdk::CloseGameConfigFile(config);
		} catch (...) {
		}
	}
	if (address == nullptr) {
		LogInstallError("vip_nofalldamage: the OnTakeDamagePre signature was not found in the game library "
						"(is gamedata.jsonc current for this game build?)");
		return false;
	}

	// int OnTakeDamagePre(CCSPlayer_DamageReactServices* service, CTakeDamageInfo* info)
	plg::vector<polyhook::DataType> arguments;
	arguments.push_back(polyhook::DataType::Pointer);
	arguments.push_back(polyhook::DataType::Pointer);

	polyhook::HookHandle hook = nullptr;
	try {
		hook = polyhook::HookDetour(address, polyhook::DataType::Int32, arguments, -1, plg::string(kHookName));
	} catch (...) {
		hook = nullptr;
	}
	if (hook == nullptr) {
		std::string polyhookError;
		try {
			if (__polyhook_GetError) {
				polyhookError = std::string(std::string_view(polyhook::GetError()));
			}
		} catch (...) {
		}
		std::string message = "vip_nofalldamage: polyhook::HookDetour failed";
		if (!polyhookError.empty()) {
			message += ": " + polyhookError;
		}
		LogInstallError(message);
		return false;
	}

	bool callbackAdded = false;
	try {
		callbackAdded = polyhook::AddCallback(hook, polyhook::CallbackType::Pre, &OnTakeDamagePre);
	} catch (...) {
		callbackAdded = false;
	}
	if (!callbackAdded) {
		// The detour may be shared (another plugin hooked the same target first and HookDetour
		// returned its callback), so it is only removed when nobody holds a callback on it any
		// more - UnhookDetour would otherwise rip the hook out from under the other plugin.
		if (__polyhook_AreCallbacksRegistered && __polyhook_UnhookDetour &&
			!polyhook::AreCallbacksRegistered(hook)) {
			try {
				polyhook::UnhookDetour(address);
			} catch (...) {
			}
		}
		LogInstallError("vip_nofalldamage: polyhook::AddCallback failed for the damage hook");
		return false;
	}

	m_hook = hook;
	m_hookAddress = address;
	m_installed = true;
	m_lastInstallError.clear(); // a failure after a successful install is reported again
	return true;
}

void NoFallDamagePlugin::LogInstallError(const std::string& message) {
	// The install is retried once a second; the same failure is reported once (the operator
	// already has the line and per-second repetitions are console noise), a different one again.
	if (m_lastInstallError == message) {
		return;
	}
	m_lastInstallError = message;
	LogError(message);
}

int NoFallDamagePlugin::SlotFromServicePointer(void* service) const noexcept {
	// The documented accessor walks the service's pawn chain itself (a CPlayerPawnComponent's
	// 40-byte CNetworkVarChainer -> the pawn -> its controller -> the slot) and answers -1 when
	// any step fails, so no hand-rolled chain read is needed here.
	if (service == nullptr || !__s2sdk_PlayerServicesToPlayerSlot) {
		return -1;
	}
	try {
		const int slot = s2sdk::PlayerServicesToPlayerSlot(service);
		if (slot < 0 || slot >= kMaxSlots) {
			return -1;
		}
		return slot;
	} catch (...) {
		return -1;
	}
}

bool NoFallDamagePlugin::ShouldBlockDamage(void* parameters, int32_t count) noexcept {
	if (parameters == nullptr || count < 2 || !__polyhook_GetArgumentPointer) {
		return false;
	}
	// The s2sdk wrappers call their binding slots unchecked - a missing one would be a null call
	// from inside the engine's damage path.
	if (!__s2sdk_GetEntData2 || !__s2sdk_GetEntDataFloat2) {
		return false;
	}
	if (DamageTypeOffset() < 0 || DamageOffset() < 0) {
		return false; // schema not ready: never guess an offset
	}
	// The hook signature is verified against the live binary: int f(CCSPlayer_DamageReactServices*
	// service, CTakeDamageInfo* info) - the detour's disassembly reads [rdi+0x38] (the service's
	// pawn chainer) and the damage float at [rsi+0x44]. polyhook indexes the declared argument
	// list, so 0 = service, 1 = info. The two-candidate probe is gone: it needed a raw chain read
	// to validate a candidate before PlayerServicesToPlayerSlot dereferences it, and the order
	// does not have to be guessed.
	void* service = nullptr;
	void* info = nullptr;
	try {
		service = polyhook::GetArgumentPointer(parameters, 0);
		info = polyhook::GetArgumentPointer(parameters, 1);
	} catch (...) {
		return false;
	}
	if (service == nullptr || info == nullptr) {
		return false;
	}
	const int slot = SlotFromServicePointer(service);
	if (slot < 0 || !m_noFallDamage[static_cast<std::size_t>(slot)]) {
		return false;
	}
	const int32_t damageType =
		static_cast<int32_t>(s2sdk::GetEntData2(info, DamageTypeOffset(), static_cast<int>(sizeof(int32_t))));
	const float damage =
		static_cast<float>(s2sdk::GetEntDataFloat2(info, DamageOffset(), static_cast<int>(sizeof(float))));
	// The original's decision (damage_logic.hpp, shared with the test).
	return vip_nofalldamage::ShouldBlockFallDamage(true, damageType, damage);
}

} // namespace

PLUGIFY_PLUGIN(PLUGIN_API, &g_noFallDamagePlugin)
