#include <Cheat.hpp>
#include "AlertManager.hpp"
#include "WorldCache.hpp"
#include "settings/Settings.hpp"
#include "main.hpp"
#include "menu/string_encryption.hpp"

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <random>
#include <vector>

void DrawEnemyArrows(ImDrawList* dl, float bearingDeg, const ImVec2& center, float ringRadius, ImU32 color, float scale, bool pulse);

extern volatile LONG g_coreReady;

namespace
{
	constexpr float kAlertClearHold = 0.32f;
	constexpr float kDtMax = 0.10f;
	constexpr uint64_t kPlayerStaleMs = 1500;
	constexpr uint64_t kCameraStaleMs = 500;
	constexpr float kRingPx = 72.0f;
	constexpr float kVelMaxUu = 2500.0f;
	constexpr float kDirMinUu = 1.0f;
	constexpr float kTeleportM = 8.0f;
	constexpr int kHistCap = 48;
	constexpr uint64_t kHistTimeoutMs = 2000;
	constexpr uint64_t kSampleMinMs = 50;
	constexpr uint64_t kSampleMaxMs = 400;
	constexpr float kInvalidVelGrace = 0.25f;
	constexpr uint64_t kDwellMs = 350;
	constexpr float kSectorHystDeg = 8.0f;

	enum class AlertId : int { Nearby = 0, Multiple = 1, Behind = 2, Count = 3 };
	enum class AlertState : uint8_t { Inactive, Pending, Visible, Fading };
	enum class ThreatConf : int { None = 0, Nearby = 1, Approaching = 2, ClosingFast = 3, PossibleEngagement = 4 };

	struct Threat
	{
		uint64_t id = 0;
		float distM = 0.0f;
		float bearingDeg = 0.0f;
		float velX = 0.0f;
		float velY = 0.0f;
		bool velValid = false;
		bool isZooming = false;
		bool isFiring = false;
		float closingMps = 0.0f;
		ThreatConf conf = ThreatConf::None;
	};

	struct Alert
	{
		AlertId id = AlertId::Nearby;
		AlertState state = AlertState::Inactive;
		char text[96] = {};
		int count = -1;
		float pendingRemain = 0.0f;
		float clearHold = 0.0f;
		float fadeRemain = 0.0f;
		float fadeTotal = 0.0f;
		float progress = 0.0f;
		float lastBearingDeg = 0.0f;
		bool lastBearingValid = false;
		uint64_t lastId = 0;
	};

	struct ArrowDraw
	{
		bool valid = false;
		float bearingDeg = 0.0f;
		float alpha = 1.0f;
	};

	struct Track
	{
		uint64_t id = 0;
		uint64_t lastMs = 0;
		float prevDist = -1.0f;
		uint64_t prevDistMs = 0;
		float sustain = 0.0f;
		float clearAcc = 0.0f;
		float invalidGrace = 0.0f;
		bool approachingActive = false;
		bool closingFastActive = false;
		bool occupied = false;
	};

	Alert g_alerts[(int)AlertId::Count]{};
	ArrowDraw g_arrow{};
	Track g_tracks[kHistCap]{};
	bool g_inited = false;
	uint64_t g_selectedId = 0;
	uint64_t g_selectedSinceMs = 0;
	int g_latchedSector = -1;
	int g_latchedWays = 8;

	float SanitizeDt(float dt)
	{
		if (!std::isfinite(dt) || dt <= 0.0f)
			return 0.0f;
		return (std::min)(dt, kDtMax);
	}

	float SampleDelay()
	{
		if (!warningDelayJitter)
			return 0.0f;
		float lo = warningDelayMin;
		float hi = warningDelayMax;
		if (!std::isfinite(lo)) lo = 0.0f;
		if (!std::isfinite(hi)) hi = 0.0f;
		if (hi < lo) { const float t = lo; lo = hi; hi = t; }
		lo = std::clamp(lo, 0.0f, 1.0f);
		hi = std::clamp(hi, 0.0f, 1.5f);
		if (hi <= 0.0f)
			return 0.0f;
		static std::mt19937 rng{ (unsigned)GetTickCount64() };
		std::uniform_real_distribution<float> dist(lo, hi);
		return dist(rng);
	}

	bool IsWorldNoise(const PlayerDynamicData& pd)
	{
		return pd.npc || pd.drone || pd.loot || pd.container || pd.extraction ||
			pd.corpse || pd.raidercache || pd.salvage || pd.grenade ||
			pd.mine || pd.trap || pd.arcprobe || pd.destroyed;
	}

	bool IsDead(const PlayerDynamicData& pd)
	{
		return pd.maxHealth > 0.0 && pd.currentHealth <= 0.0;
	}

	bool IsDowned(const PlayerDynamicData& pd)
	{
		return pd.isDBNOKnown && pd.isDBNO;
	}

	float HorizontalBearingDeg(const Vector3& camLoc, float yawDeg, const Vector3& worldLoc)
	{
		const float yawRad = yawDeg * (float)(M_PI / 180.0);
		const float fx = cosf(yawRad);
		const float fy = sinf(yawRad);
		const float dx = (float)(worldLoc.x - camLoc.x);
		const float dy = (float)(worldLoc.y - camLoc.y);
		const float len2 = dx * dx + dy * dy;
		if (!(len2 > 1.0f) || !std::isfinite(len2))
			return 0.0f;
		const float inv = 1.0f / sqrtf(len2);
		const float nx = dx * inv;
		const float ny = dy * inv;
		const float cross = fx * ny - fy * nx;
		const float dot = fx * nx + fy * ny;
		if (!std::isfinite(cross) || !std::isfinite(dot))
			return 0.0f;
		return atan2f(cross, dot) * (180.0f / (float)M_PI);
	}

	float Normalize180(float a)
	{
		if (!std::isfinite(a))
			return a;
		while (a > 180.0f) a -= 360.0f;
		while (a < -180.0f) a += 360.0f;
		return a;
	}

	float To360(float bearing180)
	{
		return (bearing180 < 0.0f) ? (bearing180 + 360.0f) : bearing180;
	}

	int Sector8(float bearing180)
	{
		float u = To360(Normalize180(bearing180)) + 22.5f;
		if (u >= 360.0f) u -= 360.0f;
		if (u < 0.0f) u += 360.0f;
		int idx = (int)floorf(u / 45.0f);
		if (idx < 0) idx = 0;
		if (idx > 7) idx = 7;
		return idx;
	}

	int Sector4(float bearing180)
	{
		float u = To360(Normalize180(bearing180)) + 45.0f;
		if (u >= 360.0f) u -= 360.0f;
		if (u < 0.0f) u += 360.0f;
		int idx = (int)floorf(u / 90.0f);
		if (idx < 0) idx = 0;
		if (idx > 3) idx = 3;
		return idx;
	}

	float SectorCenter360(int idx, int ways)
	{
		if (ways == 4)
			return (float)idx * 90.0f;
		return (float)idx * 45.0f;
	}

	float AngAbsDiff360(float a, float b)
	{
		float d = a - b;
		while (d > 180.0f) d -= 360.0f;
		while (d < -180.0f) d += 360.0f;
		return fabsf(d);
	}

	int LatchSector(float bearing180, int raw, int latched, int ways)
	{
		if (latched < 0)
			return raw;
		const float half = (ways == 4) ? 45.0f : 22.5f;
		const float a360 = To360(Normalize180(bearing180));
		if (AngAbsDiff360(a360, SectorCenter360(latched, ways)) <= half + kSectorHystDeg)
			return latched;
		return raw;
	}

	const char* SectorName8(int idx)
	{
		static const char* kNames[8] = {
			"Front", "Front Right", "Right", "Back Right",
			"Behind", "Back Left", "Left", "Front Left"
		};
		if (idx < 0 || idx > 7) return "Front";
		return kNames[idx];
	}

	const char* SectorName4(int idx)
	{
		static const char* kNames[4] = { "Front", "Right", "Behind", "Left" };
		if (idx < 0 || idx > 3) return "Front";
		return kNames[idx];
	}

	void SetText(Alert& a, const char* s)
	{
		if (!s) { a.text[0] = '\0'; return; }
		if (strncmp(a.text, s, sizeof(a.text)) == 0)
			return;
		strncpy_s(a.text, s, _TRUNCATE);
	}

	void ForceInactive(Alert& a)
	{
		const AlertId id = a.id;
		a = Alert{};
		a.id = id;
	}

	void ClearTracks()
	{
		for (int i = 0; i < kHistCap; ++i)
			g_tracks[i] = Track{};
		g_selectedId = 0;
		g_selectedSinceMs = 0;
		g_latchedSector = -1;
	}

	void EnsureInit()
	{
		if (g_inited)
			return;
		g_alerts[(int)AlertId::Nearby].id = AlertId::Nearby;
		g_alerts[(int)AlertId::Multiple].id = AlertId::Multiple;
		g_alerts[(int)AlertId::Behind].id = AlertId::Behind;
		g_inited = true;
	}

	struct BufGuard
	{
		NT::Store::CScene::PlayerBuffer* buf = nullptr;
		~BufGuard()
		{
			if (buf)
				buf->Release();
		}
	};

	Track* FindTrack(uint64_t id)
	{
		for (int i = 0; i < kHistCap; ++i)
		{
			if (g_tracks[i].occupied && g_tracks[i].id == id)
				return &g_tracks[i];
		}
		return nullptr;
	}

	Track* AllocTrack(uint64_t id, uint64_t nowMs)
	{
		Track* oldest = nullptr;
		for (int i = 0; i < kHistCap; ++i)
		{
			if (!g_tracks[i].occupied)
			{
				g_tracks[i] = Track{};
				g_tracks[i].occupied = true;
				g_tracks[i].id = id;
				g_tracks[i].lastMs = nowMs;
				return &g_tracks[i];
			}
			if (!oldest || g_tracks[i].lastMs < oldest->lastMs)
				oldest = &g_tracks[i];
		}
		if (!oldest)
			return nullptr;
		*oldest = Track{};
		oldest->occupied = true;
		oldest->id = id;
		oldest->lastMs = nowMs;
		return oldest;
	}

	void PruneTracks(uint64_t nowMs)
	{
		for (int i = 0; i < kHistCap; ++i)
		{
			if (!g_tracks[i].occupied)
				continue;
			if (nowMs < g_tracks[i].lastMs || (nowMs - g_tracks[i].lastMs) > kHistTimeoutMs)
				g_tracks[i] = Track{};
		}
	}

	bool VelocityValid(float vx, float vy)
	{
		if (!std::isfinite(vx) || !std::isfinite(vy))
			return false;
		const float len2 = vx * vx + vy * vy;
		if (!std::isfinite(len2))
			return false;
		if (len2 > kVelMaxUu * kVelMaxUu)
			return false;
		return true;
	}

	float HorizontalClosingMps(const Vector3& cam, const Vector3& enemy, float vx, float vy)
	{
		const float dx = (float)(cam.x - enemy.x);
		const float dy = (float)(cam.y - enemy.y);
		const float len2 = dx * dx + dy * dy;
		if (!(len2 >= kDirMinUu * kDirMinUu) || !std::isfinite(len2))
			return 0.0f;
		const float inv = 1.0f / sqrtf(len2);
		if (!std::isfinite(inv))
			return 0.0f;
		const float nx = dx * inv;
		const float ny = dy * inv;
		const float dot = vx * nx + vy * ny;
		if (!std::isfinite(dot))
			return 0.0f;
		return dot / 100.0f;
	}

	void CollectThreats(const std::vector<PlayerDynamicData>& data, const CachedCameraData& cam, uint64_t nowMs, std::vector<Threat>& out)
	{
		out.clear();
		for (const auto& pd : data)
		{
			if (pd.entity == 0)
				continue;
			if (pd.stale)
				continue;
			if (pd.is_team)
				continue;
			if (IsWorldNoise(pd))
				continue;
			if (IsDead(pd) || IsDowned(pd))
				continue;
			if (pd.lastUpdateMs == 0 || nowMs < pd.lastUpdateMs || (nowMs - pd.lastUpdateMs) > kPlayerStaleMs)
				continue;
			if (!LooksValidWorldPos(pd.worldLoc) || pd.worldLoc.IsZero())
				continue;
			if (NT::Store::BoneSyncer && NT::Store::BoneSyncer->IsFrozenGhost(pd.entity))
				continue;

			const double len = (pd.worldLoc - cam.location).Length();
			if (!std::isfinite(len) || len < 1.0)
				continue;
			const float distM = (float)(len / 100.0);
			if (!std::isfinite(distM) || distM < 0.0f)
				continue;

			const float bearing = HorizontalBearingDeg(cam.location, (float)cam.rotation.Yaw, pd.worldLoc);
			if (!std::isfinite(bearing))
				continue;

			Threat t{};
			t.id = pd.entity;
			t.distM = distM;
			t.bearingDeg = Normalize180(bearing);
			t.velX = (float)pd.velocity.x;
			t.velY = (float)pd.velocity.y;
			t.velValid = VelocityValid(t.velX, t.velY);
			t.isZooming = pd.isZooming;
			t.isFiring = pd.isFiring;
			t.closingMps = t.velValid ? HorizontalClosingMps(cam.location, pd.worldLoc, t.velX, t.velY) : 0.0f;
			if (!std::isfinite(t.closingMps))
			{
				t.closingMps = 0.0f;
				t.velValid = false;
			}
			out.push_back(t);
		}
	}

	void UpdateTrackMotion(Track& tr, const Threat& t, uint64_t nowMs, float dt)
	{
		bool teleported = false;
		if (tr.prevDist >= 0.0f && tr.prevDistMs != 0 && nowMs > tr.prevDistMs)
		{
			const uint64_t interval = nowMs - tr.prevDistMs;
			if (interval >= kSampleMinMs && interval <= kSampleMaxMs)
			{
				const float jump = fabsf(t.distM - tr.prevDist);
				if (jump > kTeleportM)
					teleported = true;
			}
			else if (interval > kSampleMaxMs)
			{
				tr.prevDist = -1.0f;
				tr.prevDistMs = 0;
			}
		}

		if (teleported)
		{
			tr.prevDist = t.distM;
			tr.prevDistMs = nowMs;
			tr.sustain = 0.0f;
			return;
		}

		if (tr.prevDistMs == 0 || nowMs < tr.prevDistMs || (nowMs - tr.prevDistMs) >= kSampleMinMs)
		{
			tr.prevDist = t.distM;
			tr.prevDistMs = nowMs;
		}

		if (!approachingAlertsEnabled)
		{
			tr.approachingActive = false;
			tr.closingFastActive = false;
			tr.sustain = 0.0f;
			tr.clearAcc = 0.0f;
			tr.invalidGrace = 0.0f;
			return;
		}

		const float maxDist = approachingMaxDistance;
		const float maxStay = maxDist * 1.12f;
		const bool inRange = t.distM <= (tr.approachingActive ? maxStay : maxDist);

		if (!t.velValid)
		{
			if (tr.approachingActive)
			{
				tr.invalidGrace += dt;
				if (tr.invalidGrace >= kInvalidVelGrace)
					tr.clearAcc += dt;
			}
			else
			{
				tr.sustain = 0.0f;
				tr.clearAcc = 0.0f;
			}
			if (tr.clearAcc >= approachingClearDebounce)
			{
				tr.approachingActive = false;
				tr.closingFastActive = false;
				tr.sustain = 0.0f;
				tr.clearAcc = 0.0f;
				tr.invalidGrace = 0.0f;
			}
			return;
		}

		tr.invalidGrace = 0.0f;
		const float enterSpd = approachingMinimumSpeed;
		const float fastSpd = approachingFastSpeed;
		const float clearSpd = approachingClearSpeed;

		if (!inRange)
		{
			tr.clearAcc += dt;
			if (tr.clearAcc >= approachingClearDebounce || !tr.approachingActive)
			{
				tr.approachingActive = false;
				tr.closingFastActive = false;
				tr.sustain = 0.0f;
				tr.clearAcc = 0.0f;
			}
			return;
		}

		if (t.closingMps >= enterSpd)
		{
			tr.clearAcc = 0.0f;
			tr.sustain += dt;
			if (tr.sustain >= approachingMinimumDuration)
				tr.approachingActive = true;
			if (tr.approachingActive && t.closingMps >= fastSpd)
				tr.closingFastActive = true;
			else if (tr.closingFastActive && t.closingMps < fastSpd)
				tr.closingFastActive = false;
		}
		else if (t.closingMps > clearSpd)
		{
			// Between clear and enter: hold sustain, do not reset from one noisy frame.
		}
		else
		{
			tr.clearAcc += dt;
			if (tr.clearAcc >= approachingClearDebounce)
			{
				tr.approachingActive = false;
				tr.closingFastActive = false;
				tr.sustain = 0.0f;
				tr.clearAcc = 0.0f;
			}
		}
	}

	ThreatConf Classify(const Threat& t, const Track& tr)
	{
		ThreatConf c = ThreatConf::None;
		if (nearbyEnemyEnabled && t.distM <= nearbyEnemyDistance)
			c = ThreatConf::Nearby;

		if (approachingAlertsEnabled)
		{
			if (tr.closingFastActive)
				c = ThreatConf::ClosingFast;
			else if (tr.approachingActive)
				c = ThreatConf::Approaching;
		}

		if (possibleEngagementEnabled &&
			(c == ThreatConf::Approaching || c == ThreatConf::ClosingFast))
		{
			const bool zoom = possibleEngagementUseZooming && t.isZooming;
			const bool fire = possibleEngagementUseFiring && t.isFiring;
			if (zoom || fire)
				c = ThreatConf::PossibleEngagement;
		}
		return c;
	}

	float ScoreThreat(const Threat& t, uint64_t selectedId)
	{
		float s = (float)((int)t.conf) * 100.0f;
		if (t.velValid)
			s += std::clamp(t.closingMps, 0.0f, 12.0f) * 8.0f;
		const float maxD = (std::max)(1.0f, approachingMaxDistance);
		s += (1.0f - std::clamp(t.distM / maxD, 0.0f, 1.0f)) * 20.0f;
		if (fabsf(t.bearingDeg) >= enemyBehindAngle)
			s += 15.0f;
		if (t.conf == ThreatConf::PossibleEngagement)
		{
			if (t.isZooming) s += 8.0f;
			if (t.isFiring) s += 8.0f;
		}
		if (t.id == selectedId)
			s += 12.0f;
		return s;
	}

	void StepAlert(Alert& a, bool enabled, bool inEnter, bool inStay, float dt, const char* text, int count, bool haveBearing, float bearing, uint64_t srcId)
	{
		if (!enabled)
		{
			ForceInactive(a);
			return;
		}

		if (haveBearing && std::isfinite(bearing))
		{
			a.lastBearingDeg = bearing;
			a.lastBearingValid = true;
			a.lastId = srcId;
		}

		if (text && text[0])
			SetText(a, text);
		a.count = count;

		const bool latched = (a.state == AlertState::Pending || a.state == AlertState::Visible);
		const bool condition = latched ? inStay : inEnter;

		switch (a.state)
		{
		case AlertState::Inactive:
			if (condition)
			{
				a.state = AlertState::Pending;
				a.pendingRemain = SampleDelay();
				a.clearHold = 0.0f;
				a.progress = 0.0f;
			}
			break;
		case AlertState::Pending:
			if (!condition)
			{
				ForceInactive(a);
				break;
			}
			a.pendingRemain -= dt;
			if (a.pendingRemain <= 0.0f)
			{
				a.state = AlertState::Visible;
				a.clearHold = 0.0f;
				a.progress = 1.0f;
			}
			break;
		case AlertState::Visible:
			if (condition)
			{
				a.clearHold = 0.0f;
				a.progress = 1.0f;
			}
			else
			{
				a.clearHold += dt;
				a.progress = 1.0f;
				if (a.clearHold >= kAlertClearHold)
				{
					a.state = AlertState::Fading;
					a.fadeTotal = std::clamp(warningFadeDuration, 0.05f, 2.0f);
					a.fadeRemain = a.fadeTotal;
				}
			}
			break;
		case AlertState::Fading:
			if (condition)
			{
				a.state = AlertState::Visible;
				a.clearHold = 0.0f;
				a.progress = 1.0f;
				break;
			}
			a.fadeRemain -= dt;
			a.progress = (a.fadeTotal > 0.0f) ? std::clamp(a.fadeRemain / a.fadeTotal, 0.0f, 1.0f) : 0.0f;
			if (a.fadeRemain <= 0.0f)
				ForceInactive(a);
			break;
		}
	}

	const char* ConfLabel(ThreatConf c)
	{
		switch (c)
		{
		case ThreatConf::ClosingFast: return "Closing fast";
		case ThreatConf::PossibleEngagement: return "Possible engagement";
		case ThreatConf::Approaching: return "Approaching";
		default: return "Nearby threat";
		}
	}

	void FormatPrimary(char* buf, size_t cap, const Threat& t, const char* dir)
	{
		const char* conf = ConfLabel(t.conf);
		if (warningShowDirectionText && dir && dir[0] && nearbyEnemyShowDistance)
			snprintf(buf, cap, XC("%s - %s - %dm"), conf, dir, (int)t.distM);
		else if (warningShowDirectionText && dir && dir[0])
			snprintf(buf, cap, XC("%s - %s"), conf, dir);
		else if (nearbyEnemyShowDistance)
			snprintf(buf, cap, XC("%s - %dm"), conf, (int)t.distM);
		else
			snprintf(buf, cap, "%s", conf);
	}

	void FormatMultiple(char* buf, size_t cap, int count, const char* dir)
	{
		if (warningShowDirectionText && dir && dir[0])
			snprintf(buf, cap, XC("%d threats - %s"), count, dir);
		else
			snprintf(buf, cap, XC("%d threats"), count);
	}

	ImU32 ToU32(const float c[4], float alphaMul = 1.0f)
	{
		const float a = std::clamp(c[3], 0.0f, 1.0f) * std::clamp(alphaMul, 0.0f, 1.0f);
		return IM_COL32(
			(int)(std::clamp(c[0], 0.0f, 1.0f) * 255.0f),
			(int)(std::clamp(c[1], 0.0f, 1.0f) * 255.0f),
			(int)(std::clamp(c[2], 0.0f, 1.0f) * 255.0f),
			(int)(a * 255.0f));
	}

	void Evaluate(std::vector<Threat>& threats, float dt, uint64_t nowMs)
	{
		EnsureInit();
		WarningSanitizeApproachSpeeds();
		PruneTracks(nowMs);

		const int minConf = std::clamp(warningMinConfidence, 1, 4);
		const int multiNeedConf = std::clamp(multipleMinConfidence, 1, 4);
		const int ways = (warningDirectionDetail == 0) ? 4 : 8;
		if (ways != g_latchedWays)
		{
			g_latchedSector = -1;
			g_latchedWays = ways;
		}

		int qualifyN = 0;
		int multiN = 0;
		int multiStayN = 0;
		int behindN = 0;
		float bestScore = -FLT_MAX;
		int bestIdx = -1;
		int selectedIdx = -1;
		int behindIdx = -1;
		float behindBest = -FLT_MAX;

		for (int i = 0; i < (int)threats.size(); ++i)
		{
			Threat& t = threats[i];
			Track* tr = FindTrack(t.id);
			if (!tr)
				tr = AllocTrack(t.id, nowMs);
			if (!tr)
				continue;
			if (tr->id != t.id)
			{
				*tr = Track{};
				tr->occupied = true;
				tr->id = t.id;
			}
			tr->lastMs = nowMs;
			UpdateTrackMotion(*tr, t, nowMs, dt);
			t.conf = Classify(t, *tr);

			if ((int)t.conf >= minConf)
			{
				++qualifyN;
				const float sc = ScoreThreat(t, g_selectedId);
				if (sc > bestScore)
				{
					bestScore = sc;
					bestIdx = i;
				}
				if (t.id == g_selectedId)
					selectedIdx = i;
			}
			if ((int)t.conf >= multiNeedConf && t.distM <= multipleEnemyDistance)
				++multiN;
			if ((int)t.conf >= multiNeedConf && t.distM <= multipleEnemyDistance * 1.12f)
				++multiStayN;
		}

		bool keepSelected = false;
		if (selectedIdx >= 0)
		{
			const float selScore = ScoreThreat(threats[selectedIdx], g_selectedId);
			const bool dwellOk = (nowMs >= g_selectedSinceMs) && (nowMs - g_selectedSinceMs) < kDwellMs;
			if (bestIdx < 0 || bestIdx == selectedIdx)
				keepSelected = true;
			else if (dwellOk)
				keepSelected = true;
			else if (bestScore <= selScore + 12.0f)
				keepSelected = true;
		}

		int useIdx = -1;
		if (keepSelected)
			useIdx = selectedIdx;
		else
			useIdx = bestIdx;

		if (useIdx >= 0)
		{
			const uint64_t nid = threats[useIdx].id;
			if (nid != g_selectedId)
			{
				g_selectedId = nid;
				g_selectedSinceMs = nowMs;
				g_latchedSector = -1;
			}
		}
		else
		{
			g_selectedId = 0;
			g_selectedSinceMs = 0;
			g_latchedSector = -1;
		}

		const char* dirName = "";
		char dirBuf[24] = {};
		if (useIdx >= 0 && warningShowDirectionText)
		{
			const float b = threats[useIdx].bearingDeg;
			const int raw = (ways == 4) ? Sector4(b) : Sector8(b);
			g_latchedSector = LatchSector(b, raw, g_latchedSector, ways);
			strncpy_s(dirBuf, (ways == 4) ? SectorName4(g_latchedSector) : SectorName8(g_latchedSector), _TRUNCATE);
			dirName = dirBuf;
		}

		if (useIdx >= 0)
		{
			const uint64_t sid = threats[useIdx].id;
			for (int i = 0; i < (int)threats.size(); ++i)
			{
				if (i == useIdx)
					continue;
				const Threat& t = threats[i];
				if ((int)t.conf < minConf)
					continue;
				if (t.distM > enemyBehindDistance)
					continue;
				if (fabsf(t.bearingDeg) < enemyBehindAngle)
					continue;
				if (t.id == sid)
					continue;
				++behindN;
				const float sc = ScoreThreat(t, 0);
				if (sc > behindBest)
				{
					behindBest = sc;
					behindIdx = i;
				}
			}
		}
		else
		{
			for (int i = 0; i < (int)threats.size(); ++i)
			{
				const Threat& t = threats[i];
				if ((int)t.conf < minConf)
					continue;
				if (t.distM > enemyBehindDistance)
					continue;
				if (fabsf(t.bearingDeg) < enemyBehindAngle)
					continue;
				++behindN;
				const float sc = ScoreThreat(t, 0);
				if (sc > behindBest)
				{
					behindBest = sc;
					behindIdx = i;
				}
			}
		}

		char multiText[96];
		const int multiNeed = std::clamp(multipleEnemyCount, 2, 6);
		const bool multiEnter = multiN >= multiNeed;
		const bool multiStay = multiStayN >= multiNeed;
		const char* multiDir = dirName;
		FormatMultiple(multiText, sizeof(multiText), (std::max)(multiN, multiNeed), multiDir);

		char primaryText[96];
		bool primaryEnter = false;
		float primaryBear = 0.0f;
		uint64_t primaryId = 0;
		if (useIdx >= 0)
		{
			FormatPrimary(primaryText, sizeof(primaryText), threats[useIdx], dirName);
			primaryEnter = true;
			primaryBear = threats[useIdx].bearingDeg;
			primaryId = threats[useIdx].id;
		}
		else
			primaryText[0] = '\0';

		StepAlert(g_alerts[(int)AlertId::Multiple], multipleEnemiesEnabled,
			multiEnter, multiStay, dt, multiText, multiN,
			useIdx >= 0, primaryBear, primaryId);

		const bool suppressNearby = (g_alerts[(int)AlertId::Multiple].state == AlertState::Visible);
		if (suppressNearby)
			ForceInactive(g_alerts[(int)AlertId::Nearby]);
		else
			StepAlert(g_alerts[(int)AlertId::Nearby], true,
				primaryEnter, primaryEnter, dt, primaryText, qualifyN,
				primaryEnter, primaryBear, primaryId);

		char behindText[96];
		bool behindEnter = false;
		float behindBear = 0.0f;
		uint64_t behindId = 0;
		if (enemyBehindEnabled && behindIdx >= 0)
		{
			const Threat& bt = threats[behindIdx];
			const int rawB = (ways == 4) ? Sector4(bt.bearingDeg) : Sector8(bt.bearingDeg);
			const char* bdir = (ways == 4) ? SectorName4(rawB) : SectorName8(rawB);
			FormatPrimary(behindText, sizeof(behindText), bt, bdir);
			behindEnter = true;
			behindBear = bt.bearingDeg;
			behindId = bt.id;
		}
		else
			behindText[0] = '\0';

		StepAlert(g_alerts[(int)AlertId::Behind], enemyBehindEnabled,
			behindEnter, behindEnter, dt, behindText, behindN,
			behindEnter, behindBear, behindId);

		g_arrow = ArrowDraw{};
		if (directionIndicatorEnabled)
		{
			const Alert* pick = nullptr;
			const Alert& behindA = g_alerts[(int)AlertId::Behind];
			const Alert& multiA = g_alerts[(int)AlertId::Multiple];
			const Alert& nearbyA = g_alerts[(int)AlertId::Nearby];
			if ((nearbyA.state == AlertState::Visible || nearbyA.state == AlertState::Fading) && nearbyA.lastBearingValid)
				pick = &nearbyA;
			else if (multipleEnemiesEnabled && (multiA.state == AlertState::Visible || multiA.state == AlertState::Fading) && multiA.lastBearingValid)
				pick = &multiA;
			else if (enemyBehindEnabled && (behindA.state == AlertState::Visible || behindA.state == AlertState::Fading) && behindA.lastBearingValid)
				pick = &behindA;

			if (pick)
			{
				g_arrow.valid = std::isfinite(pick->lastBearingDeg);
				g_arrow.bearingDeg = pick->lastBearingDeg;
				if (pick->state == AlertState::Fading && pick->fadeTotal > 0.0f)
					g_arrow.alpha = std::clamp(pick->fadeRemain / pick->fadeTotal, 0.0f, 1.0f);
				else
					g_arrow.alpha = 1.0f;
			}
		}
		(void)qualifyN;
	}

	void DrawBanners(ImDrawList* dl, float vw, float vh)
	{
		const float scale = std::clamp(warningInterfaceScale, 0.50f, 2.00f);
		ImFont* font = (f.custom.size() > 0) ? f.custom[0] : ImGui::GetFont();
		if (!font)
			return;

		const float fs = font->FontSize * scale;
		const float padX = 12.0f * scale;
		const float padY = 8.0f * scale;
		const float barH = 42.0f * scale;
		const float gap = 6.0f * scale;
		const float accentW = 3.0f * scale;
		const float rounding = 5.0f;
		const int maxVis = std::clamp(warningMaxVisible, 1, 5);

		const AlertId order[3] = { AlertId::Behind, AlertId::Multiple, AlertId::Nearby };
		int drawn = 0;
		float y = 48.0f * scale;

		for (int i = 0; i < 3 && drawn < maxVis; ++i)
		{
			const Alert& a = g_alerts[(int)order[i]];
			if (a.state != AlertState::Visible && a.state != AlertState::Fading)
				continue;
			if (a.text[0] == '\0')
				continue;

			const float fadeA = (a.state == AlertState::Fading && a.fadeTotal > 0.0f)
				? std::clamp(a.fadeRemain / a.fadeTotal, 0.0f, 1.0f)
				: 1.0f;
			if (fadeA <= 0.0f)
				continue;

			ImVec2 ts = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, a.text);
			float w = ts.x + padX * 2.0f + accentW + 8.0f * scale;
			w = std::clamp(w, 200.0f * scale, 520.0f * scale);
			const float h = barH;
			const float x = (vw - w) * 0.5f;
			if (!std::isfinite(x) || !std::isfinite(y))
				continue;

			const ImVec2 p1(x, y);
			const ImVec2 p2(x + w, y + h);

			dl->AddRectFilled(p1, p2, ToU32(col_warning_bg, fadeA), rounding);
			dl->AddRect(p1, p2, IM_COL32(255, 255, 255, (int)(18 * fadeA)), rounding);
			dl->AddRectFilled(ImVec2(p1.x, p1.y + 4.0f * scale), ImVec2(p1.x + accentW, p2.y - 4.0f * scale),
				ToU32(col_warning_accent, fadeA), 2.0f);

			const float tx = p1.x + accentW + padX;
			const float ty = p1.y + (h - ts.y) * 0.5f - (warningProgressBar ? 2.0f * scale : 0.0f);
			dl->AddText(font, fs, ImVec2(tx + 1.0f, ty + 1.0f), IM_COL32(0, 0, 0, (int)(180 * fadeA)), a.text);
			dl->AddText(font, fs, ImVec2(tx, ty), ToU32(col_warning_text, fadeA), a.text);

			if (warningProgressBar)
			{
				const float p = std::clamp(a.progress, 0.0f, 1.0f);
				const float ph = 3.0f * scale;
				const float px0 = p1.x + accentW + 6.0f * scale;
				const float px1 = p2.x - 6.0f * scale;
				const float py = p2.y - padY * 0.55f;
				dl->AddRectFilled(ImVec2(px0, py), ImVec2(px1, py + ph), IM_COL32(255, 255, 255, (int)(22 * fadeA)), 1.0f);
				if (p > 0.0f && std::isfinite(p))
					dl->AddRectFilled(ImVec2(px0, py), ImVec2(px0 + (px1 - px0) * p, py + ph), ToU32(col_warning_accent, fadeA), 1.0f);
			}

			y += h + gap;
			++drawn;
			(void)vh;
		}
	}

	void DrawArrow(ImDrawList* dl, float vw, float vh)
	{
		if (!g_arrow.valid || !directionIndicatorEnabled)
			return;
		if (!std::isfinite(g_arrow.bearingDeg))
			return;

		const float iface = std::clamp(warningInterfaceScale, 0.50f, 2.00f);
		const float dirSc = std::clamp(directionIndicatorScale, 0.50f, 2.00f);
		const float ring = kRingPx * iface * dirSc;
		if (!std::isfinite(ring) || ring < 8.0f)
			return;

		const ImVec2 center(vw * 0.5f, vh * 0.5f);
		if (!std::isfinite(center.x) || !std::isfinite(center.y))
			return;

		DrawEnemyArrows(dl, g_arrow.bearingDeg, center, ring,
			ToU32(col_warning_accent, g_arrow.alpha), dirSc * iface, directionIndicatorPulse);
	}
}

void Alerts::Reset()
{
	EnsureInit();
	ForceInactive(g_alerts[(int)AlertId::Nearby]);
	ForceInactive(g_alerts[(int)AlertId::Multiple]);
	ForceInactive(g_alerts[(int)AlertId::Behind]);
	g_arrow = ArrowDraw{};
	ClearTracks();
}

void Alerts::Tick(ImDrawList* dl, float deltaTime)
{
	EnsureInit();

	if (!warningsEnabled || InterlockedCompareExchange(&g_coreReady, 0, 0) == 0 ||
		!NT::Store::Entity || !NT::Store::Scene)
	{
		Reset();
		return;
	}

	if (NT::Store::Entity->GetRaidState() != NT::Store::RaidState::InRaid)
	{
		Reset();
		return;
	}

	const CachedCameraData cam = NT::Store::Entity->GetCameraSnapshot();
	const uint64_t nowMs = GetNowMs();
	if (!cam.valid ||
		!LooksValidWorldPos(cam.location) ||
		!std::isfinite(cam.rotation.Yaw) ||
		!std::isfinite(cam.rotation.Pitch) ||
		cam.lastUpdateMs == 0 ||
		nowMs < cam.lastUpdateMs ||
		(nowMs - cam.lastUpdateMs) > kCameraStaleMs)
	{
		Reset();
		return;
	}

	static std::vector<Threat> threats;
	threats.clear();
	{
		BufGuard guard;
		guard.buf = NT::Store::Scene->AcquirePlayerBuffer();
		if (!guard.buf)
		{
			Reset();
			return;
		}
		CollectThreats(guard.buf->data, cam, nowMs, threats);
	}

	Evaluate(threats, SanitizeDt(deltaTime), nowMs);

	if (!dl)
		return;
	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float vw = display.x;
	const float vh = display.y;
	if (vw < 64.0f || vh < 64.0f)
		return;

	DrawBanners(dl, vw, vh);
	DrawArrow(dl, vw, vh);
}
