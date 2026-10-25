#pragma once

#include "config/build_config.hpp"

#ifdef DEBUG_CONSOLE

#include "CacheTypes.hpp"
#include "memory/driver_types.hpp"
#include "memory/interface/ctx.hpp"
#include "sdk/SDK.hpp"
#include "settings/Settings.hpp"

#include <Windows.h>

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <cstring>

// Debug-only raid-startup + PCM/ViewTarget observer. Release compilation is empty.
namespace EspReadyDiag {

	inline std::atomic<uint64_t> g_gen{ 0 };
	inline std::atomic<uint64_t> g_raidStartMs{ 0 };
	inline std::atomic<uint64_t> g_firstSnapshotMs{ 0 };
	inline std::atomic<uint64_t> g_firstPcmMs{ 0 };
	inline std::atomic<uint64_t> g_firstCameraMs{ 0 };
	inline std::atomic<uint64_t> g_firstDrawMs{ 0 };

	inline std::atomic<uint64_t> g_lastPeriodicMs{ 0 };
	inline std::atomic<int> g_finished{ 0 };
	inline std::atomic<int> g_loggedRaid{ 0 };
	inline std::atomic<int> g_loggedSnapshot{ 0 };
	inline std::atomic<int> g_loggedPcm{ 0 };
	inline std::atomic<int> g_loggedCamera{ 0 };
	inline std::atomic<int> g_loggedDraw{ 0 };

	inline uint64_t NowMs()
	{
		return GetTickCount64();
	}

	inline void LogLine(const char* line)
	{
		DebugSystem::LogConsole(line);
	}

	inline void LogTrans(const char* name)
	{
		char line[96];
		snprintf(line, sizeof(line), "[EspReady] trans=%s", name);
		LogLine(line);
	}

	inline bool NoteAbs(std::atomic<uint64_t>& slot, std::atomic<int>& logged, const char* trans)
	{
		uint64_t expected = 0;
		const uint64_t now = NowMs();
		if (!slot.compare_exchange_strong(expected, now, std::memory_order_relaxed))
			return false;
		int loggedExpected = 0;
		if (logged.compare_exchange_strong(loggedExpected, 1, std::memory_order_relaxed))
			LogTrans(trans);
		return true;
	}

	inline void ResetRaidMilestones()
	{
		g_raidStartMs.store(0, std::memory_order_relaxed);
		g_firstSnapshotMs.store(0, std::memory_order_relaxed);
		g_firstPcmMs.store(0, std::memory_order_relaxed);
		g_firstCameraMs.store(0, std::memory_order_relaxed);
		g_firstDrawMs.store(0, std::memory_order_relaxed);
		g_lastPeriodicMs.store(0, std::memory_order_relaxed);
		g_finished.store(0, std::memory_order_relaxed);
		g_loggedRaid.store(0, std::memory_order_relaxed);
		g_loggedSnapshot.store(0, std::memory_order_relaxed);
		g_loggedPcm.store(0, std::memory_order_relaxed);
		g_loggedCamera.store(0, std::memory_order_relaxed);
		g_loggedDraw.store(0, std::memory_order_relaxed);
	}

	inline void Reset()
	{
		g_gen.store(0, std::memory_order_relaxed);
		ResetRaidMilestones();
	}

	inline void OnGeneration(uint64_t uworld)
	{
		const uint64_t prev = g_gen.load(std::memory_order_relaxed);
		if (prev == uworld)
			return;
		ResetRaidMilestones();
		g_gen.store(uworld, std::memory_order_relaxed);
	}

	inline void NoteRaidStart()
	{
		NoteAbs(g_raidStartMs, g_loggedRaid, "raid");
	}

	inline void NoteSnapshot() { NoteAbs(g_firstSnapshotMs, g_loggedSnapshot, "snapshot"); }
	inline void NotePcmCached() { NoteAbs(g_firstPcmMs, g_loggedPcm, "pcmCached"); }
	inline void NoteCamera() { NoteAbs(g_firstCameraMs, g_loggedCamera, "camera"); }
	inline void NoteDraw() { NoteAbs(g_firstDrawMs, g_loggedDraw, "draw"); }

	inline void FmtMs(char* buf, size_t n, uint64_t abs, uint64_t origin)
	{
		if (!abs || !origin) {
			snprintf(buf, n, "NA");
			return;
		}
		const uint64_t d = (abs >= origin) ? (abs - origin) : 0;
		snprintf(buf, n, "%llu", (unsigned long long)d);
	}

	struct Frame {
		bool inRaid = false;
		bool world = false;
		bool pcmCached = false;
		bool povValid = false;
		bool drewPlayerEsp = false;
	};

	inline const char* Blocker(const Frame& f)
	{
		if (!f.world)
			return "noWorld";
		if (!f.pcmCached)
			return "noPcm";
		if (!f.povValid)
			return "pcmNoPov";
		return "ready";
	}

	inline void NoteMilestones(const Frame& f)
	{
		if (!f.inRaid)
			return;
		NoteRaidStart();
		if (f.pcmCached)
			NotePcmCached();
		if (f.povValid)
			NoteCamera();
		if (f.drewPlayerEsp)
			NoteDraw();
	}

	inline bool WantsPublish(const Frame& f)
	{
		if (!f.inRaid || g_finished.load(std::memory_order_relaxed))
			return false;
		if (f.drewPlayerEsp && strcmp(Blocker(f), "ready") == 0)
			return true;
		const uint64_t now = NowMs();
		const uint64_t last = g_lastPeriodicMs.load(std::memory_order_relaxed);
		return last == 0 || (now - last) >= 1000;
	}

	inline void Emit(const Frame& f, const char* blocker)
	{
		const uint64_t origin = g_raidStartMs.load(std::memory_order_relaxed);
		char firstSnap[16], firstPcm[16], firstCam[16], firstDraw[16];
		FmtMs(firstSnap, sizeof(firstSnap), g_firstSnapshotMs.load(std::memory_order_relaxed), origin);
		FmtMs(firstPcm, sizeof(firstPcm), g_firstPcmMs.load(std::memory_order_relaxed), origin);
		FmtMs(firstCam, sizeof(firstCam), g_firstCameraMs.load(std::memory_order_relaxed), origin);
		FmtMs(firstDraw, sizeof(firstDraw), g_firstDrawMs.load(std::memory_order_relaxed), origin);

		char line[320];
		snprintf(line, sizeof(line),
			"[EspReady] firstSnapshotMs=%s firstPcmMs=%s firstCameraMs=%s firstDrawMs=%s blocker=%s",
			firstSnap, firstPcm, firstCam, firstDraw, blocker);
		LogLine(line);
	}

	inline void Publish(const Frame& f)
	{
		if (!f.inRaid || g_finished.load(std::memory_order_relaxed))
			return;

		const char* blocker = Blocker(f);
		const bool ready = f.drewPlayerEsp && strcmp(blocker, "ready") == 0;
		if (ready) {
			Emit(f, "ready");
			g_finished.store(1, std::memory_order_relaxed);
			return;
		}

		const uint64_t now = NowMs();
		g_lastPeriodicMs.store(now, std::memory_order_relaxed);
		Emit(f, blocker);
	}

}

namespace CamBindDiag {

	enum class Source : uint8_t {
		None = 0,
		PcField,
		Name,
		Shape,
		Field,
		StaticName,
		DrawPcField,
		Cached,
		Fallback
	};

	inline const char* SourceName(Source s)
	{
		switch (s) {
		case Source::PcField: return "PC_FIELD";
		case Source::Name: return "NAME";
		case Source::Shape: return "SHAPE";
		case Source::Field: return "FIELD";
		case Source::StaticName: return "STATIC_NAME";
		case Source::DrawPcField: return "DRAW_PC_FIELD";
		case Source::Cached: return "CACHED";
		case Source::Fallback: return "FALLBACK";
		default: return "NONE";
		}
	}

	inline const char* RejectName(CamReadDiag::Reject r)
	{
		using R = CamReadDiag::Reject;
		switch (r) {
		case R::None: return "None";
		case R::NoWorld: return "NoWorld";
		case R::NoLocalController: return "NoLocalController";
		case R::NoPcm: return "NoPcm";
		case R::NoViewTarget: return "NoViewTarget";
		case R::PovReadFailed: return "PovReadFailed";
		case R::PovPartialRead: return "PovPartialRead";
		case R::LocationNonFinite: return "LocationNonFinite";
		case R::LocationZero: return "LocationZero";
		case R::RotationNonFinite: return "RotationNonFinite";
		case R::RotationDegenerate: return "RotationDegenerate";
		case R::FovNonFinite: return "FovNonFinite";
		case R::FovOutOfRange: return "FovOutOfRange";
		case R::PoseJump: return "PoseJump";
		case R::FovJump: return "FovJump";
		case R::LastGoodExpired: return "LastGoodExpired";
		default: return "Unknown";
		}
	}

	inline thread_local Source t_pending = Source::None;

	inline std::atomic<uint32_t> g_worldGen{ 0 };
	inline std::atomic<uint32_t> g_pcGen{ 0 };
	inline std::atomic<uint32_t> g_pawnGen{ 0 };
	inline std::atomic<uint32_t> g_pcmGen{ 0 };

	inline std::atomic<uint32_t> g_pcmSetWorldGen{ 0 };
	inline std::atomic<int> g_pcmSetPcGen{ -1 };
	inline std::atomic<uint8_t> g_lastSource{ 0 };

	inline std::atomic<uintptr_t> g_lastWorld{ 0 };
	inline std::atomic<uintptr_t> g_lastPc{ 0 };
	inline std::atomic<uintptr_t> g_lastPawn{ 0 };
	inline std::atomic<uintptr_t> g_lastPcm{ 0 };

	inline std::atomic<int> g_consecutiveRejects{ 0 };
	inline std::atomic<int> g_lastAccepted{ 0 };
	inline std::atomic<uint8_t> g_lastReject{ 0 };
	inline std::atomic<uint64_t> g_lastPeriodicMs{ 0 };
	inline std::atomic<int> g_lastPovValid{ 0 };

	inline std::atomic<uint32_t> g_lastLogWorldGen{ 0 };
	inline std::atomic<uint32_t> g_lastLogPcGen{ 0 };
	inline std::atomic<uint32_t> g_lastLogPawnGen{ 0 };
	inline std::atomic<uint32_t> g_lastLogPcmGen{ 0 };

	inline void BeginSet(Source s)
	{
		t_pending = s;
	}

	inline void ResetGens()
	{
		g_worldGen.store(0, std::memory_order_relaxed);
		g_pcGen.store(0, std::memory_order_relaxed);
		g_pawnGen.store(0, std::memory_order_relaxed);
		g_pcmGen.store(0, std::memory_order_relaxed);
		g_pcmSetWorldGen.store(0, std::memory_order_relaxed);
		g_pcmSetPcGen.store(-1, std::memory_order_relaxed);
		g_lastSource.store(0, std::memory_order_relaxed);
		g_lastWorld.store(0, std::memory_order_relaxed);
		g_lastPc.store(0, std::memory_order_relaxed);
		g_lastPawn.store(0, std::memory_order_relaxed);
		g_lastPcm.store(0, std::memory_order_relaxed);
		g_consecutiveRejects.store(0, std::memory_order_relaxed);
		g_lastAccepted.store(0, std::memory_order_relaxed);
		g_lastReject.store(0, std::memory_order_relaxed);
		g_lastPeriodicMs.store(0, std::memory_order_relaxed);
		g_lastPovValid.store(0, std::memory_order_relaxed);
		g_lastLogWorldGen.store(0, std::memory_order_relaxed);
		g_lastLogPcGen.store(0, std::memory_order_relaxed);
		g_lastLogPawnGen.store(0, std::memory_order_relaxed);
		g_lastLogPcmGen.store(0, std::memory_order_relaxed);
		t_pending = Source::None;
	}

	inline uintptr_t NormPtr(uintptr_t p)
	{
		return is_valid_pointer_optimized(p) ? p : 0;
	}

	inline bool LocInWorld(const Vector3& loc)
	{
		if (!std::isfinite(loc.x) || !std::isfinite(loc.y) || !std::isfinite(loc.z))
			return false;
		if (loc.x == 0.0 && loc.y == 0.0 && loc.z == 0.0)
			return false;
		const double mag2 = loc.x * loc.x + loc.y * loc.y + loc.z * loc.z;
		return mag2 > 1.0e6 && mag2 < 1.0e16;
	}

	// Same live gates Loop would use without last-good restore. Does not write CamLastGood.
	inline CamReadDiag::Reject ClassifyLivePov(const Vector3& loc, const FRotator& rot, float fov)
	{
		using R = CamReadDiag::Reject;
		if (!std::isfinite(loc.x) || !std::isfinite(loc.y) || !std::isfinite(loc.z))
			return R::LocationNonFinite;
		if (loc.x == 0.0f && loc.y == 0.0f && loc.z == 0.0f)
			return R::LocationZero;
		if (!std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll))
			return R::RotationNonFinite;
		if (rot.Pitch == 0.0 && rot.Yaw == 0.0 && !LocInWorld(loc))
			return R::RotationDegenerate;
		if (PcmPovLooksLikeDummyLens(loc, rot))
			return R::RotationDegenerate;
		if (!std::isfinite(fov))
			return R::FovNonFinite;
		if (fov < 1.0f || fov > 170.0f)
			return R::FovOutOfRange;
		return R::None;
	}

	inline void FmtIntOrNA(char* buf, size_t n, int v, bool na)
	{
		if (na)
			snprintf(buf, n, "NA");
		else
			snprintf(buf, n, "%d", v);
	}

	inline void OnWorldIdentity(uintptr_t uworld)
	{
		const uintptr_t id = NormPtr(uworld);
		const uintptr_t prev = g_lastWorld.exchange(id, std::memory_order_relaxed);
		if (prev == id)
			return;
		g_worldGen.fetch_add(1, std::memory_order_relaxed);
	}

	inline void OnPcPublished(uintptr_t pc)
	{
		const uintptr_t nowId = NormPtr(pc);
		const uintptr_t prevId = g_lastPc.exchange(nowId, std::memory_order_relaxed);
		if (prevId == nowId)
			return;

		const uint32_t pcGen = g_pcGen.fetch_add(1, std::memory_order_relaxed) + 1;
		const char* trans = (!prevId && nowId) ? "acquired" : ((prevId && !nowId) ? "lost" : "changed");
		const int pcmCached = NormPtr(g_lastPcm.load(std::memory_order_relaxed)) ? 1 : 0;
		const int setPcGen = g_pcmSetPcGen.load(std::memory_order_relaxed);
		const int povValid = g_lastPovValid.load(std::memory_order_relaxed);
		const auto reject = (CamReadDiag::Reject)g_lastReject.load(std::memory_order_relaxed);

		char setPc[16];
		FmtIntOrNA(setPc, sizeof(setPc), setPcGen, setPcGen < 0);

		char line[320];
		snprintf(line, sizeof(line),
			"[CamPcTransition] pcGen=%u transition=%s pcmCached=%d pcmSetPcGen=%s povValid=%d reject=%s",
			pcGen, trans, pcmCached, setPc, povValid, RejectName(reject));
		EspReadyDiag::LogLine(line);
	}

	inline void OnPawnPublished(uintptr_t pawn)
	{
		const uintptr_t nowId = NormPtr(pawn);
		const uintptr_t prevId = g_lastPawn.exchange(nowId, std::memory_order_relaxed);
		if (prevId == nowId)
			return;
		g_pawnGen.fetch_add(1, std::memory_order_relaxed);
	}

	inline void OnPcmStore(uintptr_t oldPcm, uintptr_t newPcm)
	{
		const uintptr_t oldId = NormPtr(oldPcm);
		const uintptr_t newId = NormPtr(newPcm);
		if (oldId == newId) {
			t_pending = Source::None;
			return;
		}

		Source src = t_pending;
		t_pending = Source::None;
		if (newId == 0)
			src = Source::None;
		else if (src == Source::None)
			src = Source::Cached;

		const uint32_t worldGen = g_worldGen.load(std::memory_order_relaxed);
		const uint32_t pcGen = g_pcGen.load(std::memory_order_relaxed);
		const uint32_t pcmGen = g_pcmGen.fetch_add(1, std::memory_order_relaxed) + 1;
		g_lastPcm.store(newId, std::memory_order_relaxed);
		g_lastSource.store((uint8_t)src, std::memory_order_relaxed);

		int ownerMatch = -1;
		int vtPresent = -1;
		int vtMatches = -1;
		int completePov = 0;
		auto reason = CamReadDiag::Reject::NoPcm;

		if (newId && ctx) {
			g_pcmSetWorldGen.store(worldGen, std::memory_order_relaxed);
			g_pcmSetPcGen.store((int)pcGen, std::memory_order_relaxed);
			EspReadyDiag::NotePcmCached();

			const auto& off = Offsets::Get();
			const uintptr_t pov = newId + off.CameraManager.ViewTarget + off.FTViewTarget.POV;
			const Vector3 loc = ctx->read<Vector3>(pov + off.FMinimalViewInfo.Location);
			const FRotator rot = ctx->read<FRotator>(pov + off.FMinimalViewInfo.Rotation);
			const float fov = ctx->read<float>(pov + off.FMinimalViewInfo.FOV);
			reason = ClassifyLivePov(loc, rot, fov);
			completePov = (reason == CamReadDiag::Reject::None) ? 1 : 0;

			const uintptr_t target = ctx->read<uintptr_t>(newId + off.CameraManager.ViewTarget);
			vtPresent = is_valid_pointer_optimized(target) ? 1 : 0;

			const uintptr_t localPc = NormPtr(g_lastPc.load(std::memory_order_relaxed));
			const uintptr_t localPawn = NormPtr(g_lastPawn.load(std::memory_order_relaxed));
			if (localPc) {
				const uintptr_t owner = ctx->read<uintptr_t>(newId + off.CameraManager.PCOwner);
				ownerMatch = (owner == localPc) ? 1 : 0;
			}
			if (vtPresent == 1)
				vtMatches = (localPawn && target == localPawn) ? 1 : (localPawn ? 0 : -1);
		}

		char ownerBuf[8], vtBuf[8], matchBuf[8];
		FmtIntOrNA(ownerBuf, sizeof(ownerBuf), ownerMatch, ownerMatch < 0);
		FmtIntOrNA(vtBuf, sizeof(vtBuf), vtPresent, vtPresent < 0);
		FmtIntOrNA(matchBuf, sizeof(matchBuf), vtMatches, vtMatches < 0);

		char line[384];
		snprintf(line, sizeof(line),
			"[CamBindSet] worldGen=%u pcGen=%u pcmGen=%u source=%s ownerMatch=%s "
			"viewTargetPresent=%s viewTargetMatchesPawn=%s completePovValidAtSet=%d reasonAccepted=%s",
			worldGen, pcGen, pcmGen, SourceName(src),
			ownerBuf, vtBuf, matchBuf, completePov, RejectName(reason));
		EspReadyDiag::LogLine(line);
	}

	struct Frame {
		bool world = false;
		bool localPc = false;
		bool localPawn = false;
		bool pcmCached = false;
		bool cameraAccepted = false;
		bool lastGoodUsed = false;
		bool usedFallback = false;
		bool viewTargetKnown = false;
		int viewTargetPresent = -1;
		int viewTargetMatchesPawn = -1;
		uint64_t lastGoodAgeMs = ~0ull;
		CamReadDiag::Reject loopReject = CamReadDiag::Reject::Unknown;
		bool loopRejectOverride = false;
	};

	inline void Publish(const Frame& f)
	{
		using R = CamReadDiag::Reject;
		const uint32_t worldGen = g_worldGen.load(std::memory_order_relaxed);
		const uint32_t pcGen = g_pcGen.load(std::memory_order_relaxed);
		const uint32_t pawnGen = g_pawnGen.load(std::memory_order_relaxed);
		const uint32_t pcmGen = g_pcmGen.load(std::memory_order_relaxed);

		Source src = (Source)g_lastSource.load(std::memory_order_relaxed);
		if (f.usedFallback || f.lastGoodUsed)
			src = Source::Fallback;
		else if (!f.pcmCached && src != Source::DrawPcField)
			src = Source::None;

		const CamReadDiag::Snap& snap = CamReadDiag::g_last;
		R reject = f.loopRejectOverride ? f.loopReject : snap.reject;
		if (f.cameraAccepted && !f.lastGoodUsed && !f.usedFallback)
			reject = R::None;
		if (!f.world)
			reject = R::NoWorld;
		else if (!f.pcmCached && src == Source::None && !f.usedFallback)
			reject = R::NoPcm;

		const int prevAccepted = g_lastAccepted.load(std::memory_order_relaxed);
		if (f.cameraAccepted)
			g_consecutiveRejects.store(0, std::memory_order_relaxed);
		else
			g_consecutiveRejects.fetch_add(1, std::memory_order_relaxed);

		const int accepted = f.cameraAccepted ? 1 : 0;
		g_lastAccepted.store(accepted, std::memory_order_relaxed);
		g_lastReject.store((uint8_t)reject, std::memory_order_relaxed);
		g_lastPovValid.store(accepted, std::memory_order_relaxed);

		const bool genChanged =
			g_lastLogWorldGen.exchange(worldGen, std::memory_order_relaxed) != worldGen ||
			g_lastLogPcGen.exchange(pcGen, std::memory_order_relaxed) != pcGen ||
			g_lastLogPawnGen.exchange(pawnGen, std::memory_order_relaxed) != pawnGen ||
			g_lastLogPcmGen.exchange(pcmGen, std::memory_order_relaxed) != pcmGen;

		const uint64_t now = EspReadyDiag::NowMs();
		const uint64_t last = g_lastPeriodicMs.load(std::memory_order_relaxed);
		const bool firstAccept = accepted && prevAccepted == 0;
		const bool periodic = !accepted && (last == 0 || (now - last) >= 1000);
		if (!genChanged && !periodic && !firstAccept)
			return;
		g_lastPeriodicMs.store(now, std::memory_order_relaxed);

		const int pcmSetWorld = (int)g_pcmSetWorldGen.load(std::memory_order_relaxed);
		const int pcmSetPc = g_pcmSetPcGen.load(std::memory_order_relaxed);
		const int pcmSetThisWorld = (f.pcmCached && pcmSetWorld > 0 && worldGen == (uint32_t)pcmSetWorld) ? 1 : 0;
		const int pcmSetThisPc = (f.pcmCached && pcmSetPc >= 0 && pcGen == (uint32_t)pcmSetPc) ? 1 : 0;
		const int pcChangedSinceSet = (f.pcmCached && pcmSetPc >= 0 && pcGen != (uint32_t)pcmSetPc) ? 1 : 0;

		char vtPres[8], vtMatch[8], povRead[8];
		char locFin[8], locZero[8], rotFin[8], rotDeg[8], fovFin[8], fovOk[8];
		char ageBuf[16];

		const bool havePov = f.pcmCached || f.usedFallback || snap.sampled;
		if (!havePov) {
			snprintf(vtPres, sizeof(vtPres), "NA");
			snprintf(vtMatch, sizeof(vtMatch), "NA");
			snprintf(povRead, sizeof(povRead), "NA");
			snprintf(locFin, sizeof(locFin), "NA");
			snprintf(locZero, sizeof(locZero), "NA");
			snprintf(rotFin, sizeof(rotFin), "NA");
			snprintf(rotDeg, sizeof(rotDeg), "NA");
			snprintf(fovFin, sizeof(fovFin), "NA");
			snprintf(fovOk, sizeof(fovOk), "NA");
		} else {
			if (f.viewTargetKnown)
				FmtIntOrNA(vtPres, sizeof(vtPres), f.viewTargetPresent, f.viewTargetPresent < 0);
			else
				snprintf(vtPres, sizeof(vtPres), "NA");
			FmtIntOrNA(vtMatch, sizeof(vtMatch), f.viewTargetMatchesPawn, f.viewTargetMatchesPawn < 0);
			snprintf(povRead, sizeof(povRead), snap.sampled ? "OK" : "FAIL");
			FmtIntOrNA(locFin, sizeof(locFin), snap.locFinite ? 1 : 0, false);
			FmtIntOrNA(locZero, sizeof(locZero), snap.locZero ? 1 : 0, false);
			FmtIntOrNA(rotFin, sizeof(rotFin), snap.rotFinite ? 1 : 0, false);
			FmtIntOrNA(rotDeg, sizeof(rotDeg), snap.rotDegenerate ? 1 : 0, false);
			FmtIntOrNA(fovFin, sizeof(fovFin), snap.fovFinite ? 1 : 0, false);
			FmtIntOrNA(fovOk, sizeof(fovOk), snap.fovPlausible ? 1 : 0, false);
		}

		if (f.lastGoodAgeMs == ~0ull)
			snprintf(ageBuf, sizeof(ageBuf), "NA");
		else
			snprintf(ageBuf, sizeof(ageBuf), "%llu", (unsigned long long)f.lastGoodAgeMs);

		char line[768];
		snprintf(line, sizeof(line),
			"[CamBind] worldGen=%u pcGen=%u pawnGen=%u pcmGen=%u source=%s "
			"world=%d localPc=%d localPawn=%d pcmCached=%d pcmSetThisWorld=%d pcmSetThisPc=%d "
			"pcChangedSinceSet=%d viewTargetPresent=%s viewTargetMatchesPawn=%s povRead=%s "
			"locFinite=%s locZero=%s rotFinite=%s rotDegenerate=%s fovFinite=%s fovPlausible=%s "
			"lastGoodUsed=%d lastGoodAgeMs=%s consecutiveRejects=%d reject=%s cameraAccepted=%d",
			worldGen, pcGen, pawnGen, pcmGen, SourceName(src),
			f.world ? 1 : 0, f.localPc ? 1 : 0, f.localPawn ? 1 : 0, f.pcmCached ? 1 : 0,
			pcmSetThisWorld, pcmSetThisPc, pcChangedSinceSet,
			vtPres, vtMatch, povRead,
			locFin, locZero, rotFin, rotDeg, fovFin, fovOk,
			(f.lastGoodUsed || f.usedFallback) ? 1 : 0, ageBuf,
			g_consecutiveRejects.load(std::memory_order_relaxed),
			RejectName(reject), accepted);
		EspReadyDiag::LogLine(line);
	}

}

#endif
