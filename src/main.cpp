// AimAssist - controller aim assist for Skyrim SE/AE (SKSE plugin, CommonLibSSE-NG)
//
// Hooks LookHandler::ProcessThumbstick (the right-stick look handler). Because it only
// ever sees gamepad thumbstick events, keyboard/mouse play is never affected.
//
//  * Friction:   scales stick input down while a valid target is inside the cone.
//  * Magnetism:  adds a small stick bias toward the target's aim point.

#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <spdlog/sinks/basic_file_sink.h>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <cstdlib>
#include <fstream>
#include <string>

namespace
{
	namespace logger = SKSE::log;

	constexpr float kPi = 3.14159265358979f;

	struct Settings
	{
		bool  enabled{ true };
		bool  requireWeaponDrawn{ true };
		bool  bowsAndSpellsOnly{ true };
		bool  hostileOnly{ true };
		bool  requireLineOfSight{ true };
		float maxDistance{ 4000.0f };
		float coneDegrees{ 10.0f };
		float aimHeight{ 0.7f };
		bool  frictionEnabled{ true };
		float frictionStrength{ 0.45f };
		bool  magnetismEnabled{ true };
		float magnetismStrength{ 5.0f };
		bool  requireStickInput{ true };
		bool  invertYaw{ false };
		bool  invertPitch{ false };
	};

	Settings g;

	std::string Trim(const std::string& s)
	{
		const auto a = s.find_first_not_of(" \t\r\n");
		if (a == std::string::npos) {
			return {};
		}
		const auto b = s.find_last_not_of(" \t\r\n");
		return s.substr(a, b - a + 1);
	}

	bool ToBool(const std::string& v)
	{
		return v == "true" || v == "1" || v == "True" || v == "TRUE" || v == "yes";
	}

	bool ParseFile(const char* path)
	{
		std::ifstream in(path);
		if (!in) {
			return false;
		}

		std::string section, line;
		while (std::getline(in, line)) {
			line = Trim(line);
			if (line.empty() || line[0] == ';' || line[0] == '#') {
				continue;
			}
			if (line.front() == '[' && line.back() == ']') {
				section = line.substr(1, line.size() - 2);
				continue;
			}
			const auto eq = line.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			auto key = Trim(line.substr(0, eq));
			// MCM Helper keys carry a type prefix (fStrength, bEnabled); strip it.
			if (key.size() > 1 && std::string("fibs").find(key[0]) != std::string::npos &&
				std::isupper(static_cast<unsigned char>(key[1]))) {
				key.erase(0, 1);
			}
			const auto val = Trim(line.substr(eq + 1));
			const float f = static_cast<float>(std::atof(val.c_str()));

			if (section == "General") {
				if (key == "Enabled") g.enabled = ToBool(val);
				else if (key == "RequireWeaponDrawn") g.requireWeaponDrawn = ToBool(val);
				else if (key == "BowsAndSpellsOnly") g.bowsAndSpellsOnly = ToBool(val);
				else if (key == "HostileOnly") g.hostileOnly = ToBool(val);
				else if (key == "RequireLineOfSight") g.requireLineOfSight = ToBool(val);
				else if (key == "MaxDistance") g.maxDistance = f;
			} else if (section == "Targeting") {
				if (key == "ConeDegrees") g.coneDegrees = f;
				else if (key == "AimHeight") g.aimHeight = f;
			} else if (section == "Friction") {
				if (key == "Enabled") g.frictionEnabled = ToBool(val);
				else if (key == "Strength") g.frictionStrength = f;
			} else if (section == "Magnetism") {
				if (key == "Enabled") g.magnetismEnabled = ToBool(val);
				else if (key == "Strength") g.magnetismStrength = f;
				else if (key == "RequireStickInput") g.requireStickInput = ToBool(val);
				else if (key == "InvertYaw") g.invertYaw = ToBool(val);
				else if (key == "InvertPitch") g.invertPitch = ToBool(val);
			}
		}
		return true;
	}

	constexpr auto kBaseIni = "Data/SKSE/Plugins/AimAssist.ini";
	constexpr auto kMcmIni = "Data/MCM/Settings/AimAssist.ini";  // written by MCM Helper

	// Base INI first, then MCM overrides on top.
	void LoadSettings()
	{
		g = Settings{};
		if (!ParseFile(kBaseIni)) {
			logger::warn("AimAssist.ini not found, using defaults");
		}
		if (ParseFile(kMcmIni)) {
			logger::info("Applied MCM settings");
		}
	}

	// Cheap hot reload so MCM changes apply without restarting.
	void MaybeReload()
	{
		static auto lastCheck = std::chrono::steady_clock::now();
		static std::filesystem::file_time_type lastWrite{};

		const auto now = std::chrono::steady_clock::now();
		if (now - lastCheck < std::chrono::seconds(1)) {
			return;
		}
		lastCheck = now;

		std::error_code ec;
		const auto t = std::filesystem::last_write_time(kMcmIni, ec);
		if (ec || t == lastWrite) {
			return;
		}
		lastWrite = t;
		LoadSettings();
	}

	float WrapPi(float a)
	{
		while (a > kPi) a -= 2.0f * kPi;
		while (a < -kPi) a += 2.0f * kPi;
		return a;
	}

	bool UsingRangedAttack(RE::PlayerCharacter* player)
	{
		if (!g.bowsAndSpellsOnly) {
			return true;
		}
		for (const bool left : { false, true }) {
			auto* obj = player->GetEquippedObject(left);
			if (!obj) {
				continue;
			}
			if (obj->Is(RE::FormType::Spell)) {
				return true;
			}
			if (auto* weap = obj->As<RE::TESObjectWEAP>()) {
				if (weap->IsBow() || weap->IsCrossbow()) {
					return true;
				}
			}
		}
		return false;
	}

	// Finds the best target inside the cone. Returns false if none.
	// outYawDelta / outPitchDelta are radians the view must turn to center the target.
	// outFalloff is 1.0 at the center of the cone and 0.0 at its edge.
	bool FindTarget(RE::PlayerCharacter* player, float& outYawDelta, float& outPitchDelta, float& outFalloff)
	{
		auto* lists = RE::ProcessLists::GetSingleton();
		if (!lists) {
			return false;
		}

		const auto eye = player->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, player->GetHeight() * 0.93f };
		const float curYaw = player->GetAngleZ();
		const float curPitch = player->GetAngleX();  // positive = looking down
		const float cone = g.coneDegrees * kPi / 180.0f;
		const float maxDistSq = g.maxDistance * g.maxDistance;

		float best = cone;
		bool  found = false;

		for (auto& handle : lists->highActorHandles) {
			auto actorPtr = handle.get();
			auto* actor = actorPtr.get();
			if (!actor || actor == player || actor->IsDead() || actor->IsDisabled()) {
				continue;
			}
			if (g.hostileOnly && !actor->IsHostileToActor(player)) {
				continue;
			}

			const auto aim = actor->GetPosition() + RE::NiPoint3{ 0.0f, 0.0f, actor->GetHeight() * g.aimHeight };
			const auto d = aim - eye;
			const float distSq = d.x * d.x + d.y * d.y + d.z * d.z;
			if (distSq > maxDistSq || distSq < 1.0f) {
				continue;
			}

			const float flat = std::sqrt(d.x * d.x + d.y * d.y);
			const float wantYaw = std::atan2(d.x, d.y);
			const float wantPitch = -std::atan2(d.z, flat);
			const float yawDelta = WrapPi(wantYaw - curYaw);
			const float pitchDelta = wantPitch - curPitch;
			const float ang = std::sqrt(yawDelta * yawDelta + pitchDelta * pitchDelta);
			if (ang >= best) {
				continue;
			}

			if (g.requireLineOfSight) {
				bool los = false;
				player->HasLineOfSight(actor, los);
				if (!los) {
					continue;
				}
			}

			best = ang;
			outYawDelta = yawDelta;
			outPitchDelta = pitchDelta;
			outFalloff = 1.0f - (ang / cone);
			found = true;
		}
		return found;
	}

	struct LookHook
	{
		static void Thunk(RE::PlayerInputHandler* a_this, RE::ThumbstickEvent* a_event)
		{
			MaybeReload();
			if (g.enabled && a_event && a_event->IsRight()) {
				Adjust(a_event);
			}
			func(a_this, a_event);
		}

		static void Adjust(RE::ThumbstickEvent* ev)
		{
			auto* player = RE::PlayerCharacter::GetSingleton();
			if (!player || !player->Is3DLoaded()) {
				return;
			}
			if (g.requireWeaponDrawn && !player->AsActorState()->IsWeaponDrawn()) {
				return;
			}
			if (!UsingRangedAttack(player)) {
				return;
			}

			float yawDelta = 0.0f, pitchDelta = 0.0f, falloff = 0.0f;
			if (!FindTarget(player, yawDelta, pitchDelta, falloff)) {
				return;
			}

			const bool hasInput = std::fabs(ev->xValue) > 0.05f || std::fabs(ev->yValue) > 0.05f;

			if (g.frictionEnabled && hasInput) {
				const float scale = std::clamp(1.0f - g.frictionStrength * falloff, 0.05f, 1.0f);
				ev->xValue *= scale;
				ev->yValue *= scale;
			}

			if (g.magnetismEnabled && (hasInput || !g.requireStickInput)) {
				// stick x right turns right (yaw up); stick y up looks up (pitch down)
				float biasX = yawDelta * g.magnetismStrength * falloff;
				float biasY = -pitchDelta * g.magnetismStrength * falloff;
				if (g.invertYaw) biasX = -biasX;
				if (g.invertPitch) biasY = -biasY;
				ev->xValue = std::clamp(ev->xValue + std::clamp(biasX, -0.5f, 0.5f), -1.0f, 1.0f);
				ev->yValue = std::clamp(ev->yValue + std::clamp(biasY, -0.5f, 0.5f), -1.0f, 1.0f);
			}
		}

		static inline REL::Relocation<decltype(Thunk)> func;
	};

	void SetupLog()
	{
		auto path = logger::log_directory();
		if (!path) {
			return;
		}
		*path /= "AimAssist.log";
		auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
		auto log = std::make_shared<spdlog::logger>("global", std::move(sink));
		log->set_level(spdlog::level::info);
		log->flush_on(spdlog::level::info);
		spdlog::set_default_logger(std::move(log));
		spdlog::set_pattern("[%H:%M:%S] [%l] %v");
	}
}

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
	SKSE::Init(skse);
	SetupLog();
	logger::info("AimAssist loading");
	LoadSettings();

	// vfunc 0 dtor, 1 CanProcess, 2 ProcessThumbstick, 3 ProcessMouseMove, 4 ProcessButton
	REL::Relocation<std::uintptr_t> vtbl{ RE::VTABLE_LookHandler[0] };
	LookHook::func = vtbl.write_vfunc(0x2, LookHook::Thunk);

	logger::info("Look handler hooked");
	return true;
}
