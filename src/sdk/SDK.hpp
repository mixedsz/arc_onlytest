#pragma once
#include "config/build_config.hpp"
#include "Decryptions.hpp"
#include "Math.hpp"
#include <unordered_map>
#include <vector>
#include <chrono>
#include <d3d11.h>
#include "imgui.h"
#include <mutex>
#include <float.h>
#include <cmath>
#include <cctype>
#include <cstring>
#include <iostream>
#include <string>
#include <cstdint>

struct PointerCache;

#ifndef IS_VALID_POINTER_OPTIMIZED_DEFINED
#define IS_VALID_POINTER_OPTIMIZED_DEFINED
inline bool is_valid_pointer_optimized(uint64_t ptr) {
	if (!ptr) return false;
	// Reject unaligned / tagged garbage (e.g. 0x200400031901, 0x1506...0709).
	if (ptr & 0x7ULL) return false;
	if (ptr < 0x10000ULL || ptr > 0x00007FFFFFFFFFFFULL) return false;
	return true;
}
#endif

// PlayerArray "weak" PCs like 0x5FD31590 pass the 0x10000 floor but are not
// UObjects. Live Pioneer pointers in this build sit at 0x11xxxxxxxx+.
inline bool LooksLikeCanonicalUObjectPtr(uint64_t ptr) {
	return is_valid_pointer_optimized(ptr) && ptr >= 0x100000000ULL;
}

typedef struct _TArray {
	uint64_t Data;
	int32_t Count;
	int32_t Max;
} TArray;

inline bool is_valid_tarray(const TArray& arr, int32_t maxSaneCount = 65536) {
	if (!is_valid_pointer_optimized(arr.Data))
		return false;
	if (arr.Count <= 0 || arr.Count > maxSaneCount)
		return false;
	if (arr.Max < arr.Count)
		return false;
	return true;
}

struct MatrixD
{
	double M11, M12, M13, M14;
	double M21, M22, M23, M24;
	double M31, M32, M33, M34;
	double M41, M42, M43, M44;
};

// Projection space for every world_to_screen and screen-space cull. Seeded from the
// primary monitor only as a pre-window placeholder — SM_CXSCREEN is the wrong answer
// whenever the game is windowed, on a second monitor, at a non-native resolution, or
// under display scaling, and it is sampled once at static init so it never follows a
// resolution change. SyncViewportSize() replaces it each frame with the size of the
// surface actually being drawn into.
inline int Width = GetSystemMetrics(SM_CXSCREEN);
inline int Height = GetSystemMetrics(SM_CYSCREEN);

// Called once per frame from the render thread with the live draw-surface size.
// Reads on worker threads are unsynchronized, which is fine for an aligned int: a
// worker can only ever see the previous or the current frame's value, never a torn one.
inline void SyncViewportSize(int width, int height)
{
	// A minimized or mid-reset surface reports degenerate sizes; keeping the last good
	// pair beats dividing the projection by zero.
	if (width < 64 || height < 64)
		return;

	if (Width != width)
		Width = width;
	if (Height != height)
		Height = height;
}

class Game
{
public:

	uintptr_t
		Engine,
		Entity,
		UWorld,
		ViewPort,
		GameInstance,
		PersistentLevel,
		owningworld,
		GameState,
		LocalPlayer,
		PlayerController,
		PlayerCameraManager,
		LocalPawn,
		LocalFov,
		SkeletalMesh,
		PlayerState,
		RootComponent,
		CurrentWeapon,
		PlayerArray;
	uint32_t ViewPortIndex = 0;
	float ProjectileSpeed = 0;
	float ProjectileGravity = 0;
	float MaxProjectileSpeed = 0;
	float WorldGravity = 0;
	int64_t   ArcId = -1;
	bool IsGrenade = false;
	bool IsMine = false;
	bool IsTrap = false;
	uintptr_t HealthComponent = 0;
	uintptr_t InteractionComp = 0;
	double CurrentHealth = 0.0;
	double MaxHealth = 0.0;
	double CurrentArmor = 0.0;
	double MaxArmor = 0.0;
	int AmmoCount = -1;
	bool IsZooming = false;
	bool IsSprinting = false;
	bool IsDodgeRolling = false;
	bool IsInJump = false;
	bool IsReloading = false;
	bool IsFiring = false;
	uint8_t CharacterStance = 0;
	std::string ActorName;
	bool IsPioneer = false;
	bool Npc = false;
	bool Drone = false;
	bool Destroyed = false;
	bool IsLoot = false;
	bool CacheOpened = false;
	bool IsContainer = false;
	bool IsSalvage = false;
	bool IsExtraction = false;
	bool IsCorpse = false;
	bool IsRaiderCache = false;
	bool ContainerOpened = false;
	bool IsArcProbe = false;
	std::string ArcName;
	std::string ArcDisplayName;
	std::string LootCategoryName;
	std::string ExtractionCategoryName;
	std::vector<uint8_t> PngData;
	ID3D11ShaderResourceView* Texture = nullptr;
	int ItemValue = -1;
	bool StaticDataLoaded = false;
	Vector3 CameraLocation;
	FRotator CameraRotation;
	float CameraFOV = 90.0f;
	float CameraAspect = 1.0f;
	bool CameraValid = false;
	bool stale = false;

	int LootAmount = 0;
	uint8_t LootCategory = 0xFF;
	std::string Name;
	std::wstring SquadName;
	uint8_t Platform = 0; // PlayerPlatform: 0 unknown, 1 Steam, 2 Xbox, 3 PSN, 4 EGS
	std::string ItemName;
	bool       is_team;
	int       PlayerArraySize;
	int TeamIndex;
	uintptr_t SquadPtr = 0;
	uintptr_t Projectile = 0;
	Vector3 RelativeLocation;
	uintptr_t Owner = 0;
	std::string HeldItem;
	std::string WeaponLevel;
};

Game Cache;

inline std::mutex g_CacheMutex;

struct FVector3d {
	double x, y, z;
};

struct FVector {
	double x, y, z;
};


bool IsVisible(uintptr_t mesh)
{
	if (!is_valid_pointer_optimized(mesh))
		return false;

	if (g_PlatformVersion != PlatformVersion::Xbox)
		return Platform::Steam::IsVisible(mesh);

	float times[3];
	uintptr_t addr = mesh + Offsets::Get().USkeletalMeshComponent.BoundsScale + 0x4;
	if (!ctx->read_array(addr, times, sizeof(times)))
		return false;
	if (_isnanf(times[0]) || _isnanf(times[1]) || _isnanf(times[2]))
		return false;
	return (times[1] == times[2]) && ((times[0] - times[1]) <= 0.04f);
}

inline bool IsVisibleBatch(uintptr_t mesh)
{
	if (!is_valid_pointer_optimized(mesh))
		return false;

	if (!ctx || ctx->cr3 == 0)
		return false;

	if (g_PlatformVersion != PlatformVersion::Xbox)
		return Platform::Steam::IsVisible(mesh);

	float times[3];
	uintptr_t addr = mesh + Offsets::Get().USkeletalMeshComponent.BoundsScale + 0x4;
	if (!ctx->read_array(addr, times, sizeof(times)))
		return false;

	if (!std::isfinite(times[0]) || !std::isfinite(times[1]) || !std::isfinite(times[2]))
		return false;

	return (times[1] == times[2]) && ((times[0] - times[1]) <= 0.04f);
}

// Torn-read last-good for GetCameraInfo. Process-lifetime statics used to keep the
// previous raid's pose after EndofRound, which is what projected the next map's
// players into the air. Reset on UWorld change / raid leave.
namespace CamLastGood {
	inline Vector3 pos{};
	inline FRotator rot{};
	inline float fov = 90.0f;
	inline uint64_t tick = 0;
	inline bool initialized = false;
	inline int fovDefaultStreak = 0;
	inline uintptr_t uworld = 0;
}

inline void ResetCameraLastGood()
{
	CamLastGood::pos = {};
	CamLastGood::rot = {};
	CamLastGood::fov = 90.0f;
	CamLastGood::tick = 0;
	CamLastGood::initialized = false;
	CamLastGood::fovDefaultStreak = 0;
	CamLastGood::uworld = 0;
}

// Overlay-commanded FOV changes. The torn-read gates below treat a large FOV
// step as evidence of a bad read, which is correct for every step the overlay
// did not cause - and wrong for the ones it did. Zoom moves FOV 80 -> 11 in a
// frame, which tripped the 40-degree gate and swapped the whole camera triple
// for a cached one, detaching the ESP from the world. Confirmed in the log:
// all 89 badFov rejections fall inside a zoom-held window and there are none
// anywhere else.
//
// The window decays rather than being cleared on release, because the ramp
// back to the game's own FOV passes through arbitrary intermediate values that
// match no specific commanded number.
namespace CamFovIntent {
	inline uint64_t activeTick = 0;
	inline float commanded = 0.0f;
	inline float applied = 0.0f;
	inline float hipPov = 80.0f;

	enum class Lens { Hip, Ads, Transit };

	// Hip ~80, irons/scopes ~37-55. The 62-76 band is the ADS ramp: writing the
	// hip changer there projects at 90 while the game is already ~55 (float).
	inline Lens ClassifyLens(float pov) {
		if (!std::isfinite(pov) || pov < 1.0f || pov > 170.0f)
			return Lens::Transit;
		if (pov <= 62.0f)
			return Lens::Ads;
		if (pov >= 76.0f)
			return Lens::Hip;
		return Lens::Transit;
	}

	inline float MagnifyFov(float baseFov, float mag) {
		if (!std::isfinite(baseFov) || baseFov < 1.0f || baseFov > 170.0f)
			baseFov = 90.0f;
		if (!std::isfinite(mag) || mag < 1.0f)
			mag = 1.0f;
		if (mag > 30.0f)
			mag = 30.0f;
		constexpr float kDegToRad = 0.01745329252f;
		constexpr float kRadToDeg = 57.29577951f;
		const float zoomedHalf = std::atan(std::tan(baseFov * 0.5f * kDegToRad) / mag);
		return zoomedHalf * 2.0f * kRadToDeg;
	}

	// Called with the FOV being commanded while an override is held, and with
	// no argument on release - the window stays open either way so the ramp
	// back is covered, but the commanded value stops being offered to W2S.
	inline void Mark(float fov = 0.0f) {
		activeTick = GetTickCount64();
		commanded = fov;
		if (fov <= 0.0f)
			applied = 0.0f;
	}
	inline bool Recent() {
		return activeTick != 0 && (GetTickCount64() - activeTick) <= 400;
	}
	// The FOV the renderer is being driven to, or 0 when the overlay is not
	// driving it. Zero is not a legal FOV, so it doubles as "no override".
	inline float Commanded() {
		return (commanded > 0.0f && Recent()) ? commanded : 0.0f;
	}

	// Only hand W2S a commanded FOV that actually stuck on LockedFov. Writing
	// during ADS used to Mark(4.5) while the game kept 45, which collapsed ESP.
	inline void NoteLockReadback(float lock) {
		if (commanded <= 0.0f) {
			applied = 0.0f;
			return;
		}
		if (std::isfinite(lock) && std::fabs(lock - commanded) < 1.0f)
			applied = lock;
		else
			applied = 0.0f;
	}
	inline float ForW2S() {
		return (applied > 0.0f && Recent()) ? applied : 0.0f;
	}

	inline void NoteHipPov(float pov)
	{
		if (!std::isfinite(pov) || pov < 72.0f || pov > 120.0f)
			return;
		if (hipPov < 40.0f || hipPov > 120.0f)
			hipPov = pov;
		else
			hipPov = hipPov * 0.85f + pov * 0.15f;
	}

	inline bool PovLooksLikeGameAds(float pov, float defaultFov) {
		(void)defaultFov;
		return ClassifyLens(pov) != Lens::Hip;
	}
}

#ifdef DEBUG_CONSOLE
namespace CamReadDiag {
	enum class Reject : uint8_t {
		None = 0,
		NoWorld,
		NoLocalController,
		NoPcm,
		NoViewTarget,
		PovReadFailed,
		PovPartialRead,
		LocationNonFinite,
		LocationZero,
		RotationNonFinite,
		RotationDegenerate,
		FovNonFinite,
		FovOutOfRange,
		PoseJump,
		FovJump,
		LastGoodExpired,
		Unknown
	};

	struct Snap {
		bool sampled = false;
		bool scalarFallback = false;
		bool pairAgree = false;
		bool locFinite = false;
		bool locZero = false;
		bool rotFinite = false;
		bool rotDegenerate = false;
		bool fovFinite = false;
		bool fovPlausible = false;
		bool lastGoodUsed = false;
		uint64_t lastGoodAgeMs = ~0ull;
		Reject reject = Reject::Unknown;
	};

	inline thread_local Snap g_last{};

	inline void ResetSnap()
	{
		g_last = Snap{};
	}

	inline void NoteScalars(const Vector3& loc, const FRotator& rot, float fov)
	{
		g_last.sampled = true;
		g_last.locFinite = std::isfinite(loc.x) && std::isfinite(loc.y) && std::isfinite(loc.z);
		g_last.locZero = g_last.locFinite && loc.x == 0.0f && loc.y == 0.0f && loc.z == 0.0f;
		g_last.rotFinite = std::isfinite(rot.Pitch) && std::isfinite(rot.Yaw) && std::isfinite(rot.Roll);
		g_last.rotDegenerate = g_last.rotFinite && rot.Pitch == 0.0 && rot.Yaw == 0.0;
		g_last.fovFinite = std::isfinite(fov);
		g_last.fovPlausible = g_last.fovFinite && fov >= 1.0f && fov <= 170.0f;
	}
}
#endif

// Dummy cinematic/default lens (pid=50736 Buried City): pov=(0,-2150,-1425)
// rot=(0,0,0) fov=51, viewTargetPresent=0. mag2 ~6.7e6 passes locInWorld (1e6)
// and SHAPE worldAxes. Real Buried City is ~1e11; a spawn pose with only Z on
// the floor (~-49000) is ~2e9. Do not treat the dummy as a live player camera.
inline bool PcmPovLooksLikeDummyLens(const Vector3& loc, const FRotator& rot)
{
	if (!std::isfinite(loc.x) || !std::isfinite(loc.y) || !std::isfinite(loc.z))
		return false;
	if (!std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll))
		return false;
	const double mag2 = (double)loc.x * loc.x + (double)loc.y * loc.y + (double)loc.z * loc.z;
	if (mag2 >= 1.0e8)
		return false;
	int degenerateAxes = 0;
	if (fabs(loc.x) < 1.0) ++degenerateAxes;
	if (fabs(loc.y) < 1.0) ++degenerateAxes;
	if (fabs(loc.z) < 1.0) ++degenerateAxes;
	const bool zeroRot = fabs(rot.Pitch) < 0.05 && fabs(rot.Yaw) < 0.05;
	return zeroRot || degenerateAxes >= 1;
}

inline bool PcmHasLiveViewTarget(uintptr_t pcm)
{
	if (!is_valid_pointer_optimized(pcm) || !ctx)
		return false;
	const uintptr_t target = ctx->read<uintptr_t>(
		pcm + Offsets::Get().CameraManager.ViewTarget);
	return is_valid_pointer_optimized(target);
}

// Fill POV rotation from APlayerController::ControlRotation when ViewTarget
// still has the default (0,0) rotator (common for ~1–2s after raid possess).
inline bool TryControlRotationFromPcm(uintptr_t pcm, FRotator& outRot)
{
	if (!is_valid_pointer_optimized(pcm) || !ctx)
		return false;
	uintptr_t pc = ctx->read<uintptr_t>(pcm + Offsets::Get().CameraManager.PCOwner);
	if (!is_valid_pointer_optimized(pc))
		return false;
	const FRotator cr = ctx->read<FRotator>(
		pc + Offsets::Get().UPlayerController.ControlRotation);
	if (!std::isfinite(cr.Pitch) || !std::isfinite(cr.Yaw) || !std::isfinite(cr.Roll))
		return false;
	if (fabs(cr.Pitch) < 0.05 && fabs(cr.Yaw) < 0.05)
		return false;
	if (fabs(cr.Pitch) > 90.5 || fabs(cr.Yaw) > 360.5 || fabs(cr.Roll) > 180.5)
		return false;
	outRot = cr;
	return true;
}

inline bool TryControlRotationFromPc(uintptr_t pc, FRotator& outRot)
{
	if (!is_valid_pointer_optimized(pc) || !ctx)
		return false;
	const FRotator cr = ctx->read<FRotator>(
		pc + Offsets::Get().UPlayerController.ControlRotation);
	if (!std::isfinite(cr.Pitch) || !std::isfinite(cr.Yaw) || !std::isfinite(cr.Roll))
		return false;
	if (fabs(cr.Pitch) < 0.05 && fabs(cr.Yaw) < 0.05)
		return false;
	if (fabs(cr.Pitch) > 90.5 || fabs(cr.Yaw) > 360.5 || fabs(cr.Roll) > 180.5)
		return false;
	outRot = cr;
	return true;
}

inline bool GetCameraInfo(uintptr_t playerCameraManager, Vector3& outLocation, FRotator& outRotation, float& outFOV, uintptr_t uworld = 0)
{
#ifdef DEBUG_CONSOLE
	CamReadDiag::ResetSnap();
#endif
	if (!is_valid_pointer_optimized(playerCameraManager)) {
#ifdef DEBUG_CONSOLE
		CamReadDiag::g_last.reject = CamReadDiag::Reject::NoPcm;
#endif
		return false;
	}

	uintptr_t viewTarget = playerCameraManager + Offsets::Get().CameraManager.ViewTarget;
	uintptr_t pov = viewTarget + Offsets::Get().FTViewTarget.POV;

	// One POV snapshot: blob read first, three separate reads as fallback.
	// Unchanged from the original read path - the retry/agreement logic below
	// sits on top of it and does not care which branch produced the values.
	auto readPovSnapshot = [&](Vector3& loc, FRotator& rot, float& fov) {
		const auto& o = Offsets::Get().FMinimalViewInfo;
		uintptr_t base = pov + o.Location;
		size_t size = (size_t)(o.FOV - o.Location + sizeof(float));
		if (size <= 64) {
			uint8_t buf[64];
			if (ctx->read_array(base, buf, size)) {
				loc = *(Vector3*)buf;
				rot = *(FRotator*)(buf + (o.Rotation - o.Location));
				fov = *(float*)(buf + (o.FOV - o.Location));
				return;
			}
		}
#ifdef DEBUG_CONSOLE
		CamReadDiag::g_last.scalarFallback = true;
#endif
		loc = ctx->read<Vector3>(pov + o.Location);
		rot = ctx->read<FRotator>(pov + o.Rotation);
		fov = ctx->read<float>(pov + o.FOV);
	};

	// Wrap-safe angular difference. Raw subtraction would report ~360 for a
	// camera sitting on the +/-180 yaw boundary, which is a real orientation
	// the player passes through constantly.
	auto angleDelta = [](double a, double b) -> double {
		double d = std::fmod(std::fabs(a - b), 360.0);
		return (d > 180.0) ? (360.0 - d) : d;
	};

	// Torn-read detection by agreement between two back-to-back snapshots of the
	// same POV block. No real camera motion can move rotation several degrees or
	// FOV a degree in the time between two driver reads, so disagreement at that
	// scale means one of the two observed the struct mid-write.
	//
	// The 25-degree rotation epsilon used to let a zeroed pitch through against a
	// live -15 (seen as [CamTear] pitch read1=0 read2=-14.94). When that happened
	// the first snapshot was kept even if it was the torn one. Prefer the snapshot
	// that is not origin/zero-rot, and only then ask the two live reads to agree.
	constexpr int kMaxPovAttempts = 3;
	constexpr double kRotAgreeEpsDeg = 3.0;
	constexpr float kFovAgreeEpsDeg = 0.75f;

	auto locInWorld = [](const Vector3& loc) -> bool {
		if (!std::isfinite(loc.x) || !std::isfinite(loc.y) || !std::isfinite(loc.z))
			return false;
		if (loc.x == 0.0 && loc.y == 0.0 && loc.z == 0.0)
			return false;
		const double mag2 = loc.x * loc.x + loc.y * loc.y + loc.z * loc.z;
		return mag2 > 1.0e6 && mag2 < 1.0e16;
	};

	auto povLooksTorn = [&](const Vector3& loc, const FRotator& rot, float fov) -> bool {
		if (!std::isfinite(loc.x) || !std::isfinite(loc.y) || !std::isfinite(loc.z))
			return true;
		if (!std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll))
			return true;
		if (!std::isfinite(fov) || fov <= 0.f || fov > 180.f)
			return true;
		if (loc.x == 0.0f && loc.y == 0.0f && loc.z == 0.0f)
			return true;
		if (rot.Pitch == 0.0 && rot.Yaw == 0.0) {
			if (CamLastGood::initialized)
				return true;
			return !locInWorld(loc);
		}
		return false;
	};

	auto povAgrees = [&](const FRotator& a, float af, const FRotator& b, float bf) -> bool {
		return angleDelta(a.Pitch, b.Pitch) <= kRotAgreeEpsDeg &&
			angleDelta(a.Yaw, b.Yaw) <= kRotAgreeEpsDeg &&
			angleDelta(a.Roll, b.Roll) <= kRotAgreeEpsDeg &&
			std::fabs(af - bf) <= kFovAgreeEpsDeg;
	};

	bool povSelfConsistent = false;
	for (int attempt = 0; attempt < kMaxPovAttempts && !povSelfConsistent; ++attempt) {
		Vector3 locA{}, locB{};
		FRotator rotA{}, rotB{};
		float fovA = 0.0f, fovB = 0.0f;
		readPovSnapshot(locA, rotA, fovA);
		readPovSnapshot(locB, rotB, fovB);

		const bool tornA = povLooksTorn(locA, rotA, fovA);
		const bool tornB = povLooksTorn(locB, rotB, fovB);

		if (!tornA && !tornB && povAgrees(rotA, fovA, rotB, fovB)) {
			outLocation = locA;
			outRotation = rotA;
			outFOV = fovA;
			povSelfConsistent = true;
#ifdef DEBUG_CONSOLE
			CamReadDiag::g_last.pairAgree = true;
#endif
		} else if (tornA && !tornB) {
			outLocation = locB;
			outRotation = rotB;
			outFOV = fovB;
			povSelfConsistent = true;
		} else if (!tornA && tornB) {
			outLocation = locA;
			outRotation = rotA;
			outFOV = fovA;
			povSelfConsistent = true;
		} else if (!tornA && !tornB && CamLastGood::initialized) {
			const double dA = angleDelta(rotA.Yaw, CamLastGood::rot.Yaw)
				+ angleDelta(rotA.Pitch, CamLastGood::rot.Pitch);
			const double dB = angleDelta(rotB.Yaw, CamLastGood::rot.Yaw)
				+ angleDelta(rotB.Pitch, CamLastGood::rot.Pitch);
			if (dA <= dB) {
				outLocation = locA;
				outRotation = rotA;
				outFOV = fovA;
			} else {
				outLocation = locB;
				outRotation = rotB;
				outFOV = fovB;
			}
			povSelfConsistent = true;
		} else {
			outLocation = !tornB ? locB : locA;
			outRotation = !tornB ? rotB : rotA;
			outFOV = !tornB ? fovB : fovA;
		}
	}

#ifdef DEBUG_CONSOLE
	CamReadDiag::NoteScalars(outLocation, outRotation, outFOV);
	CamReadDiag::Reject liveReject = CamReadDiag::Reject::Unknown;
#endif

	if (!std::isfinite(outLocation.x) || !std::isfinite(outLocation.y) || !std::isfinite(outLocation.z)) {
#ifdef DEBUG_CONSOLE
		CamReadDiag::g_last.reject = CamReadDiag::Reject::LocationNonFinite;
#endif
		return false;
	}
	if (!std::isfinite(outRotation.Pitch) || !std::isfinite(outRotation.Yaw) || !std::isfinite(outRotation.Roll)) {
#ifdef DEBUG_CONSOLE
		CamReadDiag::g_last.reject = CamReadDiag::Reject::RotationNonFinite;
#endif
		return false;
	}
	if (!std::isfinite(outFOV) || outFOV <= 0.f || outFOV > 180.f) {
#ifdef DEBUG_CONSOLE
		CamReadDiag::g_last.reject = CamReadDiag::Reject::FovNonFinite;
		if (std::isfinite(outFOV))
			CamReadDiag::g_last.reject = CamReadDiag::Reject::FovOutOfRange;
#endif
		return false;
	}

	// Last-known-good camera cache with torn-read rejection. GetCameraInfo's blob
	// read (or its 3-separate-read fallback) can observe the target process
	// mid-write to the POV struct: whole-struct zero, individual scalars zeroed,
	// or a torn mix of old/new fields. Confirmed via the [CamTear]/[W2SDiag]
	// diagnostics - this poisons ViewContext::Build()'s basis every time it
	// happens, which is the ESP flicker root cause. Render-thread only.
	if (uworld && CamLastGood::uworld && uworld != CamLastGood::uworld)
		ResetCameraLastGood();

	const char* rejectReason = nullptr;

	// (a) real camera is never sitting exactly at world origin in ARC Raiders.
	if (outLocation.x == 0.0f && outLocation.y == 0.0f && outLocation.z == 0.0f) {
		rejectReason = XC("zeroPos");
#ifdef DEBUG_CONSOLE
		liveReject = CamReadDiag::Reject::LocationZero;
#endif
	}

	// Dummy default lens. Must not become last-good or W2S will sit at origin
	// while players are on the map (all offScreenX, firstDraw never fires).
	// Spawn often has a real world location with rot still (0,0) — patch from
	// ControlRotation before treating it as the cinematic dummy.
	if (!rejectReason && PcmPovLooksLikeDummyLens(outLocation, outRotation)) {
		if (TryControlRotationFromPcm(playerCameraManager, outRotation) &&
			!PcmPovLooksLikeDummyLens(outLocation, outRotation)) {
			// recovered
		} else {
			rejectReason = XC("dummyLens");
#ifdef DEBUG_CONSOLE
			liveReject = CamReadDiag::Reject::RotationDegenerate;
#endif
		}
	}

	// (b) zeroed rotation. Pitch and yaw both exactly 0.0 is the dominant tear
	// signature in the [CamTear] data (read1=0.0000 against a real read2) and is
	// not a reachable live orientation: yaw is a continuous double driven by
	// mouse input, so exact 0.0 on both axes simultaneously means the read
	// observed cleared memory. Roll is excluded - it is legitimately 0.
	// Pre-raid the camera genuinely reads (0,0,0); that state has no last-good
	// to fall back to yet, so it falls through untouched as before.
	// After possess, ControlRotation is live before ViewTarget rot — use it.
	if (!rejectReason && outRotation.Pitch == 0.0 && outRotation.Yaw == 0.0) {
		if (TryControlRotationFromPcm(playerCameraManager, outRotation)) {
			// recovered
		} else if (CamLastGood::initialized || !locInWorld(outLocation)) {
			rejectReason = XC("zeroRot");
#ifdef DEBUG_CONSOLE
			liveReject = CamReadDiag::Reject::RotationDegenerate;
#endif
		}
	}

	// (c) rotation/FOV that never agreed with itself across the retries above.
	if (!rejectReason && !povSelfConsistent) {
		rejectReason = XC("tornRot");
#ifdef DEBUG_CONSOLE
		liveReject = CamReadDiag::Reject::PovReadFailed;
#endif
	}

	// (d) FOV outside any sane ARC Raiders range (base isfinite/0/180 check above
	// is looser than this - keep both, this one is the torn-read-specific gate).
	if (!rejectReason && (outFOV <= 1.0f || outFOV > 179.0f)) {
		rejectReason = XC("badFov");
#ifdef DEBUG_CONSOLE
		liveReject = CamReadDiag::Reject::FovOutOfRange;
#endif
	}

	// (d2) default-constructed FMinimalViewInfo FOV. Every [CamTear] line that zeroed
	// Location or Rotation carried fov=90.0000 in the same read, so ~90.0 is the struct
	// default rather than a live value. The double-read agreement gate in (c) cannot see
	// it - both reads observe the same default - and (e)'s 40-degree delta does not cover
	// 55 to 90.
	// Compared with an epsilon, not ==: the value reaches here as a float through two
	// different read paths, and an exact compare let a default frame through (observed as
	// [CamDiag] fov=90.00 projScale11=1.00000 on a frame whose real FOV was 55, which
	// halves every projected box for that frame).
	// The streak escape covers the case this check would otherwise break: FOV reaching a
	// real 90 by a step change (a locked-FOV write) instead of the smooth ADS ramp that
	// normally walks lastGoodFov up to it. A tear shows 90 for a frame or two; a real
	// value holds, so holding it is what distinguishes them.
	// Suspended while the overlay is driving the FOV: releasing zoom can land
	// on a real 90 in one step, which is indistinguishable from the tear this
	// gate exists to catch except by knowing we asked for it.
	constexpr int kFovAdoptStreak = 6;
	const bool fovIntentRecent = CamFovIntent::Recent();
	const bool fovLooksDefault = std::fabs(outFOV - 90.0f) < 0.01f;
	CamLastGood::fovDefaultStreak = fovLooksDefault ? (CamLastGood::fovDefaultStreak + 1) : 0;
	if (!rejectReason && !fovIntentRecent && CamLastGood::initialized && fovLooksDefault &&
		std::fabs(CamLastGood::fov - 90.0f) > 1.0f && CamLastGood::fovDefaultStreak < kFovAdoptStreak) {
		rejectReason = XC("defaultFov");
#ifdef DEBUG_CONSOLE
		liveReject = CamReadDiag::Reject::FovJump;
#endif
	}

	// (e) sanity delta from last-good, only once we actually have a last-good to
	// compare against on THIS UWorld. A rematch teleport is hundreds of meters
	// and is not a tear - posJump only applies while last-good belongs to the
	// same world.
	const bool lastGoodThisWorld = CamLastGood::initialized &&
		(CamLastGood::uworld == 0 || uworld == 0 || CamLastGood::uworld == uworld);
	if (!rejectReason && lastGoodThisWorld) {
		float dx = outLocation.x - CamLastGood::pos.x;
		float dy = outLocation.y - CamLastGood::pos.y;
		float dz = outLocation.z - CamLastGood::pos.z;
		float posDeltaSq = dx * dx + dy * dy + dz * dz;
		if (posDeltaSq > (10000.0f * 10000.0f)) {
			rejectReason = XC("posJump");
#ifdef DEBUG_CONSOLE
			liveReject = CamReadDiag::Reject::PoseJump;
#endif
		} else if (!fovIntentRecent && std::fabs(outFOV - CamLastGood::fov) > 40.0f) {
			rejectReason = XC("badFov");
#ifdef DEBUG_CONSOLE
			liveReject = CamReadDiag::Reject::FovJump;
#endif
		}
	}

	uint64_t nowCamTick = GetTickCount64();

	if (!rejectReason) {
		CamLastGood::pos = outLocation;
		CamLastGood::rot = outRotation;
		CamLastGood::fov = outFOV;
		CamLastGood::tick = nowCamTick;
		CamLastGood::initialized = true;
		if (uworld)
			CamLastGood::uworld = uworld;
#ifdef DEBUG_CONSOLE
		CamReadDiag::g_last.reject = CamReadDiag::Reject::None;
		CamReadDiag::g_last.lastGoodUsed = false;
#endif
		return true;
	}

	if (CamLastGood::initialized && lastGoodThisWorld && (nowCamTick - CamLastGood::tick) <= 2000) {
		static uint64_t s_camCacheWindowStart = 0;
		static int s_camCacheLoggedThisWindow = 0;
		if ((nowCamTick - s_camCacheWindowStart) > 1000) {
			s_camCacheWindowStart = nowCamTick;
			s_camCacheLoggedThisWindow = 0;
		}
		if (s_camCacheLoggedThisWindow < 5) {
			++s_camCacheLoggedThisWindow;
			DBG_LOG(XC("[CamCache] restored from cache tick=") << nowCamTick
				<< XC(" age=") << (nowCamTick - CamLastGood::tick)
				<< XC(" ms reason=") << rejectReason);
		}
		// All three are restored together even when only one field was rejected.
		// A mutually consistent triple up to 2s old builds a correct basis; a
		// fresh position mixed with a cached rotation does not.
		outLocation = CamLastGood::pos;
		outRotation = CamLastGood::rot;
		outFOV = CamLastGood::fov;
#ifdef DEBUG_CONSOLE
		CamReadDiag::g_last.lastGoodUsed = true;
		CamReadDiag::g_last.lastGoodAgeMs = nowCamTick - CamLastGood::tick;
		CamReadDiag::g_last.reject = liveReject;
#endif
		return true;
	}

	// Torn and no last-good, or last-good too old. Do not hand the renderer the
	// garbage read — that is the floating-ESP path (pawn/origin pose at FOV 90).
#ifdef DEBUG_CONSOLE
	CamReadDiag::g_last.reject = liveReject;
	if (CamReadDiag::g_last.reject == CamReadDiag::Reject::Unknown &&
		CamLastGood::initialized && lastGoodThisWorld)
		CamReadDiag::g_last.reject = CamReadDiag::Reject::LastGoodExpired;
#endif
	return false;
}

struct ViewContext {
	Vector3 origin;
	Vector3 forward;
	Vector3 right;
	Vector3 up;
	double  invTanFovAspect;
	double  invTanFov;
	double  halfW;
	double  halfH;
	uint32_t screenW;
	uint32_t screenH;

	void Build(const Vector3& cameraLocation, const FRotator& cameraRotation,
	           float cameraFOV, float aspectRatio, uint32_t w, uint32_t h)
	{
		constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;
		double pitchRad = cameraRotation.Pitch * DEG2RAD;
		double yawRad   = cameraRotation.Yaw   * DEG2RAD;

		double cp = cos(pitchRad), sp = sin(pitchRad);
		double cy = cos(yawRad),   sy = sin(yawRad);

		origin  = cameraLocation;
		forward = Vector3(cp * cy, cp * sy, sp);
		right   = Vector3(-sy, cy, 0.0);
		up      = Vector3(-sp * cy, -sp * sy, cp);

		double tanHalf = tan(cameraFOV * DEG2RAD * 0.5);
		invTanFovAspect = 1.0 / (tanHalf * aspectRatio);
		invTanFov       = 1.0 / tanHalf;
		halfW   = w * 0.5;
		halfH   = h * 0.5;
		screenW = w;
		screenH = h;
	}



	inline bool ProjectNormalized(const Vector3& world_pos, double* outSx, double* outSy) const {
		if (!outSx || !outSy)
			return false;
		Vector3 delta(world_pos.x - origin.x, world_pos.y - origin.y, world_pos.z - origin.z);
		double z = delta.x * forward.x + delta.y * forward.y + delta.z * forward.z;
		if (z < 0.001)
			return false;
		double invZ = 1.0 / z;
		*outSx = (delta.x * right.x + delta.y * right.y + delta.z * right.z) * invZ * invTanFovAspect;
		*outSy = (delta.x * up.x + delta.y * up.y + delta.z * up.z) * invZ * invTanFov;
		return true;
	}

	inline bool Project(const Vector3& world_pos, Vector2* out) const {
		Vector3 delta(world_pos.x - origin.x, world_pos.y - origin.y, world_pos.z - origin.z);
		double z = delta.x * forward.x + delta.y * forward.y + delta.z * forward.z;
		if (z < 0.001) return false;
		double invZ = 1.0 / z;
		double sx = (delta.x * right.x + delta.y * right.y + delta.z * right.z) * invZ * invTanFovAspect;
		double sy = (delta.x * up.x    + delta.y * up.y    + delta.z * up.z)    * invZ * invTanFov;
		out->x = (float)(halfW + sx * halfW);
		out->y = (float)(halfH - sy * halfH);
		return true;
	}

	// Diagnostic-only twin of Project() with a richer failure reason. Mirrors the
	// exact same math/thresholds as Project() (does not change behavior - Project()
	// itself is untouched and still used for all production rendering), so it can
	// be called *in addition to* Project() at a failed entity to find out which
	// specific condition rejected it, without altering what gets drawn.
	enum class ProjectResult { Ok, ZeroDepth, BehindCamera, NanCoords, OffScreenNDC };

	inline ProjectResult ProjectDiag(const Vector3& world_pos, Vector2* out) const {
		Vector3 delta(world_pos.x - origin.x, world_pos.y - origin.y, world_pos.z - origin.z);
		double z = delta.x * forward.x + delta.y * forward.y + delta.z * forward.z;
		if (!std::isfinite(z))
			return ProjectResult::NanCoords;
		if (z < 0.001)
			return (z < 0.0) ? ProjectResult::BehindCamera : ProjectResult::ZeroDepth;
		double invZ = 1.0 / z;
		double sx = (delta.x * right.x + delta.y * right.y + delta.z * right.z) * invZ * invTanFovAspect;
		double sy = (delta.x * up.x    + delta.y * up.y    + delta.z * up.z)    * invZ * invTanFov;
		if (!std::isfinite(sx) || !std::isfinite(sy))
			return ProjectResult::NanCoords;
		if (out) {
			out->x = (float)(halfW + sx * halfW);
			out->y = (float)(halfH - sy * halfH);
		}
		if (sx < -1.5 || sx > 1.5 || sy < -1.5 || sy > 1.5)
			return ProjectResult::OffScreenNDC;
		return ProjectResult::Ok;
	}
};


inline void MapScopeNdcToLensScreen(
	double ndcSx, double ndcSy,
	float lensLeft, float lensTop, float lensW, float lensH,
	Vector2* out)
{
	out->x = (float)(lensLeft + (ndcSx + 1.0) * 0.5 * lensW);
	out->y = (float)(lensTop + (1.0 - ndcSy) * 0.5 * lensH);
}





inline bool WorldToScreenPiPLens(
	const Vector3& world_pos,
	const Vector3& cameraLocation,
	const FRotator& cameraRotation,
	float scopeHorizontalFovDeg,
	float pipRenderTargetAspect,
	float lensLeft,
	float lensTop,
	float lensW,
	float lensH,
	Vector2* out_screen,
	bool* outInsideFrustum = nullptr)
{
	ViewContext vc;
	vc.Build(cameraLocation, cameraRotation, scopeHorizontalFovDeg, pipRenderTargetAspect, 2u, 2u);
	double sx = 0.0, sy = 0.0;
	if (!vc.ProjectNormalized(world_pos, &sx, &sy))
		return false;
	if (outInsideFrustum)
		*outInsideFrustum = (sx >= -1.01 && sx <= 1.01 && sy >= -1.01 && sy <= 1.01);
	MapScopeNdcToLensScreen(sx, sy, lensLeft, lensTop, lensW, lensH, out_screen);
	return true;
}

inline const char* ProjectResultName(ViewContext::ProjectResult r) {
	switch (r) {
		case ViewContext::ProjectResult::Ok: return "ok";
		case ViewContext::ProjectResult::ZeroDepth: return "zeroDepth";
		case ViewContext::ProjectResult::BehindCamera: return "behindCam";
		case ViewContext::ProjectResult::NanCoords: return "nanCoords";
		case ViewContext::ProjectResult::OffScreenNDC: return "offScreenNDC";
		default: return "unknown";
	}
}

static inline bool WorldToScreen(
	const Vector3& world_pos,
	const Vector3& cameraLocation,
	const FRotator& cameraRotation,
	float cameraFOV,
	float aspectRatio,
	uint32_t screen_width,
	uint32_t screen_height,
	Vector2* out_screen)
{
	ViewContext vc;
	vc.Build(cameraLocation, cameraRotation, cameraFOV, aspectRatio, screen_width, screen_height);
	return vc.Project(world_pos, out_screen);
}


inline bool WorldToScreenHelper(
	const Vector3& world_pos,
	const Vector3& cameraLocation,
	const FRotator& cameraRotation,
	float cameraFOV,
	float aspectRatio,
	uint32_t screen_width,
	uint32_t screen_height,
	Vector2* out_screen)
{
	return WorldToScreen(world_pos, cameraLocation, cameraRotation, cameraFOV, aspectRatio, screen_width, screen_height, out_screen);
}

inline Vector3 QuatRotate(const Vector3& v, const FQuat& q)
{
	Vector3 qv(q.x, q.y, q.z);
	Vector3 cross1(
		qv.y * v.z - qv.z * v.y,
		qv.z * v.x - qv.x * v.z,
		qv.x * v.y - qv.y * v.x
	);
	Vector3 cross2(
		qv.y * cross1.z - qv.z * cross1.y,
		qv.z * cross1.x - qv.x * cross1.z,
		qv.x * cross1.y - qv.y * cross1.x
	);
	return Vector3(
		v.x + ((cross1.x * q.w) + cross2.x) * 2.0,
		v.y + ((cross1.y * q.w) + cross2.y) * 2.0,
		v.z + ((cross1.z * q.w) + cross2.z) * 2.0
	);
}

inline FQuat RotatorToQuat(const FRotator& rot)
{
	constexpr double DEG2RAD = 3.14159265358979323846 / 180.0;

	double pitchRad = rot.Pitch * DEG2RAD * 0.5;
	double yawRad = rot.Yaw * DEG2RAD * 0.5;
	double rollRad = rot.Roll * DEG2RAD * 0.5;

	double sp = sin(pitchRad);
	double cp = cos(pitchRad);
	double sy = sin(yawRad);
	double cy = cos(yawRad);
	double sr = sin(rollRad);
	double cr = cos(rollRad);

	FQuat q;
	q.x = cr * sp * sy - sr * cp * cy;
	q.y = -cr * sp * cy - sr * cp * sy;
	q.z = cr * cp * sy - sr * sp * cy;
	q.w = cr * cp * cy + sr * sp * sy;

	return q;
}

// Bone-array pointer shape check: alignment and address range only.
//
// This used to also impose a 4GB floor, on the theory that garbage decrypts land low
// (~2.4GB) while real bone buffers live high (~5GB). That theory does not hold. A pawn
// whose two buffers sat at 0x77914930 and 0x779124A0 (~2.0GB) was rejected on every pass
// for an entire match, yet its decrypt was demonstrably correct: the selector's high bit
// alternated, the slot alternated 0x0/0x10 in step with it, the two addresses stayed
// stable and swapped read/edit roles coherently, and numBones read 97 - the same count as
// every healthy pawn. Meanwhile a garbage value at 5GB would have sailed through the
// floor untouched. Address magnitude simply does not separate the two cases; it depends
// on which heap arena the allocator happened to serve the buffer from.
//
// What does separate them is content, and PrepareMeshBoneContext already checks it:
// it scores four candidate (ComponentToWorld, base-offset) layouts through
// ScoreBoneLayout, which requires head/pelvis/foot to sit at plausible distances, stack
// vertically, and land near the pawn origin. A bad pointer fails that on geometry no
// matter where it lives in the address space.
inline bool is_sane_bone_array_pointer(uint64_t p) {
	if (!p) return false;
	if (p & 0x7ULL) return false; // must be 8-byte aligned
	if (p < 0x10000ULL) return false; // null page / small-int garbage
	return p <= 0x00007FFFFFFFFFFFULL;
}

inline uint64_t GetBoneArrayPtrCached(uintptr_t mesh)
{
	if (!is_valid_pointer_optimized(mesh))
		return 0;

	struct CacheEntry {
		uint64_t boneArray = 0;
		uint64_t lastGoodTick = 0;
		uint64_t lastTryTick = 0;
		uint64_t stickyUntil = 0; // hold through UE double-buffer swaps
	};

	static std::unordered_map<uintptr_t, CacheEntry> cache;
	static std::mutex cacheMutex;

	uint64_t nowTick = GetTickCount64();
	std::lock_guard<std::mutex> lock(cacheMutex);

	auto it = cache.find(mesh);
	if (it != cache.end()) {
		auto& entry = it->second;
		// Fresh hit — skip decrypt for a short window.
		if ((entry.boneArray != 0) && is_sane_bone_array_pointer(entry.boneArray) &&
			((nowTick - entry.lastTryTick) <= 25))
		{
			return entry.boneArray;
		}
		// Sticky: UE swaps bone buffers every frame — don't thrash the pointer.
		if ((entry.boneArray != 0) && is_sane_bone_array_pointer(entry.boneArray) &&
			nowTick < entry.stickyUntil)
		{
			entry.lastTryTick = nowTick;
			return entry.boneArray;
		}
	}

	uint64_t boneArray = NameResolver::GetBoneArrayPtr(mesh);
	if ((boneArray != 0) && is_sane_bone_array_pointer(boneArray))
	{
		if (it != cache.end() && it->second.boneArray != 0 &&
			it->second.boneArray != boneArray && nowTick < it->second.stickyUntil)
		{
			it->second.lastTryTick = nowTick;
			return it->second.boneArray;
		}

		cache[mesh] = { boneArray, nowTick, nowTick, nowTick + 120 };

		if (cache.size() > 512) {
			auto oldest = cache.begin();
			for (auto jt = cache.begin(); jt != cache.end(); ++jt) {
				if (jt->second.lastGoodTick < oldest->second.lastGoodTick)
					oldest = jt;
			}
			cache.erase(oldest);
		}

		return boneArray;
	}

	// Decrypt can briefly return 0 — keep last good ptr only briefly (stale ptr = junk bones).
	if (it != cache.end()) {
		auto& entry = it->second;
		entry.lastTryTick = nowTick;
		if ((entry.boneArray != 0) && is_sane_bone_array_pointer(entry.boneArray) &&
			((nowTick - entry.lastGoodTick) <= 200))
		{
			return entry.boneArray;
		}
	}

	return 0;
}

// Both double-buffer slots for a mesh, cached on the same cadence as the primary above.
// PrepareMeshBoneContext scores the two against each other instead of trusting the
// selector bits - see GetBoneArrayPtrSlots in Steam.hpp for why those are not reliable.
static constexpr int kBoneLodSlots = 4;

struct BoneArrayPair {
	uint64_t slots[kBoneLodSlots] = {};
	uint32_t bones[kBoneLodSlots] = {};
	int selected = 0;   // index the selector bits name, favoured but not trusted
	uint64_t primary = 0;
	uint32_t primaryBones = 0;
};

inline BoneArrayPair GetBoneArrayPairCached(uintptr_t mesh)
{
	BoneArrayPair empty;
	if (!is_valid_pointer_optimized(mesh))
		return empty;

	struct Entry {
		BoneArrayPair pair;
		uint64_t lastTick = 0;
	};
	static std::unordered_map<uintptr_t, Entry> cache;
	static std::mutex cacheMutex;

	const uint64_t nowTick = GetTickCount64();
	std::lock_guard<std::mutex> lock(cacheMutex);

	auto it = cache.find(mesh);
	if (it != cache.end() && (nowTick - it->second.lastTick) <= 25)
		return it->second.pair;

	BoneArrayPair pair;
	NameResolver::GetBoneArrayPtrSlotsAll(mesh, pair.slots, pair.bones,
		kBoneLodSlots, pair.selected);

	bool any = false;
	for (int i = 0; i < kBoneLodSlots; ++i) {
		if (!is_sane_bone_array_pointer(pair.slots[i]))
			pair.slots[i] = 0;
		else
			any = true;
	}
	if (pair.selected >= 0 && pair.selected < kBoneLodSlots) {
		pair.primary = pair.slots[pair.selected];
		pair.primaryBones = pair.bones[pair.selected];
	}

	if (!any) {
		// Decrypt can miss transiently; a briefly stale set beats no bones at all.
		if (it != cache.end())
			return it->second.pair;
		return empty;
	}

	if (cache.size() > 512)
		cache.clear();
	cache[mesh] = { pair, nowTick };
	return pair;
}

inline bool IsValidFTransform(const FTransform& t) {
	if (!std::isfinite(t.rot.x) || !std::isfinite(t.rot.y) || !std::isfinite(t.rot.z) || !std::isfinite(t.rot.w))
		return false;
	if (!std::isfinite(t.translation.x) || !std::isfinite(t.translation.y) || !std::isfinite(t.translation.z))
		return false;
	if (!std::isfinite(t.scale.x) || !std::isfinite(t.scale.y) || !std::isfinite(t.scale.z))
		return false;
	return true;
}

// Arc bone buffer: the descriptor points at bone 0, so transforms are at boneArray +
// index * 0x60, each a plaintext component-space FTransform. kBoneArrayBaseOff is the legacy
// pre-patch base and is retained only so the [BoneSlot] diagnostic can still label a pointer
// that landed one bone late.
static constexpr uint64_t kBoneArrayBaseOff = 0x60;
static constexpr uint64_t kBoneStride = 0x60;

struct MeshBoneContext {
	uintptr_t boneArray = 0;      // resolved base used for reads (includes +0x60 when needed)
	uintptr_t boneArrayRaw = 0;   // decrypt result
	FTransform componentToWorld{};
	bool valid = false;
	// What the slot search actually saw. A winning score still sitting at the soft floor
	// means every candidate was rejected on geometry and the winner only won on the
	// tie-break bonus - which is indistinguishable from a real match unless it is logged.
	float chosenScore = -1.e9f;
	int slotsPresent = 0;
};

inline MeshBoneContext PrepareMeshBoneContext(uintptr_t mesh);
inline MeshBoneContext PrepareMeshBoneContext(uintptr_t mesh, const Vector3& hintOrigin);
inline Vector3 GetBoneFromContext(const MeshBoneContext& mbc, int bone_id);

// Forces a usable scale on a transform whose scale field did not read as a real scale. The
// ComponentToWorld scale is unreliable on this build even with the correct 0x370 base: the
// rotation and translation read perfectly, but one player's scale came back as
// (-0.000, 3.1e+144, -1.08e+46). Because that is not exactly zero, an all-zero check passes
// it through, and the matrix multiply then throws every bone far enough out to be rejected on
// coherence - while a mesh whose garbage happened to be all zeroes was normalised to unit and
// drew correctly. That is why only some players lost their skeleton. Player meshes are
// unit-scaled, so anything outside a plausible range is a misread and unit is the right
// recovery. The range is wide enough to leave a genuinely scaled bone alone.
inline void NormalizeFTransformScale(FTransform& t)
{
	auto plausible = [](double v) {
		return std::isfinite(v) && fabs(v) >= 1.0e-3 && fabs(v) <= 1.0e3;
	};
	if (!plausible(t.scale.x) || !plausible(t.scale.y) || !plausible(t.scale.z))
		t.scale = { 1.0, 1.0, 1.0 };
}

// C2W-only. NormalizeFTransformScale's 1e3 bound still lets through the 30-120 garbage
// that some meshes put in the scale slots with a zero quaternion (SheepHerder / Yanks300:
// c2wT matches the pawn, c2wS≈(71,95,119), c2wQn=0). ToMatrixWithScale then treats a
// zero quat as identity and multiplies every bone by that scale, so the pose is ~100x
// too tall and IsCoherentPlayerSkeleton rejects on topPelvis - box draws, skeleton does
// not. Bone locals on those meshes are already unit rest-pose, and translation is the
// pawn origin, so unit scale (and identity if the quat is empty) recovers a drawable
// pose. Stored C2W on the context is left raw so [BoneRaw] still shows the misread.
inline void SanitizeComponentToWorld(FTransform& t)
{
	NormalizeFTransformScale(t);
	auto characterScale = [](double v) {
		return std::isfinite(v) && fabs(v) >= 0.25 && fabs(v) <= 4.0;
	};
	if (!characterScale(t.scale.x) || !characterScale(t.scale.y) || !characterScale(t.scale.z))
		t.scale = { 1.0, 1.0, 1.0 };

	const double qn = t.rot.x * t.rot.x + t.rot.y * t.rot.y + t.rot.z * t.rot.z + t.rot.w * t.rot.w;
	if (!(std::isfinite(qn) && qn >= 0.25 && qn <= 4.0)) {
		t.rot.x = 0.0;
		t.rot.y = 0.0;
		t.rot.z = 0.0;
		t.rot.w = 1.0;
	}
}

inline Vector3 TransformBoneToWorld(const FTransform& bone, const FTransform& c2w)
{
	FTransform b = bone;
	FTransform c = c2w;
	NormalizeFTransformScale(b);
	SanitizeComponentToWorld(c);
	D3DMATRIX matrix = MatrixMultiplication(b.ToMatrixWithScale(), c.ToMatrixWithScale());
	Vector3 pos(matrix._41, matrix._42, matrix._43);
	if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
		return Vector3{};
	return pos;
}

inline bool LooksValidWorldPos(const Vector3& v)
{
	if (!std::isfinite(v.x) || !std::isfinite(v.y) || !std::isfinite(v.z))
		return false;
	if (v.x == 0.0 && v.y == 0.0 && v.z == 0.0)
		return false;
	const double mag2 = v.x * v.x + v.y * v.y + v.z * v.z;
	return mag2 > 1.0 && mag2 < 1.0e16;
}

// Cached PCM with an empty ViewTarget/POV used to stop ScanForPCM. Draw skipped until
// a later UWorld change. Same gates as IsValidCameraFrame / live GetCameraInfo without
// last-good or CamLastGood writes. Scanner-thread safe.
inline bool PcmHasUsableLivePov(uintptr_t pcm)
{
	if (!is_valid_pointer_optimized(pcm) || !ctx)
		return false;

	const auto& off = Offsets::Get();
	const uintptr_t pov = pcm + off.CameraManager.ViewTarget + off.FTViewTarget.POV;
	const Vector3 loc = ctx->read<Vector3>(pov + off.FMinimalViewInfo.Location);
	if (!std::isfinite(loc.x) || !std::isfinite(loc.y) || !std::isfinite(loc.z))
		return false;
	if (loc.x == 0.0f && loc.y == 0.0f && loc.z == 0.0f)
		return false;
	const double mag2 = loc.x * loc.x + loc.y * loc.y + loc.z * loc.z;
	if (mag2 <= 1.0e6 || mag2 >= 1.0e16)
		return false;

	const FRotator rotRead = ctx->read<FRotator>(pov + off.FMinimalViewInfo.Rotation);
	if (!std::isfinite(rotRead.Pitch) || !std::isfinite(rotRead.Yaw) || !std::isfinite(rotRead.Roll))
		return false;
	FRotator rot = rotRead;

	const float fov = ctx->read<float>(pov + off.FMinimalViewInfo.FOV);
	if (!std::isfinite(fov) || fov < 1.0f || fov > 170.0f)
		return false;
	if (PcmPovLooksLikeDummyLens(loc, rot)) {
		if (!TryControlRotationFromPcm(pcm, rot) || PcmPovLooksLikeDummyLens(loc, rot))
			return false;
	}
	// ViewTarget Target actor is often still null for ~1–2s after possess while
	// the embedded POV already has a real map location. Require a live Target
	// only when the location is still weak (lobby / cinematic dummy range).
	if (!PcmHasLiveViewTarget(pcm)) {
		if (mag2 < 1.0e9)
			return false;
	}
	return true;
}

// Identifies a PlayerCameraManager by the shape of its POV. Location and FOV alone are not
// enough: an actor whose POV read back as (0,197377,0) with a field of view of 55 passed
// both, and adopting it put the camera 28 billion units from the map, which failed the
// distance gate for every player and blanked the ESP while the radar kept working. The two
// additions that reject it are a degeneracy test - a real camera sits somewhere in the map
// on all three axes, not on exactly one - and a check that the rotator is really a rotator,
// since that false positive carried a roll of 1.4 billion.
//
// worldAxes used to require two axes above 1000 uu. That rejected a live spawn camera
// sitting near the XY origin with a real Z (Buried City floor is ~-49000). One in-world
// axis is enough; (0,197377,0) is already dead from degenerateAxes > 1.
inline bool LooksLikePlayerCameraManager(uintptr_t cand, bool requirePCOwner = true)
{
	if (!is_valid_pointer_optimized(cand))
		return false;

	const auto& off = Offsets::Get();
	const uintptr_t pov = cand + off.CameraManager.ViewTarget + off.FTViewTarget.POV;

	Vector3 loc = ctx->read<Vector3>(pov + off.FMinimalViewInfo.Location);
	if (!std::isfinite(loc.x) || !std::isfinite(loc.y) || !std::isfinite(loc.z))
		return false;
	int degenerateAxes = 0;
	if (fabs(loc.x) < 1.0) ++degenerateAxes;
	if (fabs(loc.y) < 1.0) ++degenerateAxes;
	if (fabs(loc.z) < 1.0) ++degenerateAxes;
	if (degenerateAxes > 1)
		return false;
	int worldAxes = 0;
	if (fabs(loc.x) > 1000.0) ++worldAxes;
	if (fabs(loc.y) > 1000.0) ++worldAxes;
	if (fabs(loc.z) > 1000.0) ++worldAxes;
	if (worldAxes < 1)
		return false;
	const double mag2 = loc.x * loc.x + loc.y * loc.y + loc.z * loc.z;
	if (mag2 < 1.0e6 || mag2 > 1.0e16)
		return false;

	FRotator rot = ctx->read<FRotator>(pov + off.FMinimalViewInfo.Rotation);
	if (!std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll))
		return false;
	if (fabs(rot.Pitch) > 90.5 || fabs(rot.Yaw) > 360.5 || fabs(rot.Roll) > 180.5)
		return false;

	const float fov = ctx->read<float>(pov + off.FMinimalViewInfo.FOV);
	if (!std::isfinite(fov) || fov < 20.0f || fov > 160.0f)
		return false;
	if (PcmPovLooksLikeDummyLens(loc, rot))
		return false;
	// Same early-spawn rule as PcmHasUsableLivePov: strong map loc does not need
	// ViewTarget Target filled yet.
	if (!PcmHasLiveViewTarget(cand)) {
		const double mag2Early = loc.x * loc.x + loc.y * loc.y + loc.z * loc.z;
		if (mag2Early < 1.0e9)
			return false;
	}

	if (!requirePCOwner)
		return true;
	return is_valid_pointer_optimized(ctx->read<uintptr_t>(cand + off.CameraManager.PCOwner));
}

// Name-free PCM test that does not need a live POV. APlayerController::PlayerCameraManager
// is not a UPROPERTY, so the field is found by probing; this is the check that says the
	// candidate is actually the camera manager for `expectedPc`. DefaultFOV (Steam 0x3F0) and
// LockedFov (0x42C) are no longer adjacent on CL-1341255.
inline bool LooksLikeOwnedPlayerCameraManager(uintptr_t cand, uintptr_t expectedPc)
{
	if (!is_valid_pointer_optimized(cand) || !is_valid_pointer_optimized(expectedPc))
		return false;
	const auto& off = Offsets::Get();
	if (ctx->read<uintptr_t>(cand + off.CameraManager.PCOwner) != expectedPc)
		return false;
	return true;
}

// Bind PC+PlayerCameraManager without a live ViewTarget. DefaultFOV is often still 0
// at possess; name is enough to tell a camera manager from the junk pointer at 0x4E0
// (Debug 50: ack=0x4AF, pcm field set, POV empty). Do not use this on a probe sweep —
// FName on every 8-byte slot is too expensive and too loose.
inline bool PcmLooksBindable(uintptr_t cand, uintptr_t expectedPc = 0, bool allowName = true)
{
	if (!is_valid_pointer_optimized(cand))
		return false;
	if (LooksLikePlayerCameraManager(cand, false))
		return true;
	if (expectedPc && LooksLikeOwnedPlayerCameraManager(cand, expectedPc))
		return true;
	const auto& off = Offsets::Get();
	const float dfov = ctx->read<float>(cand + off.CameraManager.DefaultFOV);
	if (std::isfinite(dfov) && dfov >= 40.0f && dfov <= 120.0f)
		return true;
	if (!allowName)
		return false;
	const uintptr_t owner = ctx->read<uintptr_t>(cand + off.CameraManager.PCOwner);
	if (expectedPc && is_valid_pointer_optimized(owner) && owner != expectedPc)
		return false;
	const std::string an = NameResolver::GetObjectNameA(ctx->image_base, cand);
	if (an.find(XC("CameraManager")) != std::string::npos)
		return true;
	const std::string cn = NameResolver::GetClassPrivateNameA(ctx->image_base, cand);
	return cn.find(XC("CameraManager")) != std::string::npos;
}


// Score a candidate head/pelvis/feet layout. Higher = more human / upright / near pawn.
inline float ScoreBoneLayout(const Vector3& head, const Vector3& pelvis, const Vector3& foot,
	const Vector3& hintOrigin)
{
	if (!LooksValidWorldPos(head) || !LooksValidWorldPos(pelvis) || !LooksValidWorldPos(foot))
		return -1.f;

	const float headPelvis = (float)head.distance(pelvis);
	const float pelvisFoot = (float)pelvis.distance(foot);
	const float headFoot = (float)head.distance(foot);
	if (headPelvis < 6.f || headPelvis > 170.f) return -1.f;
	if (pelvisFoot < 12.f || pelvisFoot > 200.f) return -1.f;
	if (headFoot < 25.f || headFoot > 300.f) return -1.f;

	const float dzHeadPelvis = (float)(head.z - pelvis.z);
	const float dzPelvisFoot = (float)(pelvis.z - foot.z);
	const float dxHeadFoot = (float)(head.x - foot.x);
	const float dyHeadFoot = (float)(head.y - foot.y);
	const float horiz = sqrtf(dxHeadFoot * dxHeadFoot + dyHeadFoot * dyHeadFoot);
	const float vert = (float)(head.z - foot.z);

	float score = 0.f;

	// Prefer head above pelvis above feet (standing / crouch).
	if (dzHeadPelvis > 10.f) score += 40.f + (std::min)(dzHeadPelvis, 90.f) * 0.3f;
	else if (dzHeadPelvis > 0.f) score += 10.f;
	else score -= 35.f; // head below pelvis → sideways/inverted garbage

	if (dzPelvisFoot > 10.f) score += 25.f;
	else if (dzPelvisFoot > -20.f) score += 5.f; // prone-ish OK
	else score -= 20.f;

	// Standing: vertical extent should dominate horizontal (rejects XY-mapped skeletons).
	if (vert > 45.f) {
		if (horiz < vert * 0.85f) score += 50.f;
		else if (horiz < vert * 1.35f) score += 15.f;
		else score -= 60.f; // classic "horizontal skeleton" bug
	}

	if (!hintOrigin.IsZero() && LooksValidWorldPos(hintOrigin)) {
		const float toPawn = (float)pelvis.distance(hintOrigin);
		if (toPawn < 60.f) score += 40.f;
		else if (toPawn < 120.f) score += 15.f;
		else if (toPawn < 220.f) score -= 10.f;
		else score -= 80.f;
	}

	return score;
}

// Raw, untransformed view of one bone plus the component transform it would be combined
// with. The world-space positions alone cannot distinguish a scale problem from a layout
// problem, since both stretch the pose - this shows which it is.
struct RawBoneDump {
	bool ok = false;
	double qx = 0, qy = 0, qz = 0, qw = 0, qnorm = 0;
	double tx = 0, ty = 0, tz = 0;
	double sx = 0, sy = 0, sz = 0;
	double c2wx = 0, c2wy = 0, c2wz = 0;
	double c2wsx = 0, c2wsy = 0, c2wsz = 0;
	double c2wqnorm = 0;
};

inline RawBoneDump ReadRawBoneDump(const MeshBoneContext& mbc, int bone_id)
{
	RawBoneDump d{};
	if (!mbc.valid || bone_id < 0 || bone_id > 200)
		return d;
	const uint64_t addr = mbc.boneArray + (uint64_t)bone_id * kBoneStride;
	if (!is_valid_pointer_optimized(addr))
		return d;

	FTransform b = ctx->read<FTransform>(addr);
	d.qx = b.rot.x; d.qy = b.rot.y; d.qz = b.rot.z; d.qw = b.rot.w;
	d.qnorm = b.rot.x * b.rot.x + b.rot.y * b.rot.y + b.rot.z * b.rot.z + b.rot.w * b.rot.w;
	d.tx = b.translation.x; d.ty = b.translation.y; d.tz = b.translation.z;
	d.sx = b.scale.x; d.sy = b.scale.y; d.sz = b.scale.z;

	const FTransform& c = mbc.componentToWorld;
	d.c2wx = c.translation.x; d.c2wy = c.translation.y; d.c2wz = c.translation.z;
	d.c2wsx = c.scale.x; d.c2wsy = c.scale.y; d.c2wsz = c.scale.z;
	d.c2wqnorm = c.rot.x * c.rot.x + c.rot.y * c.rot.y + c.rot.z * c.rot.z + c.rot.w * c.rot.w;
	d.ok = true;
	return d;
}

inline Vector3 GetBoneFromContext(const MeshBoneContext& mbc, int bone_id)
{
	if (!mbc.valid || bone_id < 0 || bone_id > 200)
		return Vector3{};

	uint64_t boneTransformAddr = mbc.boneArray + (uint64_t)bone_id * kBoneStride;
	if (!is_valid_pointer_optimized(boneTransformAddr))
		return Vector3{};

	// Bones are plaintext component-space FTransforms on this build - quaternion at +0x00,
	// translation at +0x20, scale at +0x40, matching this struct byte for byte. The per-bone
	// cipher that used to be applied here corrupted good data and was already disabling
	// itself, because its key block never validated.
	FTransform bone = ctx->read<FTransform>(boneTransformAddr);
	if (!IsValidFTransform(bone))
		return Vector3{};
	NormalizeFTransformScale(bone);
	return TransformBoneToWorld(bone, mbc.componentToWorld);
}

// Prefer ComponentToWorld — RelativeLocation is often local-space and drops ESP.
// Stick to primary (0x360). Flipping to 0x370 when both "look valid" caused ESP snaps.
inline Vector3 GetSceneComponentWorldLocation(uintptr_t component)
{
	if (!is_valid_pointer_optimized(component))
		return Vector3{};

	const auto& sc = Offsets::Get().USceneComponent;
	FTransform primary = ctx->read<FTransform>(component + sc.ComponentToWorld);
	if (LooksValidWorldPos(primary.translation))
		return primary.translation;

	// Same three-way uncertainty as the bone path, and the same three candidates, so the two
	// paths cannot settle on different bases for the same component.
	for (uintptr_t offAlt : { (uintptr_t)0x370, (uintptr_t)0x378, (uintptr_t)0x208 }) {
		if (offAlt == sc.ComponentToWorld)
			continue;
		FTransform alt = ctx->read<FTransform>(component + offAlt);
		if (LooksValidWorldPos(alt.translation))
			return alt.translation;
	}

	Vector3 rel = ctx->read<Vector3>(component + sc.RelativeLocation);
	if (LooksValidWorldPos(rel))
		return rel;
	return Vector3{};
}

// Probe-safe PCM test. DefaultFOV-only (PcmLooksBindable) is too loose across
// 0x3B0..0xE00 — random floats at +DefaultFOV would bind junk. Require a live
// POV, an owned DefaultFOV, or DefaultFOV plus an in-world root.
inline bool PcmLooksBindableFromProbe(uintptr_t cand, uintptr_t expectedPc)
{
	if (!is_valid_pointer_optimized(cand))
		return false;
	if (LooksLikePlayerCameraManager(cand, false))
		return true;
	if (expectedPc && LooksLikeOwnedPlayerCameraManager(cand, expectedPc))
		return true;
	if (!PcmLooksBindable(cand, expectedPc, false))
		return false;
	const uintptr_t root = ctx->read<uintptr_t>(cand + Offsets::Get().UObject.RootComponent);
	if (!is_valid_pointer_optimized(root))
		return false;
	return LooksValidWorldPos(GetSceneComponentWorldLocation(root));
}

inline MeshBoneContext PrepareMeshBoneContext(uintptr_t mesh, const Vector3& hintOrigin)
{
	MeshBoneContext best;
	if (!is_valid_pointer_optimized(mesh))
		return best;

	// Both halves of the double buffer are candidates now, not just the one the selector
	// points at. The selector maths cannot be confirmed from a log and two public
	// implementations compute it differently, so the choice is settled the same way the
	// c2w offset and array base already are: whichever produces something shaped like a
	// person wins. A mesh that resolves correctly today keeps the slot it has - the
	// primary carries a scoring bonus, so the alternate only displaces it when the
	// primary genuinely fails to describe a body.
	const BoneArrayPair arrays = GetBoneArrayPairCached(mesh);
	uint64_t raw = arrays.primary;
	if (!raw) {
		for (int i = 0; i < kBoneLodSlots && !raw; ++i)
			raw = arrays.slots[i];
	}
	if (!raw || !is_valid_pointer_optimized(raw))
		return best;

	const auto& sc = Offsets::Get().USceneComponent;

	struct LayoutSticky {
		uintptr_t c2wOff = 0;
		uint64_t baseOff = 0;
		uint64_t rawBase = 0;
		uint64_t untilMs = 0;
	};
	static std::unordered_map<uintptr_t, LayoutSticky> layoutCache;
	static std::mutex layoutMutex;

	auto buildCand = [&](uintptr_t c2wOff, uint64_t baseOff, uint64_t rawBase) -> MeshBoneContext {
		MeshBoneContext cand;
		if (!rawBase || !is_valid_pointer_optimized(rawBase))
			return cand;
		FTransform c2w = ctx->read<FTransform>(mesh + c2wOff);
		if (!IsValidFTransform(c2w))
			return cand;
		NormalizeFTransformScale(c2w);
		cand.boneArrayRaw = rawBase;
		cand.boneArray = rawBase + baseOff;
		cand.componentToWorld = c2w;
		cand.valid = is_valid_pointer_optimized(cand.boneArray);
		return cand;
	};

	auto scoreCand = [&](MeshBoneContext& cand) -> float {
		if (!cand.valid) return -1.e9f;
		if (!hintOrigin.IsZero() && LooksValidWorldPos(hintOrigin)) {
			const float meshToPawn = (float)cand.componentToWorld.translation.distance(hintOrigin);
			if (meshToPawn > 700.f)
				return -1.e9f;
		}
		const Vector3 head = GetBoneFromContext(cand, (int)Bones::Head);
		const Vector3 neck = GetBoneFromContext(cand, (int)Bones::neck_02);
		const Vector3 pelvisPos = GetBoneFromContext(cand, (int)Bones::pelvis);
		Vector3 foot = GetBoneFromContext(cand, (int)Bones::foot_l);
		if (!LooksValidWorldPos(foot))
			foot = GetBoneFromContext(cand, (int)Bones::foot_r);

		float score = ScoreBoneLayout(
			LooksValidWorldPos(head) ? head : neck,
			pelvisPos, foot, hintOrigin);
		if (score < 0.f) {
			float soft = -50.f;
			if (LooksValidWorldPos(pelvisPos) && !hintOrigin.IsZero() &&
				pelvisPos.distance(hintOrigin) < 200.0)
				soft += 30.f;
			if (LooksValidWorldPos(neck) && LooksValidWorldPos(pelvisPos) &&
				neck.distance(pelvisPos) > 10.0 && neck.distance(pelvisPos) < 150.0)
				soft += 20.f;
			if (LooksValidWorldPos(foot) && LooksValidWorldPos(pelvisPos) &&
				foot.distance(pelvisPos) > 15.0 && foot.distance(pelvisPos) < 200.0)
				soft += 15.f;
			score = soft;
		}
		return score;
	};

	const uint64_t nowMs = GetTickCount64();
	{
		std::lock_guard<std::mutex> lk(layoutMutex);
		auto lit = layoutCache.find(mesh);
		if (lit != layoutCache.end() && nowMs < lit->second.untilMs) {
			// The remembered slot may have swapped roles since it was chosen, so fall
			// back to whichever pointer still exists rather than dropping the mesh.
			uint64_t stickyRaw = lit->second.rawBase;
			bool stickyStillPresent = false;
			for (int i = 0; i < kBoneLodSlots; ++i)
				if (stickyRaw != 0 && stickyRaw == arrays.slots[i]) { stickyStillPresent = true; break; }
			if (!stickyStillPresent)
				stickyRaw = raw;
			MeshBoneContext sticky = buildCand(lit->second.c2wOff, lit->second.baseOff, stickyRaw);
			const float stickyScore = sticky.valid ? scoreCand(sticky) : -1.e9f;
			if (sticky.valid && stickyScore > -40.f) {
				sticky.chosenScore = stickyScore;
				for (int i = 0; i < kBoneLodSlots; ++i)
					if (arrays.slots[i]) sticky.slotsPresent++;
				return sticky;
			}
		}
	}

	// The sources disagree on ComponentToWorld, so all three reported bases stay in and the
	// geometry scoring decides. 0x370 leads because two posts agree on it and one of them
	// gives a self-consistent rotation/translation pair; 0x378 and 0x208 are the other two
	// reported values. A wrong base is not a near miss here - it takes the scale from
	// unrelated memory and stretches the whole pose - so scoring on geometry is what
	// distinguishes them.
	const uintptr_t c2wOffs[3] = { sc.ComponentToWorld, (uintptr_t)0x378, (uintptr_t)0x208 };
	// The descriptor points straight at bone 0 on this build, so transforms are at
	// boneArray + index * 0x60 with nothing added, and the logs already showed this winning
	// over the legacy +0x60 base. Keeping it as a single value drops a dimension from the
	// search below, so the scoring is only ever choosing between ComponentToWorld bases and
	// LOD slots rather than also arbitrating a base offset it could never distinguish.
	const uint64_t baseOffs[1] = { 0 };
	float bestScore = -1.e9f;
	uintptr_t bestC2wOff = sc.ComponentToWorld;
	uint64_t bestBaseOff = 0;
	uint64_t bestRawBase = raw;
	bool haveScored = false;

	for (uintptr_t c2wOff : c2wOffs) {
		for (uint64_t baseOff : baseOffs) {
			for (int slotIdx = 0; slotIdx < kBoneLodSlots; ++slotIdx) {
				const uint64_t rawBase = arrays.slots[slotIdx];
				if (!rawBase)
					continue;
				MeshBoneContext cand = buildCand(c2wOff, baseOff, rawBase);
				float score = scoreCand(cand);
				if (score <= -1.e8f)
					continue;
				if (c2wOff == sc.ComponentToWorld) score += 2.f;
				// Keeps every mesh that already resolves on the slot it resolves on
				// today. A different slot has to describe a body better, not merely
				// tie, before it takes over.
				if (rawBase == arrays.primary) score += 4.f;
				// A stripped rig from a coarser LOD loses to a full one when the
				// geometry scores are close.
				if (arrays.bones[slotIdx] > arrays.primaryBones) score += 2.f;

				if (!haveScored || score > bestScore) {
					bestScore = score;
					best = cand;
					bestC2wOff = c2wOff;
					bestBaseOff = baseOff;
					bestRawBase = rawBase;
					haveScored = true;
				}
			}
		}
	}

	if (!best.valid) {
		best = buildCand(sc.ComponentToWorld, 0, raw);
		bestC2wOff = sc.ComponentToWorld;
		bestBaseOff = 0;
		bestRawBase = raw;
	}

	best.chosenScore = haveScored ? bestScore : -1.e9f;
	for (int i = 0; i < kBoneLodSlots; ++i)
		if (arrays.slots[i]) best.slotsPresent++;

	if (best.valid) {
		std::lock_guard<std::mutex> lk(layoutMutex);
		layoutCache[mesh] = { bestC2wOff, bestBaseOff, bestRawBase, nowMs + 200 };
		if (layoutCache.size() > 512) {
			auto oldest = layoutCache.begin();
			for (auto jt = layoutCache.begin(); jt != layoutCache.end(); ++jt) {
				if (jt->second.untilMs < oldest->second.untilMs)
					oldest = jt;
			}
			layoutCache.erase(oldest);
		}
	}

	return best;
}

inline MeshBoneContext PrepareMeshBoneContext(uintptr_t mesh)
{
	return PrepareMeshBoneContext(mesh, Vector3{});
}

inline Vector3 GetBoneWithRotation(uintptr_t mesh, int bone_id)
{
	MeshBoneContext mbc = PrepareMeshBoneContext(mesh);
	if (!mbc.valid)
		return Vector3{};
	return GetBoneFromContext(mbc, bone_id);
}

// Coherence tolerances. Widened from the original 70/170/150/280/180/320 values —
// those were tuned against same-frame bone/pawn samples and didn't leave room for
// the async player-vitals poll (worldLoc) and the bone-array poll (skeleton) landing
// on different frames during fast movement/network lag, which caused legitimate
// players to fail "coherent"/"spine" and flicker. Weakspot bones (head-to-neck)
// stay tighter since that's a same-skeleton, same-frame structural check, not a
// cross-poll one.
static constexpr float kCoherentHeadNeckTolerance = 100.0f;   // was 70
static constexpr float kCoherentTopPelvisTolerance = 220.0f;  // was 170
static constexpr float kCoherentNeckPelvisTolerance = 200.0f; // was 150
static constexpr float kCoherentPelvisWorldLocTolerance = 550.0f; // was 280 - dominant cause of "coherent" flicker
static constexpr float kCoherentFootSpreadTolerance = 240.0f; // was 180
static constexpr float kCoherentSpanMin = 20.0f;
static constexpr float kCoherentSpanMax = 380.0f;             // was 320
// How far below the capsule origin the feet have to sit before a horizontally
// dominant pose is believed. Any real stance - standing, crouched, prone - keeps the
// capsule centre well above the soles; a pose that has been read with its axes
// rotated leaves them level with it. Deliberately far below the ~90cm a standing
// pawn reports so a shrunken crouch or prone capsule clears it untouched.
static constexpr float kCoherentMinFootDrop = 30.0f;

// Soft reject only obvious junk (wrong C2W / bone base). Allows crouch, prone, DBNO, missing Head.
//
// outReason names the specific rule that rejected the pose. "coherent" on its own says
// nothing actionable - there are seven separate ways to fail here and they have entirely
// different causes, from a bad decrypt to a legitimately unusual stance - so a player who
// is permanently missing a skeleton cannot be diagnosed without knowing which one fired.
inline bool IsCoherentPlayerSkeleton(const Vector3& head, const Vector3& neck, const Vector3& pelvis,
	const Vector3& footL, const Vector3& footR, const Vector3& worldLoc,
	const char** outReason = nullptr)
{
	auto reject = [&](const char* why) -> bool {
		if (outReason) *outReason = why;
		return false;
	};
	if (outReason) *outReason = "ok";

	if (!LooksValidWorldPos(neck) || !LooksValidWorldPos(pelvis))
		return reject("badSpineRead");

	// Head is optional — LOD / anim often leaves it bad while neck/limbs are fine.
	const Vector3 top = LooksValidWorldPos(head) ? head : neck;
	if (LooksValidWorldPos(head) && head.distance(neck) > kCoherentHeadNeckTolerance)
		return reject("headNeck");
	if (top.distance(pelvis) > kCoherentTopPelvisTolerance || neck.distance(pelvis) > kCoherentNeckPelvisTolerance)
		return reject("topPelvis");

	if (!worldLoc.IsZero() && LooksValidWorldPos(worldLoc)) {
		if (pelvis.distance(worldLoc) > kCoherentPelvisWorldLocTolerance)
			return reject("pelvisWorldLoc");
	}

	const bool hasL = LooksValidWorldPos(footL);
	const bool hasR = LooksValidWorldPos(footR);
	if (!hasL && !hasR)
		return reject("noFeet");

	Vector3 foot = hasL ? footL : footR;
	if (hasL && hasR) {
		if (footL.distance(footR) > kCoherentFootSpreadTolerance)
			return reject("footSpread");
		foot.x = (footL.x + footR.x) * 0.5;
		foot.y = (footL.y + footR.y) * 0.5;
		foot.z = (std::min)(footL.z, footR.z);
	}

	const float vert = (float)(top.z - foot.z);
	const float dx = (float)(top.x - foot.x);
	const float dy = (float)(top.y - foot.y);
	const float horiz = sqrtf(dx * dx + dy * dy);
	const float span = (float)top.distance(foot);
	if (span < kCoherentSpanMin || span > kCoherentSpanMax)
		return reject(span < kCoherentSpanMin ? "spanShort" : "spanLong");

	const bool horizDominant = horiz > vert * 1.75f;

	// Only reject classic sideways-mapped standing skeletons (prone is horiz-dominant and OK).
	if (vert > 55.f && horizDominant)
		return reject("sideways");

	// The rule above cannot see the worst case. It only fires once the pose still has
	// 55cm of vertical extent, so a read whose axes are fully rotated - head and feet
	// at the same height, the entire body laid out sideways - sails through with
	// vert near zero and gets drawn as a horizontal scribble inside an upright box.
	//
	// Height alone cannot separate that from a genuinely prone player, because both
	// are horizontally dominant. Where the feet sit relative to the capsule can: every
	// real stance keeps the capsule origin above the soles, while a rotated read puts
	// them level with it.
	if (horizDominant && !worldLoc.IsZero() && LooksValidWorldPos(worldLoc)) {
		if ((float)(worldLoc.z - foot.z) < kCoherentMinFootDrop)
			return reject("flatFeetLevel");
	}

	return true;
}

std::vector<int> GetArcWeakspotBones(const std::string& arcName) {
	// Expect display name (Wasp/Hornet/...) or raw class — match case-insensitively.
	std::string name = arcName;
	for (char& c : name)
		c = (char)tolower((unsigned char)c);

	auto has = [&](const char* s) { return name.find(s) != std::string::npos; };

	if (has("wasp") || has("lightdrone"))                           return { 5, 7, 6, 8 };
	if (has("snitch"))                                              return { 13, 14, 15 };
	if (has("hornet") || has("lightdrone_elite"))                   return { 11, 14 };
	if (has("rocketeer") || has("heavydrone_missile"))              return { 26, 36, 21, 31 };
	if (has("bombardier") || has("chonkplatform_mortar"))           return { 14, 46, 28, 34, 52, 40, 22 };
	if (has("sentinel") || has("c_sniper") || has("sniper"))        return { 6 };
	if (has("shredder") || has("c_runner"))                         return { 13, 12, 14 };
	if (has("bastion") || (has("chonk") && !has("mortar")))         return { 11, 39, 21, 27, 45, 33, 51 };
	if (has("tick"))                                                return { 3, 4, 5 };
	if (has("turret"))                                              return { 4, 5, 6 };
	if (has("fireball") || (has("flamethrower") && has("rollbot"))) return { 5, 6, 7 };
	if (has("pop") && has("roll"))                                  return { 5, 6, 7 };
	if (has("comet") || has("boom"))                                return { 5, 6, 8 };
	if (has("surveyor") || has("blockout"))                         return { 8, 9, 10 };
	if (has("leaper") || has("bullcrab"))                           return { 10, 12, 14 };
	if (has("spotter") || has("pinger"))                            return { 6, 7, 8 };
	if (has("firefly"))                                             return { 10, 12, 14 };
	if (has("vaporizer") || has("heavydrone_laser"))                return { 20, 24, 28 };
	if (has("queen") || has("peppermint"))                          return { 20, 24, 30 };
	if (has("matriarch") || has("spearmint"))                       return { 20, 24, 30 };
	return {};
}

// actorOrigin: ARC world location used to reject garbage bone samples.
std::unordered_map<int, Vector3> GetArcWeakspots(uintptr_t mesh, const std::string& arcName, const Vector3& actorOrigin) {
	std::unordered_map<int, Vector3> weakspots;

	if (!is_valid_pointer_optimized(mesh))
		return weakspots;

	MeshBoneContext mbc = PrepareMeshBoneContext(mesh);
	if (!mbc.valid)
		return weakspots;

	Vector3 origin = actorOrigin;
	if (origin.IsZero())
		origin = mbc.componentToWorld.translation;

	auto acceptBone = [&](const Vector3& pos) -> bool {
		if (pos.IsZero())
			return false;
		if (!std::isfinite(pos.x) || !std::isfinite(pos.y) || !std::isfinite(pos.z))
			return false;
		if (!origin.IsZero() && origin.distance(pos) > 600.0f)
			return false;
		return true;
	};

	std::vector<int> bones = GetArcWeakspotBones(arcName);
	for (int idx : bones) {
		Vector3 pos = GetBoneFromContext(mbc, idx);
		if (acceptBone(pos))
			weakspots[idx] = pos;
	}

	// If mapped indices are stale/empty, probe a bone range for distinct mesh points.
	bool distinct = false;
	for (const auto& [idx, pos] : weakspots) {
		(void)idx;
		if (!origin.IsZero() && origin.distance(pos) > 20.0f) {
			distinct = true;
			break;
		}
	}
	if (weakspots.empty() || !distinct) {
		weakspots.clear();
		for (int idx = 1; idx < 64; ++idx) {
			Vector3 pos = GetBoneFromContext(mbc, idx);
			if (!acceptBone(pos))
				continue;
			if (!origin.IsZero() && origin.distance(pos) < 15.0f)
				continue;
			weakspots[idx] = pos;
		}
	}

	return weakspots;
}

inline std::unordered_map<int, Vector3> GetArcWeakspots(uintptr_t mesh, const std::string& arcName) {
	return GetArcWeakspots(mesh, arcName, Vector3{});
}

inline ImU32 ToU32(float col[4]) {
	return ImGui::ColorConvertFloat4ToU32(ImVec4(col[0], col[1], col[2], col[3]));
}
ImU32 HueToImU32(float hue, float saturation = 1.f, float value = 1.f, int alpha = 255)
{
	float r, g, b;
	ImGui::ColorConvertHSVtoRGB(hue / 360.f, saturation, value, r, g, b);
	return IM_COL32((int)(r * 255), (int)(g * 255), (int)(b * 255), alpha);
}


struct UObject;
struct UGameInstance;
struct UWorld;
struct ULocalPlayer;
struct UPlayerController;
struct USceneComponent;
struct UIHoverData;
struct UInteractionComponent;
struct ULevel;
struct UWorldSettings;

struct UObject
{
	uintptr_t address;

	UObject() : address(0) {}
	UObject(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	std::wstring GetName() const;
	std::string GetNameString() const;
	bool IsDestroyed() const;
	USceneComponent GetRootComponent() const;
	UIHoverData GetUIHoverData() const;
	UInteractionComponent GetInteractionComponent() const;
};

struct UEngine
{
	uintptr_t address;

	UEngine() : address(0) {}
	UEngine(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	UGameInstance GetGameInstance() const;
};

struct UGameInstance
{
	uintptr_t address;

	UGameInstance() : address(0) {}
	UGameInstance(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	ULocalPlayer GetLocalPlayer() const;
};

struct UPawn;

struct UWorld
{
	uintptr_t address;

	UWorld() : address(0) {}
	UWorld(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	std::wstring GetName() const;
	std::string GetNameString() const;
	bool IsGameWorld() const;
	ULevel GetPersistentLevel() const;
	UGameInstance GetGameInstance() const;
	TArray GetLevels() const;
};

struct UPlayerController
{
	uintptr_t address;

	UPlayerController() : address(0) {}
	UPlayerController(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	UPawn GetAcknowledgedPawn() const;
};

struct ULocalPlayer
{
	uintptr_t address;

	ULocalPlayer() : address(0) {}
	ULocalPlayer(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	UPlayerController GetPlayerController() const
	{
		if (!IsValid())
			return UPlayerController(0);

		uintptr_t controller = ctx->read<uintptr_t>(address + Offsets::Get().ULocalPlayer.PlayerController);
		return UPlayerController(controller);
	}
};

struct USkeletalMeshComponent;
struct UHealthComponent;
struct UInteractionComponent;
struct UPlayerState;
struct UItemDataAsset;
struct UInventoryComponent;
struct UItemBase;
struct UItemActor;
struct UProjectileMovement;
struct UEngine;
struct UFlashlightComponent;
struct UAnimInstance;
struct UCrossLayerAnimationProperties;

struct UPawn
{
	uintptr_t address;

	UPawn() : address(0) {}
	UPawn(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	USceneComponent GetRootComponent() const;
	USkeletalMeshComponent GetMesh() const;
	UHealthComponent GetHealthComponent() const;
	UInteractionComponent GetInteractionComponent() const;
	UInventoryComponent GetInventoryComponent() const;
	UPlayerState GetPlayerState() const;
	UFlashlightComponent GetFlashlightComponent() const;
};

struct USceneComponent
{
	uintptr_t address;

	USceneComponent() : address(0) {}
	USceneComponent(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	Vector3 GetRelativeLocation() const;
	Vector3 GetComponentVelocity() const;
};

struct USkeletalMeshComponent
{
	uintptr_t address;

	USkeletalMeshComponent() : address(0) {}
	USkeletalMeshComponent(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	UAnimInstance GetAnimScriptInstance() const;
};

struct UAnimInstance
{
	uintptr_t address;

	UAnimInstance() : address(0) {}
	UAnimInstance(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	UCrossLayerAnimationProperties GetCrossLayerAnimationPropertiesRef() const;
};

struct UCrossLayerAnimationProperties
{
	uintptr_t address;

	UCrossLayerAnimationProperties() : address(0) {}
	UCrossLayerAnimationProperties(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	int32_t GetEquippedWeaponAmmoCount() const;
	bool IsZooming() const;
	bool IsSprinting() const;
	bool IsDodgeRolling() const;
	bool IsInJump() const;
	uint8_t GetCharacterStance() const;
	bool IsReloading() const;
	bool IsFiring() const;
};

struct UFlashlightComponent
{
	uintptr_t address;

	UFlashlightComponent() : address(0) {}
	UFlashlightComponent(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	FLinearColor GetFlashlightColor() const;
	void SetFlashlightColor(const FLinearColor& color) const;
};

struct UHealthComponent
{
	uintptr_t address;

	UHealthComponent() : address(0) {}
	UHealthComponent(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	double GetCurrentHealth() const;
	double GetMaxHealth() const;
	double GetCurrentArmor() const;
	double GetMaxArmor() const;
};

struct UInteractionComponent
{
	uintptr_t address;

	UInteractionComponent() : address(0) {}
	UInteractionComponent(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	bool IsOpened() const;
};

struct ULevel
{
	uintptr_t address;

	ULevel() : address(0) {}
	ULevel(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	UWorldSettings GetWorldSettings() const;
	TArray GetActorArray() const;
};

struct UWorldSettings
{
	uintptr_t address;

	UWorldSettings() : address(0) {}
	UWorldSettings(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	float GetWorldGravity() const;
};

struct UPlayerState
{
	uintptr_t address;

	UPlayerState() : address(0) {}
	UPlayerState(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	std::wstring GetSquadName() const;
	int32_t GetSquadSeed() const;
	std::string GetPlayerName() const;
	uintptr_t GetSquad() const;
	uint8_t GetPlayerPlatform() const;
};

enum class PlayerPlatform : uint8_t {
	Unknown = 0,
	Steam = 1,
	Xbox = 2,
	Psn = 3,
	Egs = 4
};

struct UItemDataAsset
{
	uintptr_t address;

	UItemDataAsset() : address(0) {}
	UItemDataAsset(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	uint8_t GetItemCategory() const;
	double GetWeight() const;
};

struct UInventoryComponent
{
	uintptr_t address;

	UInventoryComponent() : address(0) {}
	UInventoryComponent(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	TArray GetCurrentItemActors() const;
	TArray GetLocalCurrentItems() const;
	uintptr_t GetEquippedPrimaryItem() const;
	uintptr_t GetStowedWeapon0Actor() const;
	uintptr_t GetStowedToolActor() const;
	uintptr_t GetStowedWeapon0DataAsset() const;
	int32_t GetStowedWeapon0Quality() const;
	uintptr_t ResolveHeldItemActor(const char** outSrc = nullptr) const;
};

struct UItemBase
{
	uintptr_t address;

	UItemBase() : address(0) {}
	UItemBase(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	uintptr_t GetProjectileOverride() const;
};

struct UItemActor
{
	uintptr_t address;

	UItemActor() : address(0) {}
	UItemActor(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	int8_t GetWeaponQuality() const;
	TArray GetVisualizedMods() const;
};

struct UProjectileMovement
{
	uintptr_t address;

	UProjectileMovement() : address(0) {}
	UProjectileMovement(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	float GetInitialSpeed() const;
	float GetMaxSpeed() const;
	float GetProjectileGravity() const;
};

enum class EExtractionState : uint8_t
{
	Dormant = 0,
	Disabled = 1,
	Closed = 2,
	Closing = 3,
	Requested = 4,
	Arriving = 5,
	Ready = 6
};

struct ASalvageExtractionPointBase
{
	uintptr_t address;

	ASalvageExtractionPointBase() : address(0) {}
	ASalvageExtractionPointBase(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	bool GetCanExtract() const;
	bool GetIsEnabled() const;
	EExtractionState GetState() const;
	uintptr_t GetExtractionDataAsset() const;
	std::string GetDialogueTagName() const;
};

struct FGameplayTag
{
	uintptr_t address;

	FGameplayTag() : address(0) {}
	FGameplayTag(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	std::string GetTagNameString() const;
};

struct UExtractionDataAsset
{
	uintptr_t address;

	UExtractionDataAsset() : address(0) {}
	UExtractionDataAsset(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	FGameplayTag GetDialogueTag() const;
};

struct UIHoverData
{
	uintptr_t address;

	UIHoverData() : address(0) {}
	UIHoverData(uintptr_t addr) : address(addr) {}

	operator uintptr_t() const { return address; }
	operator bool() const { return is_valid_pointer_optimized(address); }

	bool IsValid() const { return is_valid_pointer_optimized(address); }
	std::string GetItemName() const;
	int32_t GetLootAmount() const;
	UItemDataAsset GetDataAsset() const;
};

inline ULocalPlayer UGameInstance::GetLocalPlayer() const
{
	if (!IsValid())
		return ULocalPlayer(0);

	TArray localPlayers{};
	ctx->read_array(address + Offsets::Get().UGameInstance.LocalPlayers, &localPlayers, sizeof(TArray));

	if (!localPlayers.Data || localPlayers.Count <= 0)
		return ULocalPlayer(0);

	uint64_t localPlayer = ctx->read<uint64_t>(localPlayers.Data);
	return ULocalPlayer(localPlayer);
}

inline UPawn UPlayerController::GetAcknowledgedPawn() const
{
	if (!IsValid())
		return UPawn(0);

	uintptr_t pawn = ctx->read<uintptr_t>(address + Offsets::Get().UPlayerController.AcknowledgedPawn);
	return UPawn(pawn);
}

inline ULocalPlayer get_localplayer(uintptr_t gameInstance)
{
	UGameInstance gi(gameInstance);
	return gi.GetLocalPlayer();
}



inline uint64_t GetLocalPlayer(uint64_t GameInstance)
{

	TArray localPlayers{};
	ctx->read_array(GameInstance + Offsets::Get().UGameInstance.LocalPlayers, &localPlayers, sizeof(TArray));

	if (!localPlayers.Data)
	{
		return 0;
	}

	if (localPlayers.Count <= 0)
	{
		return 0;
	}

	uint64_t localPlayer = ctx->read<uint64_t>(localPlayers.Data);

	return localPlayer;
}


inline std::wstring UObject::GetName() const
{
	if (!IsValid())
		return L"";
	return NameResolver::GetObjectName(ctx->image_base, address);
}

inline std::string UObject::GetNameString() const
{
	if (!IsValid())
		return "";
	return NameResolver::GetObjectNameA(ctx->image_base, address);
}

inline bool UObject::IsDestroyed() const
{
	if (!IsValid())
		return true;
	return ctx->read<bool>(address + Offsets::Get().UObject.IsDestroyed);
}

inline USceneComponent UObject::GetRootComponent() const
{
	if (!IsValid())
		return USceneComponent(0);
	uintptr_t root = ctx->read<uintptr_t>(address + Offsets::Get().UObject.RootComponent);
	return USceneComponent(root);
}

inline UIHoverData UObject::GetUIHoverData() const
{
	if (!IsValid())
		return UIHoverData(0);
	return UIHoverData(address + Offsets::Get().UObject.UIHoverData);
}

inline UInteractionComponent UObject::GetInteractionComponent() const
{
	if (!this->IsValid())
		return UInteractionComponent(0);
	uintptr_t interaction = ctx->read<uintptr_t>(this->address + Offsets::Get().UObject.InteractionComponent);
	return UInteractionComponent(interaction);
}

inline std::wstring UWorld::GetName() const
{
	if (!IsValid())
		return L"";
	return NameResolver::GetObjectName(ctx->image_base, address);
}

inline std::string UWorld::GetNameString() const
{
	if (!IsValid())
		return "";
	return NameResolver::GetObjectNameA(ctx->image_base, address);
}

inline bool UWorld::IsGameWorld() const
{
	return GetNameString() != X("MainMenu") && IsValid();
}

inline ULevel UWorld::GetPersistentLevel() const
{
	if (!IsValid())
		return ULevel(0);
	uintptr_t level = ctx->read<uintptr_t>(address + Offsets::Get().UWorld.PersistentLevel);
	return ULevel(level);
}

inline TArray UWorld::GetLevels() const
{
	TArray result{};
	if (!IsValid())
		return result;
	ctx->read_array(address + Offsets::Get().UWorld.Levels, &result, sizeof(TArray));
	return result;
}


inline USceneComponent UPawn::GetRootComponent() const
{
	if (!IsValid())
		return USceneComponent(0);
	uintptr_t root = ctx->read<uintptr_t>(address + Offsets::Get().UObject.RootComponent);
	return USceneComponent(root);
}

inline USkeletalMeshComponent UPawn::GetMesh() const
{
	if (!IsValid())
		return USkeletalMeshComponent(0);
	uintptr_t mesh = ctx->read<uintptr_t>(address + Offsets::Get().UPawn.Mesh);
	return USkeletalMeshComponent(mesh);
}

inline UHealthComponent UPawn::GetHealthComponent() const
{
	if (!IsValid())
		return UHealthComponent(0);
	uintptr_t health = ctx->read<uintptr_t>(address + Offsets::Get().UPawn.HealthComponent);
	return UHealthComponent(health);
}

inline UInteractionComponent UPawn::GetInteractionComponent() const
{
	if (!IsValid())
		return UInteractionComponent(0);
	uintptr_t interaction = ctx->read<uintptr_t>(address + Offsets::Get().UObject.InteractionComponent);
	return UInteractionComponent(interaction);
}

inline UPlayerState UPawn::GetPlayerState() const
{
	if (!IsValid())
		return UPlayerState(0);
	uintptr_t playerState = ctx->read<uintptr_t>(address + Offsets::Get().UPawn.PlayerState);
	return UPlayerState(playerState);
}

inline UFlashlightComponent UPawn::GetFlashlightComponent() const
{
	if (!IsValid())
		return UFlashlightComponent(0);
	uintptr_t flashlight = ctx->read<uintptr_t>(address + Offsets::Get().UPawn.FlashlightComponent);
	return UFlashlightComponent(flashlight);
}

inline Vector3 USceneComponent::GetRelativeLocation() const
{
	if (!IsValid())
		return Vector3(0, 0, 0);
	Vector3 pos = ctx->read<Vector3>(address + Offsets::Get().USceneComponent.RelativeLocation);
	if (_isnanf(pos.x) || _isnanf(pos.y) || _isnanf(pos.z))
		return Vector3(0, 0, 0);
	return pos;
}

inline Vector3 USceneComponent::GetComponentVelocity() const
{
	if (!IsValid())
		return Vector3(0, 0, 0);
	Vector3 vel = ctx->read<Vector3>(address + Offsets::Get().USceneComponent.ComponentVelocity);
	if (_isnanf(vel.x) || _isnanf(vel.y) || _isnanf(vel.z))
		return Vector3(0, 0, 0);
	return vel;
}

inline double UHealthComponent::GetCurrentHealth() const
{
	if (!IsValid())
		return 0.0;
	double val = ctx->read<double>(address + Offsets::Get().UHealthComponent.CurrentHealth);
	if (std::isnan(val) || val < 0.0 || val > 100000.0)
		return 0.0;
	return val;
}

inline double UHealthComponent::GetMaxHealth() const
{
	if (!IsValid())
		return 0.0;
	double val = ctx->read<double>(address + Offsets::Get().UHealthComponent.MaxHealth);
	if (std::isnan(val) || val < 0.0 || val > 100000.0)
		return 0.0;
	return val;
}

inline double UHealthComponent::GetCurrentArmor() const
{
	if (!IsValid())
		return 0.0;
	double val = ctx->read<double>(address + Offsets::Get().UHealthComponent.CurrentArmor);
	if (std::isnan(val) || val < 0.0 || val > 100000.0)
		return 0.0;
	return val;
}

inline double UHealthComponent::GetMaxArmor() const
{
	if (!IsValid())
		return 0.0;
	double val = ctx->read<double>(address + Offsets::Get().UHealthComponent.MaxArmor);
	if (std::isnan(val) || val < 0.0 || val > 100000.0)
		return 0.0;
	return val;
}

inline UAnimInstance USkeletalMeshComponent::GetAnimScriptInstance() const
{
	if (!IsValid())
		return UAnimInstance(0);
	uintptr_t animInstance = ctx->read<uintptr_t>(address + Offsets::Get().USkeletalMeshComponent.AnimScriptInstance);
	return UAnimInstance(animInstance);
}

inline UCrossLayerAnimationProperties UAnimInstance::GetCrossLayerAnimationPropertiesRef() const
{
	if (!IsValid())
		return UCrossLayerAnimationProperties(0);
	uintptr_t crossLayerProps = ctx->read<uintptr_t>(address + Offsets::Get().UAnimInstance.CrossLayerAnimationPropertiesRef);
	return UCrossLayerAnimationProperties(crossLayerProps);
}

inline int32_t UCrossLayerAnimationProperties::GetEquippedWeaponAmmoCount() const
{
	if (!IsValid())
		return 0;
	return ctx->read<int32_t>(address + Offsets::Get().UCrossLayerAnimationProperties.EquippedWeaponAmmoCount);
}

inline bool UCrossLayerAnimationProperties::IsZooming() const
{
	if (!IsValid())
		return false;
	return (ctx->read<uint8_t>(address + Offsets::Get().UCrossLayerAnimationProperties.bIsZooming) >> 0) & 1;
}

inline bool UCrossLayerAnimationProperties::IsSprinting() const
{
	if (!IsValid())
		return false;
	return (ctx->read<uint8_t>(address + Offsets::Get().UCrossLayerAnimationProperties.bIsSprinting) >> 0) & 1;
}

inline bool UCrossLayerAnimationProperties::IsDodgeRolling() const
{
	if (!IsValid())
		return false;
	return (ctx->read<uint8_t>(address + Offsets::Get().UCrossLayerAnimationProperties.bIsDodgeRolling) >> 0) & 1;
}

inline bool UCrossLayerAnimationProperties::IsInJump() const
{
	if (!IsValid())
		return false;
	return (ctx->read<uint8_t>(address + Offsets::Get().UCrossLayerAnimationProperties.bIsInJump) >> 0) & 1;
}

inline uint8_t UCrossLayerAnimationProperties::GetCharacterStance() const
{
	if (!IsValid())
		return 0;
	return ctx->read<uint8_t>(address + Offsets::Get().UCrossLayerAnimationProperties.CharacterStance);
}

inline bool UCrossLayerAnimationProperties::IsReloading() const
{
	if (!IsValid())
		return false;
	return ctx->read<uint8_t>(address + Offsets::Get().UCrossLayerAnimationProperties.bIsReloading) == 1;
}

inline bool UCrossLayerAnimationProperties::IsFiring() const
{
	if (!IsValid())
		return false;
	return ctx->read<uint8_t>(address + Offsets::Get().UCrossLayerAnimationProperties.bIsFiring) == 1;
}

struct CrossLayerBatch {
	int32_t ammoCount = 0;
	int32_t fireWeaponCount = 0;
	int32_t reloadActionIndex = 0;
	bool bIsZooming = false, bIsSprinting = false, bIsDodgeRolling = false, bIsInJump = false;
	uint8_t characterStance = 0;
	uint8_t locoMode = 0;
	uint8_t loopingReloadState = 0;
	uint8_t fireState = 0;
	bool rawIsReloading = false, rawIsFiring = false, rawIsSprinting = false;
	uint8_t rawZooming = 0;
	bool bIsTryingToSprint = false, bInSprintState = false;
	bool bIsReloading = false, bIsFiring = false;
	double timeSinceFire = -1.0;
	double gameTimeSeconds = -1.0;
	double startReloadTimeStamp = -1.0;
	double weaponReloadTime = -1.0;
	double timeSpentReloadingScaled = -1.0;
	// Raw byte, not a bool: the probe needs to see values other than 0/1 to
	// tell a wrong offset from a genuine flag.
	uint8_t rawWasRecentlyRendered = 0xFF;
	bool wasRecentlyRenderedValid = false;
	// 0x100..0x120 verbatim. The dump says this class stores one bool per byte,
	// so on a populated CLAP the cluster is visible directly and the flag can be
	// located by watching which byte tracks cover - no reliance on the derived
	// 0x10B or on the cross-build delta it came from.
	uint8_t boolWindow[32]{};
	bool boolWindowValid = false;
};

inline bool ReadCrossLayerBatch(uintptr_t crossAddr, CrossLayerBatch& out) {
	if (!crossAddr || !is_valid_pointer_optimized(crossAddr))
		return false;
	const auto& o = Offsets::Get().UCrossLayerAnimationProperties;

	auto consider = [](uintptr_t& minOff, uintptr_t& maxEnd, uintptr_t off, size_t nbytes) {
		if (!off || !nbytes)
			return;
		if (off < minOff)
			minOff = off;
		const uintptr_t end = off + (uintptr_t)nbytes;
		if (end > maxEnd)
			maxEnd = end;
	};

	uintptr_t minOff = ~uintptr_t(0);
	uintptr_t maxEnd = 0;
	consider(minOff, maxEnd, o.EquippedWeaponAmmoCount, 4);
	consider(minOff, maxEnd, o.bIsZooming, 1);
	consider(minOff, maxEnd, o.bIsSprinting, 1);
	consider(minOff, maxEnd, o.bIsDodgeRolling, 1);
	consider(minOff, maxEnd, o.bIsInJump, 1);
	consider(minOff, maxEnd, o.CharacterStance, 1);
	consider(minOff, maxEnd, o.bIsReloading, 1);
	consider(minOff, maxEnd, o.bIsFiring, 1);
	consider(minOff, maxEnd, o.LocoMode, 1);
	consider(minOff, maxEnd, o.LoopingReloadState, 1);
	consider(minOff, maxEnd, o.FireWeaponCount, 4);
	consider(minOff, maxEnd, o.FireState, 1);
	consider(minOff, maxEnd, o.TimeSinceFire, 8);
	consider(minOff, maxEnd, o.GameTimeSeconds, 8);
	consider(minOff, maxEnd, o.StartReloadTimeStamp, 8);
	consider(minOff, maxEnd, o.WeaponReloadTime, 8);
	consider(minOff, maxEnd, o.TimeSpentReloadingScaled, 8);
	consider(minOff, maxEnd, o.bIsTryingToSprint, 1);
	consider(minOff, maxEnd, o.CurrentReloadActionIndex, 4);
	consider(minOff, maxEnd, o.bWasRecentlyRendered, 1);
	if (minOff == ~uintptr_t(0) || maxEnd <= minOff)
		return false;

	const size_t size = (size_t)(maxEnd - minOff);
	if (size == 0 || size > 0xA80)
		return false;
	uint8_t buf[0xA80];
	if (!ctx->read_array(crossAddr + minOff, buf, size))
		return false;

	auto inRange = [&](uintptr_t off, size_t nbytes) -> bool {
		return off && off >= minOff && (off + (uintptr_t)nbytes) <= maxEnd;
	};
	auto rdU8 = [&](uintptr_t off) -> uint8_t {
		return inRange(off, 1) ? buf[off - minOff] : 0;
	};
	auto rdI32 = [&](uintptr_t off) -> int32_t {
		if (!inRange(off, 4))
			return 0;
		int32_t v = 0;
		memcpy(&v, buf + (off - minOff), 4);
		return v;
	};
	auto rdF64 = [&](uintptr_t off) -> double {
		if (!inRange(off, 8))
			return -1.0;
		double v = 0.0;
		memcpy(&v, buf + (off - minOff), 8);
		return v;
	};
	auto rdTime = [&](uintptr_t off) -> double {
		const double d = rdF64(off);
		if (std::isfinite(d) && d >= 0.0 && d < 1.0e7)
			return d;
		if (!inRange(off, 4))
			return d;
		float f = 0.f;
		memcpy(&f, buf + (off - minOff), 4);
		if (std::isfinite(f) && f >= 0.f && f < 1.0e7f)
			return (double)f;
		return d;
	};
	auto clapBool = [](uint8_t v) -> bool { return v == 1; };

	int32_t rawAmmo = rdI32(o.EquippedWeaponAmmoCount);
	out.ammoCount = (rawAmmo >= 0 && rawAmmo <= 9999) ? rawAmmo : -1;
	out.fireWeaponCount = rdI32(o.FireWeaponCount);
	out.rawZooming = rdU8(o.bIsZooming);
	// Exactly 1 = ADS. Accepting 1..7 painted Aiming on almost every player and
	// starved Jump/Dodge/Walk in the flag priority chain.
	out.bIsZooming = clapBool(out.rawZooming);
	out.bIsDodgeRolling = clapBool(rdU8(o.bIsDodgeRolling));
	out.bIsInJump = clapBool(rdU8(o.bIsInJump));
	{
		const uint8_t st = rdU8(o.CharacterStance);
		// ECharacterStance: 0 stand, 1 crouch, 2 prone. Anything else is a dead slot.
		out.characterStance = (st <= 2) ? st : 0;
	}
	out.locoMode = rdU8(o.LocoMode);
	out.loopingReloadState = rdU8(o.LoopingReloadState);
	out.fireState = rdU8(o.FireState);
	out.rawIsReloading = clapBool(rdU8(o.bIsReloading));
	out.rawIsFiring = clapBool(rdU8(o.bIsFiring));
	out.timeSinceFire = rdTime(o.TimeSinceFire);
	out.gameTimeSeconds = rdTime(o.GameTimeSeconds);
	out.startReloadTimeStamp = rdTime(o.StartReloadTimeStamp);
	out.weaponReloadTime = rdTime(o.WeaponReloadTime);
	out.timeSpentReloadingScaled = rdTime(o.TimeSpentReloadingScaled);
	out.reloadActionIndex = rdI32(o.CurrentReloadActionIndex);
	out.rawIsSprinting = clapBool(rdU8(o.bIsSprinting));
	out.bIsTryingToSprint = clapBool(rdU8(o.bIsTryingToSprint));
	if (inRange(o.bWasRecentlyRendered, 1)) {
		out.rawWasRecentlyRendered = rdU8(o.bWasRecentlyRendered);
		out.wasRecentlyRenderedValid = true;
	}
	constexpr uintptr_t kBoolWinBegin = 0x100;
	if (inRange(kBoolWinBegin, sizeof(out.boolWindow))) {
		memcpy(out.boolWindow, buf + (kBoolWinBegin - minOff), sizeof(out.boolWindow));
		out.boolWindowValid = true;
	}
	if (o.bInSprintState) {
		const uint8_t inSprint = ctx->read<uint8_t>(crossAddr + o.bInSprintState);
		if (ctx->last_status() == TransportStatus::Ok)
			out.bInSprintState = clapBool(inSprint);
	}

	// Debug 54: bIsSprinting@0x141 was 1 on every sample, LocoMode always Still,
	// TimeSinceFire/GameTime always 0. Do not use the stuck bit or the dead timers.
	// Sprint from trying/in-sprint/loco==Sprinting. Firing is filled by SlowCache
	// from FireWeaponCount deltas; leave bIsFiring false here.
	out.bIsSprinting = out.bIsTryingToSprint || out.bInSprintState || out.locoMode == 3;
	out.bIsFiring = false;
	const bool loopingReload = out.loopingReloadState >= 1 && out.loopingReloadState <= 3;
	const bool reloadStep = out.reloadActionIndex >= 1 && out.reloadActionIndex <= 12;
	out.bIsReloading = loopingReload || reloadStep;
	return true;
}

inline bool ReadMeshAnimationBatch(uintptr_t meshAddr, CrossLayerBatch& out) {
	if (!meshAddr || !is_valid_pointer_optimized(meshAddr))
		return false;
	uintptr_t animInstance = ctx->read<uintptr_t>(meshAddr + Offsets::Get().USkeletalMeshComponent.AnimScriptInstance);
	if (!animInstance || !is_valid_pointer_optimized(animInstance))
		return false;
	uintptr_t crossAddr = ctx->read<uintptr_t>(animInstance + Offsets::Get().UAnimInstance.CrossLayerAnimationPropertiesRef);
	return ReadCrossLayerBatch(crossAddr, out);
}

inline bool LocalPawnIsZooming(uintptr_t pawn)
{
	if (!is_valid_pointer_optimized(pawn))
		return false;
	const uintptr_t mesh = ctx->read<uintptr_t>(pawn + Offsets::Get().UPawn.Mesh);
	CrossLayerBatch batch{};
	return ReadMeshAnimationBatch(mesh, batch) && batch.bIsZooming;
}

// 3rd-person camera sits ~6m behind the pawn, so a leaked local avatar draws as
// "6m" ESP on your own body. Pointer identity has to cover AckPawn, Character,
// ViewTarget, and PawnPrivate/CurrentPawn - not just EntityCache's LocalPawn.
// Capture once per cache/render pass; per-entity DMA of these fields races the
// debug heap (and still missed self when LocalPawn lagged Character).
struct LocalAvatarIds {
	uintptr_t pawn = 0;
	uintptr_t ack = 0;
	uintptr_t ps = 0;
	uintptr_t pawnPrivate = 0;
	uintptr_t currentPawn = 0;
	uintptr_t pc = 0;
	uintptr_t pcPS = 0;
	uintptr_t character = 0;
	uintptr_t viewTarget = 0;
};

inline LocalAvatarIds CaptureLocalAvatarIds(uintptr_t localPawn, uintptr_t ackPawn,
	uintptr_t localPS, uintptr_t localPC = 0, uintptr_t pcm = 0)
{
	LocalAvatarIds id;
	if (is_valid_pointer_optimized(localPawn))
		id.pawn = localPawn;
	if (is_valid_pointer_optimized(ackPawn))
		id.ack = ackPawn;
	if (is_valid_pointer_optimized(localPS)) {
		id.ps = localPS;
		const auto& psOff = Offsets::Get().UPlayerState;
		const uintptr_t pp = ctx->read<uintptr_t>(localPS + psOff.PawnPrivate);
		if (is_valid_pointer_optimized(pp))
			id.pawnPrivate = pp;
		if (psOff.CurrentPawn) {
			const uintptr_t cp = ctx->read<uintptr_t>(localPS + psOff.CurrentPawn);
			if (is_valid_pointer_optimized(cp))
				id.currentPawn = cp;
		}
	}
	if (is_valid_pointer_optimized(localPC)) {
		id.pc = localPC;
		if (g_PlatformVersion != PlatformVersion::Xbox) {
			const uintptr_t pcPS = ctx->read<uintptr_t>(
				localPC + Offsets::Steam::UPlayerController_PlayerState);
			if (is_valid_pointer_optimized(pcPS))
				id.pcPS = pcPS;
			const uintptr_t ch = ctx->read<uintptr_t>(
				localPC + Offsets::Steam::UPlayerController_Character);
			if (is_valid_pointer_optimized(ch))
				id.character = ch;
		}
	}
	if (is_valid_pointer_optimized(pcm)) {
		const uintptr_t tgt = ctx->read<uintptr_t>(
			pcm + Offsets::Get().CameraManager.ViewTarget);
		if (is_valid_pointer_optimized(tgt))
			id.viewTarget = tgt;
	}
	return id;
}

inline bool MatchesLocalAvatar(uint64_t entity, uint64_t entityPS, const LocalAvatarIds& id)
{
	if (!is_valid_pointer_optimized(entity))
		return false;
	if (id.pawn && entity == id.pawn)
		return true;
	if (id.ack && entity == id.ack)
		return true;
	if (id.pawnPrivate && entity == id.pawnPrivate)
		return true;
	if (id.currentPawn && entity == id.currentPawn)
		return true;
	if (id.character && entity == id.character)
		return true;
	if (id.viewTarget && entity == id.viewTarget)
		return true;
	if (id.ps && is_valid_pointer_optimized(entityPS) && entityPS == id.ps)
		return true;
	if (id.pcPS && is_valid_pointer_optimized(entityPS) && entityPS == id.pcPS)
		return true;
	if (id.pc && g_PlatformVersion != PlatformVersion::Xbox) {
		const uintptr_t ctrl = ctx->read<uintptr_t>(
			entity + Offsets::Steam::UPawn_Controller);
		if (is_valid_pointer_optimized(ctrl) && ctrl == id.pc)
			return true;
	}
	return false;
}

inline bool IsLocalAvatar(uint64_t entity, uint64_t entityPS,
	uintptr_t localPawn, uintptr_t ackPawn, uintptr_t localPS,
	uintptr_t localPC = 0, uintptr_t pcm = 0)
{
	return MatchesLocalAvatar(entity, entityPS,
		CaptureLocalAvatarIds(localPawn, ackPawn, localPS, localPC, pcm));
}

// Single byte. ClipSize and VisualizedWeaponMods used to ride along in the same
// span, but the ammo fallback and mod pricing that consumed them are gone.
inline bool ReadItemActorWeaponQuality(uintptr_t itemActorAddr, int8_t& quality)
{
	if (!itemActorAddr || !is_valid_pointer_optimized(itemActorAddr))
		return false;
	const auto& o = Offsets::Get().UItemActor;
	if (!o.WeaponQuality)
		return false;
	const int8_t raw = ctx->read<int8_t>(itemActorAddr + o.WeaponQuality);
	if (ctx->last_status() != TransportStatus::Ok)
		return false;
	quality = raw;
	return true;
}

inline FLinearColor UFlashlightComponent::GetFlashlightColor() const
{
	if (!IsValid())
		return FLinearColor(1.0f, 1.0f, 1.0f, 1.0f);
	return ctx->read<FLinearColor>(address + Offsets::Get().UFlashlightComponent.FlashlightColor);
}

inline void UFlashlightComponent::SetFlashlightColor(const FLinearColor& color) const
{
	if (!IsValid())
		return;
	ctx->write_status<FLinearColor>(address + Offsets::Get().UFlashlightComponent.FlashlightColor, color);
}

inline bool UInteractionComponent::IsOpened() const
{
	if (!IsValid())
		return false;
	return ctx->read<bool>(address + Offsets::Get().UInteractionComponent.IsOpened);
}

inline bool LooksLikeSquadLabel(const std::string& s)
{
	if (s.empty() || s.size() > 32)
		return false;
	if (s == "NULL" || s == "None" || s == "none")
		return false;
	for (unsigned char c : s) {
		if (c < 32 || c > 126)
			return false;
	}
	return true;
}

inline std::string ReadFStringNarrow(uintptr_t addr)
{
	if (!addr || !is_valid_pointer_optimized(addr))
		return {};
	uintptr_t data = ctx->read<uintptr_t>(addr);
	int32_t count = ctx->read<int32_t>(addr + 8);
	if (!is_valid_pointer_optimized(data) || count <= 1 || count > 64)
		return {};
	wchar_t buf[65]{};
	const int n = count > 64 ? 64 : count;
	if (!ctx->read_array(data, buf, (size_t)n * sizeof(wchar_t)))
		return {};
	std::string out;
	out.reserve((size_t)n);
	for (int i = 0; i < n && buf[i]; ++i) {
		if (buf[i] > 0 && buf[i] < 128)
			out.push_back(static_cast<char>(buf[i]));
		else
			return {};
	}
	return out;
}

// Dump EmbarkSquad::Title is FText size 0x10, not FString. Loot hover already
// walks FText as data+0x30 / string+0x10; try that plus a few FString slots on
// the first pointer so a layout shift does not blank the label again.
inline std::string ReadFTextAt(uintptr_t textAddr)
{
	if (!textAddr || !is_valid_pointer_optimized(textAddr))
		return {};

	auto take = [](const std::string& s) -> std::string {
		return LooksLikeSquadLabel(s) ? s : std::string();
	};

	uintptr_t p0 = ctx->read<uintptr_t>(textAddr);
	if (is_valid_pointer_optimized(p0)) {
		uintptr_t details = ctx->read<uintptr_t>(p0 + 0x30);
		if (is_valid_pointer_optimized(details)) {
			std::string s = take(ReadFStringNarrow(details + 0x10));
			if (!s.empty())
				return s;
		}
		static const uintptr_t kStrOffs[] = { 0x00, 0x10, 0x18, 0x20, 0x28, 0x30 };
		for (uintptr_t off : kStrOffs) {
			std::string s = take(ReadFStringNarrow(p0 + off));
			if (!s.empty())
				return s;
		}
		uintptr_t sp = ctx->read<uintptr_t>(p0 + 0x18);
		if (is_valid_pointer_optimized(sp)) {
			wchar_t buf[64]{};
			if (ctx->read_array(sp, buf, sizeof(buf))) {
				std::string out;
				out.reserve(16);
				for (int i = 0; i < 63 && buf[i]; ++i) {
					if (buf[i] > 0 && buf[i] < 128)
						out.push_back(static_cast<char>(buf[i]));
					else
						break;
				}
				std::string s = take(out);
				if (!s.empty())
					return s;
			}
		}
	}

	return take(ReadFStringNarrow(textAddr));
}

inline std::wstring UPlayerState::GetSquadName() const
{
	if (!IsValid())
		return L"";
	uintptr_t squad = GetSquad();
	if (!is_valid_pointer_optimized(squad))
		return L"";

	const std::string title = ReadFTextAt(squad + Offsets::Get().UPlayerState.SquadName);
	if (title.empty())
		return L"";
	return std::wstring(title.begin(), title.end());
}

inline int32_t UPlayerState::GetSquadSeed() const
{
	if (!IsValid())
		return 0;
	uintptr_t squad = GetSquad();
	if (!is_valid_pointer_optimized(squad))
		return 0;
	if (g_PlatformVersion == PlatformVersion::Xbox)
		return 0;
	return ctx->read<int32_t>(squad + Offsets::Steam::UEmbarkSquad_Seed);
}

inline uint32_t rol32(uint32_t value, int32_t shift)
{
	return (value << shift) | (value >> (32 - shift));
}


inline bool LooksLikePlayerName(const std::string& s)
{
	if (s.empty() || s.size() > 64)
		return false;
	for (unsigned char c : s) {
		if (c < 32)
			return false;
	}
	return true;
}

inline std::string ReadPlayerNameFString(uintptr_t strAddr, bool decode)
{
	if (!strAddr)
		return "";
	uintptr_t bufPtr = ctx->read<uintptr_t>(strAddr);
	int32_t len = ctx->read<int32_t>(strAddr + 0x8);
	if (!validpointer(bufPtr) || len <= 0 || len > 64)
		return "";

	std::vector<uint16_t> w(static_cast<size_t>(len) + 1);
	if (!ctx->read_array(bufPtr, w.data(), static_cast<uint32_t>(len) * 2u))
		return "";
	w[len] = 0;

	if (decode)
		NameResolver::DecodeInPlace(w.data(), len);

	std::string out;
	out.reserve(static_cast<size_t>(len) * 3u);
	for (int i = 0; i < len; ++i) {
		const uint16_t c = w[i];
		if (c == 0)
			break;
		if (c <= 0x7F)
			out.push_back(static_cast<char>(c));
		else if (c <= 0x7FF) {
			out.push_back(static_cast<char>(0xC0 | (c >> 6)));
			out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
		} else {
			out.push_back(static_cast<char>(0xE0 | (c >> 12)));
			out.push_back(static_cast<char>(0x80 | ((c >> 6) & 0x3F)));
			out.push_back(static_cast<char>(0x80 | (c & 0x3F)));
		}
	}
	return out;
}

inline std::string UPlayerState::GetPlayerName() const
{
	if (!IsValid())
		return "";

	const uintptr_t configured = Offsets::Get().UPlayerState.PlayerName;
	const uintptr_t tries[] = { configured, configured + 0x18, 0x4A8 };

	std::string best;
	auto asciiRatio = [](const std::string& s) -> float {
		if (s.empty())
			return 0.f;
		int n = 0;
		for (unsigned char c : s)
			if (c < 128)
				++n;
		return (float)n / (float)s.size();
	};
	for (uintptr_t off : tries) {
		std::string decoded = ReadPlayerNameFString(address + off, true);
		std::string plain = ReadPlayerNameFString(address + off, false);
		const bool dOk = LooksLikePlayerName(decoded);
		const bool pOk = LooksLikePlayerName(plain);
		std::string chosen;
		if (dOk && pOk) {
			// Failed DecodeInPlace turns ASCII into high UTF-16; the plaintext
			// copy is the real name. A real encrypted wide name stays high after
			// decode and garbage as plaintext, so keep decoded then.
			if (asciiRatio(decoded) < 0.5f && asciiRatio(plain) >= 0.8f)
				chosen = std::move(plain);
			else
				chosen = std::move(decoded);
		} else if (dOk) {
			chosen = std::move(decoded);
		} else if (pOk) {
			chosen = std::move(plain);
		}
		if (!chosen.empty())
			return chosen;
	}
	return best;
}

inline uintptr_t UPlayerState::GetSquad() const
{
	if (!IsValid())
		return 0;
	return ctx->read<uintptr_t>(address + Offsets::Get().UPlayerState.Squad);
}

inline std::string PlatformAsciiLower(const uint8_t* bytes, int n)
{
	std::string s;
	if (!bytes || n <= 0)
		return s;
	bool utf16 = (n >= 4) && ((n & 1) == 0);
	if (utf16) {
		for (int i = 1; i < n; i += 2) {
			if (bytes[i] != 0) {
				utf16 = false;
				break;
			}
		}
	}
	if (utf16) {
		for (int i = 0; i + 1 < n; i += 2) {
			const unsigned char c = bytes[i];
			if (c >= 32 && c < 127)
				s.push_back((char)tolower(c));
		}
	} else {
		for (int i = 0; i < n; ++i) {
			const unsigned char c = bytes[i];
			if (c >= 32 && c < 127)
				s.push_back((char)tolower(c));
		}
	}
	return s;
}

inline uint8_t ClassifyPlatformTypeToken(const std::string& tok)
{
	if (tok == "steam" || tok == "steamid" || tok == "steam64")
		return (uint8_t)PlayerPlatform::Steam;
	if (tok == "xbl" || tok == "xbox" || tok == "xsx" || tok == "xss" || tok == "gdk"
		|| tok == "live" || tok == "xboxone" || tok == "xboxseries")
		return (uint8_t)PlayerPlatform::Xbox;
	if (tok == "psn" || tok == "ps5" || tok == "ps4" || tok == "ps3"
		|| tok == "playstation")
		return (uint8_t)PlayerPlatform::Psn;
	if (tok == "epic" || tok == "egs" || tok == "eos" || tok == "epicgames"
		|| tok == "productuser")
		return (uint8_t)PlayerPlatform::Egs;
	return (uint8_t)PlayerPlatform::Unknown;
}

inline std::string ReadAnsiPlatformFString(uintptr_t strAddr)
{
	if (!ctx || !strAddr)
		return "";
	const uintptr_t p = ctx->read<uintptr_t>(strAddr);
	const int32_t n = ctx->read<int32_t>(strAddr + 8);
	const int32_t m = ctx->read<int32_t>(strAddr + 12);
	if (!is_valid_pointer_optimized(p) || n <= 0 || n > 32)
		return "";
	if (m != 0 && (m < n || m > 64))
		return "";
	char buf[33]{};
	if (!ctx->read_array(p, buf, (size_t)n))
		return "";
	std::string s;
	s.reserve((size_t)n);
	for (int i = 0; i < n; ++i) {
		const unsigned char c = (unsigned char)buf[i];
		if (c == 0)
			break;
		if (c < 32 || c >= 127)
			return "";
		s.push_back((char)c);
	}
	return s;
}

// UniqueNetIdString keeps Type as inline UTF-16/ASCII after the vtable. Whole-object
// printable soup was what swapped Xbox/PSN between overlay clients; this only accepts
// a bounded token with a non-alpha neighbor (or NUL / ':').
inline uint8_t ScanInlinePlatformTokens(uintptr_t idObj)
{
	if (!ctx || !is_valid_pointer_optimized(idObj))
		return (uint8_t)PlayerPlatform::Unknown;

	uint8_t b[192]{};
	if (!ctx->read_array(idObj, b, sizeof(b)))
		return (uint8_t)PlayerPlatform::Unknown;

	static const char* kToks[] = {
		"steamid", "steam", "playstation", "xboxone", "xboxseries",
		"xbox", "xbl", "xsx", "xss", "gdk", "psn", "ps5", "ps4",
		"epicgames", "epic", "egs", "eos"
	};

	auto asciiAt = [&](int i, const char* tok) -> bool {
		const int L = (int)strlen(tok);
		if (i < 8 || i + L > (int)sizeof(b))
			return false;
		if (i > 0) {
			const unsigned char prev = b[i - 1];
			if ((prev >= 'A' && prev <= 'Z') || (prev >= 'a' && prev <= 'z'))
				return false;
		}
		for (int k = 0; k < L; ++k) {
			const unsigned char c = b[i + k];
			const unsigned char t = (unsigned char)tok[k];
			if (tolower(c) != t)
				return false;
		}
		if (i + L < (int)sizeof(b)) {
			const unsigned char next = b[i + L];
			if ((next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z'))
				return false;
		}
		return true;
	};
	auto utf16At = [&](int i, const char* tok) -> bool {
		const int L = (int)strlen(tok);
		if (i < 8 || i + L * 2 > (int)sizeof(b))
			return false;
		if (i >= 2 && b[i - 1] == 0) {
			const unsigned char prev = b[i - 2];
			if ((prev >= 'A' && prev <= 'Z') || (prev >= 'a' && prev <= 'z'))
				return false;
		}
		for (int k = 0; k < L; ++k) {
			if (b[i + k * 2 + 1] != 0)
				return false;
			if (tolower(b[i + k * 2]) != (unsigned char)tok[k])
				return false;
		}
		const int end = i + L * 2;
		if (end + 1 < (int)sizeof(b) && b[end + 1] == 0) {
			const unsigned char next = b[end];
			if ((next >= 'A' && next <= 'Z') || (next >= 'a' && next <= 'z'))
				return false;
		}
		return true;
	};

	uint8_t egs = (uint8_t)PlayerPlatform::Unknown;
	for (int i = 8; i < (int)sizeof(b) - 2; ++i) {
		for (const char* tok : kToks) {
			if (!asciiAt(i, tok) && !utf16At(i, tok))
				continue;
			const uint8_t p = ClassifyPlatformTypeToken(tok);
			if (p == (uint8_t)PlayerPlatform::Steam || p == (uint8_t)PlayerPlatform::Xbox
				|| p == (uint8_t)PlayerPlatform::Psn)
				return p;
			if (p == (uint8_t)PlayerPlatform::Egs)
				egs = p;
		}
	}
	return egs;
}

inline uint8_t ClassifyPlatformTypeString(const std::string& raw)
{
	if (raw.empty() || raw.size() > 96)
		return (uint8_t)PlayerPlatform::Unknown;

	std::string s;
	s.reserve(raw.size());
	for (unsigned char c : raw) {
		if (c >= 'A' && c <= 'Z')
			s.push_back((char)(c - 'A' + 'a'));
		else if (c >= 32 && c < 127)
			s.push_back((char)c);
	}
	if (s.find("7656119") != std::string::npos)
		return (uint8_t)PlayerPlatform::Steam;

	{
		bool digits = !s.empty() && s.size() >= 15 && s.size() <= 20;
		for (char c : s) {
			if (c < '0' || c > '9') { digits = false; break; }
		}
		if (digits) {
			uint64_t v = 0;
			bool ov = false;
			for (char c : s) {
				if (v > (~0ull / 10ull)) { ov = true; break; }
				v = v * 10ull + (uint64_t)(c - '0');
			}
			if (!ov) {
				if ((v >> 48) == 0x0009ull)
					return (uint8_t)PlayerPlatform::Xbox;
				if (s.size() >= 18)
					return (uint8_t)PlayerPlatform::Psn;
				if (s.size() >= 15 && (v >> 48) != 0x0110ull)
					return (uint8_t)PlayerPlatform::Psn;
			}
		}
	}

	// FUniqueNetIdEOSPlus::ToString is PlatformNetId_+_EpicAccountId|PUID.
	{
		const auto plus = s.find("_+_");
		if (plus != std::string::npos && plus > 0 && plus <= 32) {
			const std::string prefix = s.substr(0, plus);
			const uint8_t prefTok = ClassifyPlatformTypeToken(prefix);
			if (prefTok == (uint8_t)PlayerPlatform::Steam || prefTok == (uint8_t)PlayerPlatform::Xbox
				|| prefTok == (uint8_t)PlayerPlatform::Psn)
				return prefTok;
			bool digits = !prefix.empty();
			for (char c : prefix) {
				if (c < '0' || c > '9') { digits = false; break; }
			}
			if (digits && prefix.size() >= 15 && prefix.size() <= 20) {
				uint64_t v = 0;
				bool ov = false;
				for (char c : prefix) {
					if (v > (~0ull / 10ull)) { ov = true; break; }
					v = v * 10ull + (uint64_t)(c - '0');
				}
				if (!ov && (v >> 48) == 0x0009ull)
					return (uint8_t)PlayerPlatform::Xbox;
			}
		}
	}

	std::string type = s;
	const auto colon = s.find(':');
	if (colon != std::string::npos && colon > 0 && colon <= 16)
		type = s.substr(0, colon);

	uint8_t direct = ClassifyPlatformTypeToken(type);
	if (direct != (uint8_t)PlayerPlatform::Unknown)
		return direct;

	std::string tok;
	auto flush = [&]() -> uint8_t {
		const uint8_t p = ClassifyPlatformTypeToken(tok);
		tok.clear();
		return p;
	};
	for (char c : s) {
		if ((c >= 'a' && c <= 'z') || (c >= '0' && c <= '9'))
			tok.push_back(c);
		else {
			const uint8_t p = flush();
			if (p != (uint8_t)PlayerPlatform::Unknown)
				return p;
		}
	}
	return flush();
}

inline uint8_t ClassifyPlatformFStringAt(uintptr_t strAddr)
{
	uint8_t p = ClassifyPlatformTypeString(ReadPlayerNameFString(strAddr, false));
	if (p != (uint8_t)PlayerPlatform::Unknown)
		return p;
	return ClassifyPlatformTypeString(ReadAnsiPlatformFString(strAddr));
}

// ID payload only: SteamID64 / XUID / type string. Do not run this over a 128-byte
// UniqueNetId object — printable bytes from pointers collide into "xbox"/"psn"
// and two overlay clients then cache different badges for the same pawn.
inline uint8_t ClassifyPlayerPlatformBlob(const uint8_t* bytes, int n)
{
	if (!bytes || n <= 0 || n > 64)
		return (uint8_t)PlayerPlatform::Unknown;

	if (n == 8) {
		uint64_t v = 0;
		memcpy(&v, bytes, 8);
		const uint32_t hi = (uint32_t)(v >> 32);
		if (hi == 0x01100001u || hi == 0x01100002u)
			return (uint8_t)PlayerPlatform::Steam;
		if ((v >> 48) == 0x0009ull && v > 0x0009000000000000ull)
			return (uint8_t)PlayerPlatform::Xbox;
	}

	return ClassifyPlatformTypeString(PlatformAsciiLower(bytes, n));
}

inline uint8_t ClassifyPlayerPlatformText(const std::string& s)
{
	return ClassifyPlatformTypeString(s);
}

inline uint8_t ClassifyPlayerPlatformFName(uint32_t comparisonIndex)
{
	if (!ctx || !comparisonIndex || comparisonIndex > 0x00FFFFFF)
		return (uint8_t)PlayerPlatform::Unknown;
	std::wstring w = DecryptFNameEntry(ctx->image_base, comparisonIndex);
	if (w.empty())
		return (uint8_t)PlayerPlatform::Unknown;
	std::string a;
	a.reserve(w.size());
	for (wchar_t c : w) {
		if (c >= 32 && c < 127)
			a.push_back((char)tolower((unsigned char)c));
	}
	return ClassifyPlayerPlatformText(a);
}

inline bool ReadPlatformTArray(uintptr_t arrAddr, uint8_t* buf, int bufCap, int& outN)
{
	outN = 0;
	if (!ctx || !buf || bufCap <= 0)
		return false;
	TArray arr = ctx->read<TArray>(arrAddr);
	if (!is_valid_pointer_optimized(arr.Data))
		return false;
	if (arr.Count <= 0 || arr.Count > bufCap)
		return false;
	if (arr.Max != 0 && arr.Max < arr.Count)
		return false;
	if (!ctx->read_array(arr.Data, buf, (size_t)arr.Count))
		return false;
	outN = arr.Count;
	return true;
}

inline uint8_t PreferPlayerPlatform(uint8_t a, uint8_t b);

inline bool PtrInGameImage(uint64_t p)
{
	if (!ctx || !ctx->image_base || p < ctx->image_base)
		return false;
	return (p - ctx->image_base) < 0x20000000ull;
}

// Live UniqueNetId vfptrs are stored as ImageBase=0x140000000 values (PlatDiag
// idhex 40073C4C / 701DFB4B). Rebase onto the actual PioneerGame mapping.
inline uint64_t ResolveGameImagePtr(uint64_t p)
{
	if (!p)
		return 0;
	if (PtrInGameImage(p))
		return p;
	if (ctx && ctx->image_base && (p >> 32) == 1ull && p >= 0x140000000ull) {
		const uint64_t live = ctx->image_base + (p - 0x140000000ull);
		if (PtrInGameImage(live))
			return live;
	}
	return 0;
}

inline uint8_t ClassifyRttiTypeName(const std::string& raw)
{
	if (raw.empty())
		return (uint8_t)PlayerPlatform::Unknown;
	std::string s;
	s.reserve(raw.size());
	for (unsigned char c : raw) {
		if (c >= 'A' && c <= 'Z')
			s.push_back((char)(c - 'A' + 'a'));
		else if (c >= 32 && c < 127)
			s.push_back((char)c);
	}
	if (s.find("steam") != std::string::npos)
		return (uint8_t)PlayerPlatform::Steam;
	if (s.find("playstation") != std::string::npos || s.find("psn") != std::string::npos
		|| s.find("ps5") != std::string::npos || s.find("ps4") != std::string::npos)
		return (uint8_t)PlayerPlatform::Psn;
	if (s.find("xbox") != std::string::npos || s.find("xbl") != std::string::npos
		|| s.find("gdk") != std::string::npos || s.find("xsapi") != std::string::npos)
		return (uint8_t)PlayerPlatform::Xbox;
	if (s.find("eos") != std::string::npos || s.find("epic") != std::string::npos
		|| s.find("egs") != std::string::npos)
		return (uint8_t)PlayerPlatform::Egs;
	return (uint8_t)PlayerPlatform::Unknown;
}

inline std::string ReadMsvcRttiTypeNameFromVtable(uint64_t vtable)
{
	vtable = ResolveGameImagePtr(vtable);
	if (!ctx || !vtable)
		return {};
	const uint64_t colRaw = ctx->read<uint64_t>(vtable - 8);
	const uint64_t col = ResolveGameImagePtr(colRaw);
	if (!col)
		return {};
	const uint32_t typeRva = ctx->read<uint32_t>(col + 12);
	if (!typeRva)
		return {};
	const uint64_t typeDesc = ctx->image_base + typeRva;
	if (!PtrInGameImage(typeDesc))
		return {};
	char name[81]{};
	if (!ctx->read_array(typeDesc + 16, name, 80))
		return {};
	if (name[0] != '.' && name[0] != '?')
		return {};
	std::string out;
	out.reserve(48);
	for (int i = 0; i < 80 && name[i]; ++i) {
		const unsigned char c = (unsigned char)name[i];
		if (c < 32 || c >= 127)
			break;
		out.push_back((char)c);
	}
	return out;
}

inline uint8_t ClassifyModuleTypePointer(uint64_t p)
{
	if (!PtrInGameImage(p))
		return (uint8_t)PlayerPlatform::Unknown;
	uint8_t raw[32]{};
	if (!ctx->read_array(p, raw, sizeof(raw)))
		return (uint8_t)PlayerPlatform::Unknown;
	return ClassifyPlatformTypeString(PlatformAsciiLower(raw, (int)sizeof(raw)));
}

inline uint8_t ClassifyVtableRtti(uint64_t vtable)
{
	struct Ent { uint64_t vt; uint8_t plat; };
	static Ent cache[16]{};
	static int n = 0;
	for (int i = 0; i < n; ++i) {
		if (cache[i].vt == vtable)
			return cache[i].plat;
	}
	const uint8_t p = ClassifyRttiTypeName(ReadMsvcRttiTypeNameFromVtable(vtable));
	if (p != (uint8_t)PlayerPlatform::Unknown && n < 16) {
		cache[n].vt = vtable;
		cache[n].plat = p;
		++n;
	}
	return p;
}

// Live PioneerGame UniqueNetId vfptr low 32 (PlatDiag idhex). Shared console
// class 0x4BFB1D70 is EOSPlus (Xbox and PSN together) — never map it to one badge.
inline uint8_t ClassifyUniqueNetIdVtable(uint64_t vtable)
{
	switch ((uint32_t)vtable) {
	case 0x4C3C0740u: return (uint8_t)PlayerPlatform::Steam;
	case 0x4C166E70u:
	case 0x4C1710C0u: return (uint8_t)PlayerPlatform::Xbox;
	default: return (uint8_t)PlayerPlatform::Unknown;
	}
}

inline bool IsConsoleEosPlusVtable(uint64_t vtable)
{
	return (uint32_t)vtable == 0x4BFB1D70u;
}

inline uint8_t ClassifyHeapTypePointer(uint64_t p)
{
	if (!ctx || !is_valid_pointer_optimized(p))
		return (uint8_t)PlayerPlatform::Unknown;
	if (PtrInGameImage(p))
		return ClassifyModuleTypePointer(p);
	uint8_t raw[32]{};
	if (!ctx->read_array(p, raw, sizeof(raw)))
		return (uint8_t)PlayerPlatform::Unknown;
	const std::string s = PlatformAsciiLower(raw, (int)sizeof(raw));
	if (s.empty() || s.size() > 20)
		return (uint8_t)PlayerPlatform::Unknown;
	return ClassifyPlatformTypeString(s);
}

inline bool ReadPlatformLooseBytes(uintptr_t arrAddr, uint8_t* buf, int bufCap, int& outN)
{
	outN = 0;
	if (!ctx || !buf || bufCap <= 0)
		return false;
	TArray arr = ctx->read<TArray>(arrAddr);
	if (!is_valid_pointer_optimized(arr.Data))
		return false;
	if (arr.Count <= 0 || arr.Count > bufCap)
		return false;
	if (!ctx->read_array(arr.Data, buf, (size_t)arr.Count))
		return false;
	outN = arr.Count;
	return true;
}

// FUniqueNetIdSteam is vtable + inline SteamID64. Console wrappers may store a
// .rdata Type pointer or an XUID at +8/+16. Do not soup-scan the object.
inline uint8_t ClassifyUniqueNetIdIdentity(uintptr_t idObj)
{
	if (!ctx || !is_valid_pointer_optimized(idObj))
		return (uint8_t)PlayerPlatform::Unknown;

	uint8_t head[96]{};
	if (!ctx->read_array(idObj, head, sizeof(head)))
		return (uint8_t)PlayerPlatform::Unknown;

	uint64_t vtable = 0;
	memcpy(&vtable, head, 8);

	uint8_t best = ClassifyUniqueNetIdVtable(vtable);
	if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
		|| best == (uint8_t)PlayerPlatform::Psn)
		return best;

	for (int off = 8; off + 8 <= (int)sizeof(head); off += 8)
		best = PreferPlayerPlatform(best, ClassifyPlayerPlatformBlob(head + off, 8));
	if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
		|| best == (uint8_t)PlayerPlatform::Psn)
		return best;

	best = PreferPlayerPlatform(best, ClassifyVtableRtti(vtable));
	if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
		|| best == (uint8_t)PlayerPlatform::Psn)
		return best;

	for (int off = 8; off + 8 <= (int)sizeof(head); off += 8) {
		uint64_t q = 0;
		memcpy(&q, head + off, 8);
		best = PreferPlayerPlatform(best, ClassifyHeapTypePointer(q));
		if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
			|| best == (uint8_t)PlayerPlatform::Psn)
			return best;
	}
	return best;
}

inline uint8_t ParseFUniqueNetIdObject(uintptr_t idObj, int depth = 0)
{
	if (!ctx || !is_valid_pointer_optimized(idObj) || depth > 2)
		return (uint8_t)PlayerPlatform::Unknown;

	uint8_t best = ClassifyUniqueNetIdIdentity(idObj);
	if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
		|| best == (uint8_t)PlayerPlatform::Psn)
		return best;

	const uintptr_t strOffs[] = { 0x08, 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40, 0x48 };
	for (uintptr_t strOff : strOffs) {
		best = PreferPlayerPlatform(best, ClassifyPlatformFStringAt(idObj + strOff));
		if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
			|| best == (uint8_t)PlayerPlatform::Psn)
			return best;
	}

	uint8_t buf[64]{};
	int n = 0;
	const uintptr_t arrOffs[] = { 0x10, 0x18, 0x20, 0x28, 0x30, 0x38, 0x40 };
	for (uintptr_t off : arrOffs) {
		if (!ReadPlatformTArray(idObj + off, buf, 64, n) && !ReadPlatformLooseBytes(idObj + off, buf, 64, n))
			continue;
		best = PreferPlayerPlatform(best, ClassifyPlayerPlatformBlob(buf, n));
		if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
			|| best == (uint8_t)PlayerPlatform::Psn)
			return best;
	}

	best = PreferPlayerPlatform(best, ScanInlinePlatformTokens(idObj));
	if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
		|| best == (uint8_t)PlayerPlatform::Psn)
		return best;

	// FUniqueNetIdEOSPlus keeps the native XBL/PSN UniqueNetId as a TSharedPtr
	// in the object (not at PlatformId Repl+0). Follow heap pointers only.
	uint8_t head[96]{};
	uint64_t selfVt = 0;
	const bool gotHead = ctx->read_array(idObj, head, sizeof(head));
	if (gotHead)
		memcpy(&selfVt, head, 8);
	if (depth < 2 && gotHead && (depth > 0 || IsConsoleEosPlusVtable(selfVt))) {
		for (int off = 8; off + 8 <= (int)sizeof(head); off += 8) {
			uint64_t q = 0;
			memcpy(&q, head + off, 8);
			if (q == idObj || q == selfVt)
				continue;
			if (!is_valid_pointer_optimized(q) || PtrInGameImage(q))
				continue;
			best = PreferPlayerPlatform(best, ParseFUniqueNetIdObject(q, depth + 1));
			if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
				|| best == (uint8_t)PlayerPlatform::Psn)
				return best;
		}
	}

	return best;
}

inline uint8_t ParseUniqueNetIdRepl(uintptr_t replAddr)
{
	if (!ctx || !is_valid_pointer_optimized(replAddr))
		return (uint8_t)PlayerPlatform::Unknown;

	uint8_t best = ParseFUniqueNetIdObject(ctx->read<uintptr_t>(replAddr));
	if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
		|| best == (uint8_t)PlayerPlatform::Psn)
		return best;

	uint8_t replHead[48]{};
	if (ctx->read_array(replAddr, replHead, sizeof(replHead))) {
		for (int off = 8; off + 8 <= (int)sizeof(replHead); off += 8) {
			uint64_t q = 0;
			memcpy(&q, replHead + off, 8);
			best = PreferPlayerPlatform(best, ClassifyHeapTypePointer(q));
			if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
				|| best == (uint8_t)PlayerPlatform::Psn)
				return best;
		}
	}

	const uintptr_t strOffs[] = { 0x10, 0x18, 0x20, 0x28, 0x30 };
	for (uintptr_t off : strOffs) {
		best = PreferPlayerPlatform(best, ClassifyPlatformFStringAt(replAddr + off));
		if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
			|| best == (uint8_t)PlayerPlatform::Psn)
			return best;
	}

	uint8_t buf[64]{};
	int n = 0;
	const uintptr_t tarrayOffs[] = { 0x20, 0x18, 0x10, 0x28 };
	for (uintptr_t off : tarrayOffs) {
		if (!ReadPlatformTArray(replAddr + off, buf, 64, n) && !ReadPlatformLooseBytes(replAddr + off, buf, 64, n))
			continue;
		best = PreferPlayerPlatform(best, ClassifyPlayerPlatformBlob(buf, n));
		if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
			|| best == (uint8_t)PlayerPlatform::Psn)
			return best;
	}

	best = PreferPlayerPlatform(best, ParseFUniqueNetIdObject(ctx->read<uintptr_t>(replAddr)));
	if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
		|| best == (uint8_t)PlayerPlatform::Psn)
		return best;
	best = PreferPlayerPlatform(best, ParseFUniqueNetIdObject(ctx->read<uintptr_t>(replAddr + 8)));
	return best;
}

inline uint8_t PreferPlayerPlatform(uint8_t a, uint8_t b)
{
	auto rank = [](uint8_t p) -> int {
		if (p == (uint8_t)PlayerPlatform::Steam || p == (uint8_t)PlayerPlatform::Xbox
			|| p == (uint8_t)PlayerPlatform::Psn)
			return 2;
		if (p == (uint8_t)PlayerPlatform::Egs)
			return 1;
		return 0;
	};
	return rank(b) > rank(a) ? b : a;
}

inline uint8_t ScanBytesForPlatform(uintptr_t addr, int size)
{
	if (!ctx || !is_valid_pointer_optimized(addr) || size <= 0 || size > 512)
		return (uint8_t)PlayerPlatform::Unknown;
	uint8_t blob[512]{};
	if (!ctx->read_array(addr, blob, (size_t)size))
		return (uint8_t)PlayerPlatform::Unknown;
	return ClassifyPlayerPlatformBlob(blob, size);
}

inline uint8_t UPlayerState::GetPlayerPlatform() const
{
	if (!IsValid() || !ctx)
		return (uint8_t)PlayerPlatform::Unknown;

	const bool xbox = (g_PlatformVersion == PlatformVersion::Xbox);
	const uintptr_t uniqueOff = xbox
		? Offsets::Xbox::UPlayerState_UniqueId
		: Offsets::Steam::UPlayerState_UniqueId;
	const uintptr_t compOff = xbox
		? Offsets::Xbox::UPlayerState_PlatformIdComponent
		: Offsets::Steam::UPlayerState_PlatformIdComponent;
	const uintptr_t platOff = xbox
		? Offsets::Xbox::UEmbarkPlatformIdComponent_PlatformId
		: Offsets::Steam::UEmbarkPlatformIdComponent_PlatformId;

	uint8_t best = (uint8_t)PlayerPlatform::Unknown;
	const uintptr_t uniqueAlts[] = { uniqueOff, uniqueOff + 0x18 };
	const uintptr_t compAlts[] = { compOff, (uintptr_t)(compOff == 0x4A0 ? 0x488 : compOff - 0x18) };
	// CL-1372005 PlatformId @ 0x188; keep prior live alts as fallbacks.
	const uintptr_t platAlts[] = { platOff, 0x188, 0x150, 0x168, 0x180, 0x138, 0x140, 0x148, 0x158, 0x160 };

	uintptr_t usedComp = 0;
	uintptr_t usedPlatOff = 0x150;
	for (uintptr_t off : compAlts) {
		if (!off)
			continue;
		const uintptr_t comp = ctx->read<uintptr_t>(address + off);
		if (!is_valid_pointer_optimized(comp))
			continue;
		if (usedComp == 0)
			usedComp = comp;
		for (uintptr_t pOff : platAlts) {
			if (!pOff)
				continue;
			const uint8_t got = ParseUniqueNetIdRepl(comp + pOff);
			if (got != (uint8_t)PlayerPlatform::Unknown) {
				best = PreferPlayerPlatform(best, got);
				usedPlatOff = pOff;
			}
			if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
				|| best == (uint8_t)PlayerPlatform::Psn)
				break;
		}
		if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
			|| best == (uint8_t)PlayerPlatform::Psn)
			break;
	}
	for (uintptr_t off : uniqueAlts) {
		best = PreferPlayerPlatform(best, ParseUniqueNetIdRepl(address + off));
		if (best == (uint8_t)PlayerPlatform::Steam || best == (uint8_t)PlayerPlatform::Xbox
			|| best == (uint8_t)PlayerPlatform::Psn)
			break;
	}

	if (debugConsoleEnabled) {
		static uintptr_t s_logged[24]{};
		static int s_loggedN = 0;
		bool seen = false;
		for (int i = 0; i < s_loggedN; ++i) {
			if (s_logged[i] == address) {
				seen = true;
				break;
			}
		}
		if (!seen && s_loggedN < 24) {
			s_logged[s_loggedN++] = address;
			uint8_t raw[16]{};
			ctx->read_array(address + uniqueOff, raw, sizeof(raw));
			TArray bytes = ctx->read<TArray>(address + uniqueOff + 0x20);
			const uintptr_t comp = usedComp ? usedComp : ctx->read<uintptr_t>(address + compOff);
			uint8_t platRaw[48]{};
			TArray platArr{};
			uintptr_t idPtr = 0;
			uint8_t idHex[16]{};
			uint8_t idBody[48]{};
			uint64_t id8 = 0;
			uint64_t vtable = 0;
			uint64_t nestVt[4]{};
			if (is_valid_pointer_optimized(comp)) {
				ctx->read_array(comp + usedPlatOff, platRaw, sizeof(platRaw));
				platArr = ctx->read<TArray>(comp + usedPlatOff + 0x20);
				idPtr = ctx->read<uintptr_t>(comp + usedPlatOff);
				if (is_valid_pointer_optimized(idPtr)) {
					ctx->read_array(idPtr, idHex, sizeof(idHex));
					ctx->read_array(idPtr, idBody, sizeof(idBody));
					memcpy(&vtable, idHex, 8);
					id8 = ctx->read<uint64_t>(idPtr + 8);
					const uintptr_t nestOffs[] = { 0x10, 0x18, 0x20, 0x30 };
					for (int ni = 0; ni < 4; ++ni) {
						const uintptr_t np = ctx->read<uintptr_t>(idPtr + nestOffs[ni]);
						if (is_valid_pointer_optimized(np) && np != idPtr)
							nestVt[ni] = ctx->read<uint64_t>(np);
					}
				}
			}
			char hex[40]{};
			char phex[100]{};
			char ihex[40]{};
			char ibody[100]{};
			for (int i = 0; i < 8; ++i)
				snprintf(hex + i * 2, 3, "%02X", raw[i]);
			for (int i = 0; i < 24; ++i)
				snprintf(phex + i * 2, 3, "%02X", platRaw[i]);
			for (int i = 0; i < 8; ++i)
				snprintf(ihex + i * 2, 3, "%02X", idHex[i]);
			for (int i = 0; i < 48; ++i)
				snprintf(ibody + i * 2, 3, "%02X", idBody[i]);
			std::string typeStr;
			if (is_valid_pointer_optimized(idPtr)) {
				typeStr = ReadPlayerNameFString(idPtr + 0x08, false);
				if (typeStr.empty())
					typeStr = ReadPlayerNameFString(idPtr + 0x10, false);
				if (typeStr.empty())
					typeStr = ReadPlayerNameFString(idPtr + 0x18, false);
				if (typeStr.empty())
					typeStr = ReadPlayerNameFString(idPtr + 0x20, false);
			}
			if (typeStr.empty() && is_valid_pointer_optimized(comp))
				typeStr = ReadPlayerNameFString(comp + usedPlatOff + 0x10, false);
			std::string rtti = ReadMsvcRttiTypeNameFromVtable(vtable);
			if (rtti.size() > 48)
				rtti.resize(48);
			char line[1024];
			snprintf(line, sizeof(line),
				"[PlatDiag] ps=0x%llX plat=%u uniqueOff=0x%llX replTArr=0x%llX n=%d/%d hex=%s "
				"comp=0x%llX platOff=0x%llX phex=%s pTArr=0x%llX pn=%d/%d id=0x%llX idhex=%s "
				"id8=0x%llX type=%s rtti=%s nest=%08X/%08X/%08X/%08X idbody=%s",
				(unsigned long long)address, (unsigned)best,
				(unsigned long long)uniqueOff,
				(unsigned long long)bytes.Data, (int)bytes.Count, (int)bytes.Max,
				hex,
				(unsigned long long)comp,
				(unsigned long long)usedPlatOff,
				phex,
				(unsigned long long)platArr.Data, (int)platArr.Count, (int)platArr.Max,
				(unsigned long long)idPtr,
				ihex,
				(unsigned long long)id8,
				typeStr.empty() ? "-" : typeStr.c_str(),
				rtti.empty() ? "-" : rtti.c_str(),
				(unsigned)(uint32_t)nestVt[0], (unsigned)(uint32_t)nestVt[1],
				(unsigned)(uint32_t)nestVt[2], (unsigned)(uint32_t)nestVt[3],
				ibody);
			DebugSystem::LogConsole(line);
		}
	}
	return best;
}

inline uint8_t UItemDataAsset::GetItemCategory() const
{
	if (!IsValid())
		return 0xFF;
	return ctx->read<uint8_t>(address + Offsets::Get().UItemDataAsset.ItemCategory);
}

inline double UItemDataAsset::GetWeight() const
{
	if (!IsValid())
		return 0.0;
	return ctx->read<double>(address + Offsets::Get().UItemDataAsset.Weight);
}

// The configured inventory offset is confirmed for this build, so it is used directly and
// the field walk below only runs if a pawn does not carry a readable pointer there. That
// ordering matters: the walk costs a name decode per step, so making it the routine path
// starves the DMA link the rest of the cache reads through.
inline std::atomic<uintptr_t> g_resolvedInventoryOffset{ 0 };

inline uintptr_t ResolveInventoryOffset(uintptr_t pawn)
{
	std::atomic<uintptr_t>& s_resolved = g_resolvedInventoryOffset;
	const uintptr_t cached = s_resolved.load(std::memory_order_relaxed);
	if (cached)
		return cached;

	const uintptr_t configured = Offsets::Get().UPawn.InventoryComponent;
	if (configured &&
		is_valid_pointer_optimized(ctx->read<uintptr_t>(pawn + configured))) {
		s_resolved.store(configured, std::memory_order_relaxed);
		return configured;
	}

	auto looksLikeInventory = [](uintptr_t candidate) -> bool {
		if (!is_valid_pointer_optimized(candidate))
			return false;
		const std::string name = NameResolver::GetObjectNameA(ctx->image_base, candidate);
		if (name.empty())
			return false;
		return name.find(XC("Inventory")) != std::string::npos;
	};

	// Hard cap on how many times the field walk may run, for the reason above. A handful of
	// attempts is enough to cross a pass where the pawn is mid-spawn and its fields read null.
	static std::atomic<int> s_attempts{ 0 };
	if (s_attempts.fetch_add(1, std::memory_order_relaxed) >= 8)
		return configured;

	constexpr uintptr_t kScanBegin = 0x300;
	constexpr uintptr_t kScanEnd = 0x1400;
	for (uintptr_t off = kScanBegin; off < kScanEnd; off += 8) {
		if (looksLikeInventory(ctx->read<uintptr_t>(pawn + off))) {
			s_resolved.store(off, std::memory_order_relaxed);
			return off;
		}
	}
	return configured;
}

inline UInventoryComponent UPawn::GetInventoryComponent() const
{
	if (!IsValid())
		return UInventoryComponent(0);
	const uintptr_t off = ResolveInventoryOffset(address);
	if (!off)
		return UInventoryComponent(0);
	uintptr_t inventory = ctx->read<uintptr_t>(address + off);
	return UInventoryComponent(inventory);
}

inline TArray UInventoryComponent::GetCurrentItemActors() const
{
	TArray result{};
	if (!IsValid())
		return result;
	ctx->read_array(address + Offsets::Get().UInventoryComponent.CurrentItemActors, &result, sizeof(TArray));
	return result;
}

// Second array the game falls through to when CurrentItemActors is empty. Offset is zero on
// platforms where it has not been located, in which case this reports nothing rather than
// reading the object header.
inline TArray UInventoryComponent::GetLocalCurrentItems() const
{
	TArray result{};
	const uintptr_t off = Offsets::Get().UInventoryComponent.LocalCurrentItems;
	if (!IsValid() || off == 0)
		return result;
	ctx->read_array(address + off, &result, sizeof(TArray));
	return result;
}

inline uintptr_t UInventoryComponent::GetEquippedPrimaryItem() const
{
	const uintptr_t off = Offsets::Get().UInventoryComponent.EquippedPrimaryItem;
	if (!IsValid() || off == 0)
		return 0;
	return ctx->read<uintptr_t>(address + off);
}

inline uintptr_t UInventoryComponent::GetStowedWeapon0Actor() const
{
	const auto& o = Offsets::Get().UInventoryComponent;
	if (!IsValid() || o.StowedWeapon0 == 0)
		return 0;
	return ctx->read<uintptr_t>(address + o.StowedWeapon0 + o.StowedActorInLayout);
}

inline uintptr_t UInventoryComponent::GetStowedToolActor() const
{
	const uintptr_t off = Offsets::Get().UInventoryComponent.StowedToolActor;
	if (!IsValid() || off == 0)
		return 0;
	return ctx->read<uintptr_t>(address + off);
}

inline uintptr_t UInventoryComponent::GetStowedWeapon0DataAsset() const
{
	const auto& o = Offsets::Get().UInventoryComponent;
	if (!IsValid() || o.StowedWeapon0 == 0 || o.StowedItemDataAsset == 0)
		return 0;
	return ctx->read<uintptr_t>(address + o.StowedWeapon0 + o.StowedItemDataAsset);
}

// Quality straight off the stowed slot rather than the weapon actor. Returns -1
// when unavailable so callers can tell "no data" from a legitimate quality 0.
inline int32_t UInventoryComponent::GetStowedWeapon0Quality() const
{
	const auto& o = Offsets::Get().UInventoryComponent;
	if (!IsValid() || o.StowedWeapon0 == 0 || o.StowedWeaponQuality == 0)
		return -1;
	const int32_t q = ctx->read<int32_t>(address + o.StowedWeapon0 + o.StowedWeaponQuality);
	if (ctx->last_status() != TransportStatus::Ok)
		return -1;
	return q;
}

inline uintptr_t UInventoryComponent::ResolveHeldItemActor(const char** outSrc) const
{
	// EquippedPrimary is what is actually in hands when a gun is out. Scoring
	// utilities above WeaponActor inside CurrentItemActors made every smoke/
	// bandage/mine in the inventory list win over the equipped gun.
	auto isStowed = [](const std::string& name) {
		return name.empty() || name.find(XC("Stowed")) != std::string::npos;
	};
	auto isUtility = [](const std::string& name) -> bool {
		auto has = [&](const char* s) { return name.find(s) != std::string::npos; };
		return has(XC("Smoke")) || has(XC("Grenade")) || has(XC("Trigger")) ||
			has(XC("Mine")) || has(XC("Trap")) || has(XC("Flare")) ||
			has(XC("Throwable")) || has(XC("Glowstick")) || has(XC("GlowStick")) ||
			has(XC("Deployable")) || has(XC("Bandage")) || has(XC("Vita")) ||
			has(XC("Adrenalin")) || has(XC("Cloak")) || has(XC("Barricade")) ||
			has(XC("ZipLine")) || has(XC("Zipline")) || has(XC("Binocular")) ||
			has(XC("DoorBlocker")) || has(XC("Decoy")) || has(XC("Spray")) ||
			has(XC("Pulse")) || has(XC("Defibrillator")) || has(XC("SnapHook")) ||
			has(XC("Snaphook"));
	};
	auto isWeapon = [](const std::string& name) -> bool {
		return name.find(XC("WeaponActor")) != std::string::npos ||
			name.find(XC("BP_WeaponActor")) != std::string::npos;
	};
	auto scoreName = [&](const std::string& name) -> int {
		if (isStowed(name))
			return -1;
		if (isUtility(name))
			return 100;
		if (name.find(XC("ItemActor")) != std::string::npos ||
			name.find(XC("BP_ItemActor")) != std::string::npos ||
			name.find(XC("BP_Item_")) != std::string::npos)
			return 60;
		if (isWeapon(name))
			return 20;
		return 30;
	};

	auto bestFromArray = [&](const TArray& arr) -> uintptr_t {
		if (!arr.Data || arr.Count <= 0)
			return 0;
		const int n = arr.Count > 32 ? 32 : arr.Count;
		uintptr_t cands[32];
		if (!ctx->read_array(arr.Data, cands, (size_t)n * sizeof(uintptr_t)))
			return 0;
		uintptr_t best = 0;
		int bestScore = -1;
		for (int i = 0; i < n; ++i) {
			if (!is_valid_pointer_optimized(cands[i]))
				continue;
			const std::string name = NameResolver::GetObjectNameA(ctx->image_base, cands[i]);
			const int sc = scoreName(name);
			if (sc > bestScore) {
				bestScore = sc;
				best = cands[i];
			}
		}
		return best;
	};

	auto bestUtilityFromArray = [&](const TArray& arr) -> uintptr_t {
		if (!arr.Data || arr.Count <= 0)
			return 0;
		const int n = arr.Count > 32 ? 32 : arr.Count;
		uintptr_t cands[32];
		if (!ctx->read_array(arr.Data, cands, (size_t)n * sizeof(uintptr_t)))
			return 0;
		uintptr_t best = 0;
		int bestScore = -1;
		for (int i = 0; i < n; ++i) {
			if (!is_valid_pointer_optimized(cands[i]))
				continue;
			const std::string name = NameResolver::GetObjectNameA(ctx->image_base, cands[i]);
			if (!isUtility(name) || isStowed(name))
				continue;
			const int sc = scoreName(name);
			if (sc > bestScore) {
				bestScore = sc;
				best = cands[i];
			}
		}
		return best;
	};

	const uintptr_t equipped = GetEquippedPrimaryItem();
	if (is_valid_pointer_optimized(equipped)) {
		const std::string eqName = NameResolver::GetObjectNameA(ctx->image_base, equipped);
		if (!isStowed(eqName)) {
			// Gun equipped: only override when LocalCurrentItems has a throwable
			// actually in hands (smoke/trigger pull-out while primary still set).
			if (isWeapon(eqName)) {
				if (uintptr_t util = bestUtilityFromArray(GetLocalCurrentItems())) {
					if (outSrc)
						*outSrc = XC("local");
					return util;
				}
				if (outSrc)
					*outSrc = XC("equipped");
				return equipped;
			}
			if (outSrc)
				*outSrc = XC("equipped");
			return equipped;
		}
	}

	if (uintptr_t p = bestFromArray(GetCurrentItemActors())) {
		if (outSrc)
			*outSrc = XC("current");
		return p;
	}
	if (uintptr_t p = bestFromArray(GetLocalCurrentItems())) {
		if (outSrc)
			*outSrc = XC("local");
		return p;
	}
	// Stowed tool (utility) before stowed primary — holding a mine/trap often
	// leaves EquippedPrimary empty and StowedTool populated.
	if (uintptr_t p = GetStowedToolActor(); is_valid_pointer_optimized(p)) {
		const std::string n = NameResolver::GetObjectNameA(ctx->image_base, p);
		if (!isStowed(n)) {
			if (outSrc)
				*outSrc = XC("tool");
			return p;
		}
	}
	if (uintptr_t p = GetStowedWeapon0Actor(); is_valid_pointer_optimized(p)) {
		const std::string n = NameResolver::GetObjectNameA(ctx->image_base, p);
		if (!isStowed(n) && (isWeapon(n) ||
			n.find(XC("ItemActor")) != std::string::npos)) {
			if (outSrc)
				*outSrc = XC("stowed");
			return p;
		}
	}
	if (outSrc)
		*outSrc = XC("none");
	return 0;
}

inline uintptr_t UItemBase::GetProjectileOverride() const
{
	if (!IsValid())
		return 0;
	return ctx->read<uintptr_t>(address + Offsets::Get().UItemBase.ProjectileOverride);
}

inline int8_t UItemActor::GetWeaponQuality() const
{
	if (!IsValid())
		return 0;
	return ctx->read<int8_t>(address + Offsets::Get().UItemActor.WeaponQuality);
}

inline TArray UItemActor::GetVisualizedMods() const
{
	TArray result{};
	if (!IsValid())
		return result;
	ctx->read_array(address + Offsets::Get().UItemActor.VisualizedMods, &result, sizeof(TArray));
	return result;
}

inline float UProjectileMovement::GetInitialSpeed() const
{
	if (!IsValid())
		return 0.0f;
	float val = ctx->read<float>(address + Offsets::Get().UProjectileMovement.InitialSpeed);
	if (_isnanf(val) || val < 0.0f || val > 100000.0f)
		return 0.0f;
	return val;
}

inline float UProjectileMovement::GetMaxSpeed() const
{
	if (!IsValid())
		return 0.0f;
	float val = ctx->read<float>(address + Offsets::Get().UProjectileMovement.MaxSpeed);
	if (_isnanf(val) || val < 0.0f || val > 100000.0f)
		return 0.0f;
	return val;
}

inline float UProjectileMovement::GetProjectileGravity() const
{
	if (!IsValid())
		return 0.0f;
	float val = ctx->read<float>(address + Offsets::Get().UProjectileMovement.ProjectileGravity);
	if (_isnanf(val) || val < -10.0f || val > 10.0f)
		return 1.0f;
	return val;
}

inline bool ASalvageExtractionPointBase::GetCanExtract() const
{
	if (!IsValid())
		return false;
	return ctx->read<bool>(address + Offsets::Get().ASalvageExtractionPointBase.bCanExtract);
}

inline bool ASalvageExtractionPointBase::GetIsEnabled() const
{
	if (!IsValid())
		return false;
	return ctx->read<bool>(address + Offsets::Get().ASalvageExtractionPointBase.bIsEnabled);
}

inline EExtractionState ASalvageExtractionPointBase::GetState() const
{
	if (!IsValid())
		return EExtractionState::Dormant;
	return ctx->read<EExtractionState>(address + Offsets::Get().ASalvageExtractionPointBase.State);
}

inline uintptr_t ASalvageExtractionPointBase::GetExtractionDataAsset() const
{
	if (!IsValid())
		return 0;
	return ctx->read<uintptr_t>(address + Offsets::Get().ASalvageExtractionPointBase.ExtractionDataAsset);
}

inline std::string ASalvageExtractionPointBase::GetDialogueTagName() const
{
	uintptr_t dataAssetPtr = GetExtractionDataAsset();
	UExtractionDataAsset dataAsset(dataAssetPtr);
	if (!dataAsset.IsValid())
		return "";
	return dataAsset.GetDialogueTag().GetTagNameString();
}

inline std::string FGameplayTag::GetTagNameString() const
{
	if (!IsValid())
		return "";

	uint32_t comparisonIndex = ctx->read<uint32_t>(address + Offsets::Get().FGameplayTag.TagName);
	if (comparisonIndex == 0 || comparisonIndex == 0xFFFFFFFF)
		return "";

	std::wstring wname = ::DecryptFNameEntry(ctx->image_base, comparisonIndex);
	if (wname.empty())
		return "";

	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), NULL, 0, NULL, NULL);
	if (size_needed <= 0)
		return "";

	std::string result(size_needed, 0);
	WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), &result[0], size_needed, NULL, NULL);
	return result;
}

inline FGameplayTag UExtractionDataAsset::GetDialogueTag() const
{
	if (!IsValid())
		return FGameplayTag(0);
	return FGameplayTag(address + Offsets::Get().UExtractionDataAsset.DialogueTag);
}


inline int32_t UIHoverData::GetLootAmount() const
{
	if (!IsValid())
		return 0;
	return ctx->read<int32_t>(address + Offsets::Get().UIHoverData.LootAmount);
}

inline UItemDataAsset UIHoverData::GetDataAsset() const
{
	if (!IsValid())
		return UItemDataAsset(0);
	uintptr_t dataAsset = ctx->read<uintptr_t>(address + Offsets::Get().UIHoverData.DataAsset);
	return UItemDataAsset(dataAsset);
}

inline bool ReadUIHoverLootAndDataAsset(uintptr_t hoverAddr, int32_t& outLootAmount, uintptr_t& outDataAsset) {
	if (!hoverAddr || !is_valid_pointer_optimized(hoverAddr))
		return false;
	const auto& o = Offsets::Get().UIHoverData;
	size_t size = (size_t)(o.DataAsset - o.LootAmount + sizeof(uintptr_t));
	uint8_t buf[24];
	if (!ctx->read_array(hoverAddr + o.LootAmount, buf, size))
		return false;
	outLootAmount = *(int32_t*)buf;
	outDataAsset = *(uintptr_t*)(buf + (o.DataAsset - o.LootAmount));
	return true;
}



namespace TextDec
{

	struct FString : private TArray
	{
		std::wstring ToWString() const
		{
			if (Count <= 0 || Count > 1024 || !Data || !validpointer(Data))
				return L"";

			std::wstring ws(static_cast<size_t>(Count), L'\0');
			if (!ctx->read_array(Data, ws.data(), (size_t)Count * sizeof(wchar_t)))
				return L"";
			const auto n = ws.find(L'\0');
			if (n != std::wstring::npos)
				ws.resize(n);
			return ws;
		}

		std::string ToString() const
		{
			std::wstring ws = ToWString();
			std::string str;
			str.reserve(ws.size());
			for (wchar_t c : ws) {
				str += static_cast<char>(c);
			}
			return str;
		}
	};

	std::string ReadFString(uint64_t address) {
		FString fstring = ctx->read<FString>(address);
		return fstring.ToString();
	}
}

inline UWorldSettings ULevel::GetWorldSettings() const
{
	if (!IsValid())
		return UWorldSettings(0);
	uintptr_t worldSettings = ctx->read<uintptr_t>(address + Offsets::Get().ULevel.WorldSettings);
	return UWorldSettings(worldSettings);
}

inline TArray ULevel::GetActorArray() const
{
	TArray result{};
	if (!IsValid())
		return result;
	if (!ctx->read_array(address + Offsets::Get().ULevel.ActorArray, &result, sizeof(TArray)))
		return result;
	if (!is_valid_pointer_optimized(result.Data) || result.Count < 0 || result.Count > 100000 || result.Max < 0)
	{
		result.Data = 0;
		result.Count = 0;
		result.Max = 0;
	}
	return result;
}

inline void ScanActorArrayOffset(uint64_t worldAddr, bool enabled = true) {
	if (!enabled || !ctx || !worldAddr) return;

	const auto& offsets = Offsets::Get();

	TArray levels{};
	ctx->read_array(worldAddr + offsets.UWorld.Levels, &levels, sizeof(TArray));

	if (!is_valid_pointer_optimized(levels.Data) || levels.Count <= 0 || levels.Count > 500) {
		return;
	}

	struct OffsetResult { uint32_t offset; int totalActors; int validLevels; };
	std::vector<OffsetResult> results;

	int scanLimit = (levels.Count < 50) ? levels.Count : 50;

	for (uint32_t off = 0x0; off <= 0x200; off += 0x8) {
		int totalActors = 0;
		int validLevels = 0;

		for (int i = 0; i < scanLimit; i++) {
			uint64_t level = ctx->read<uint64_t>(levels.Data + i * 8);
			if (!is_valid_pointer_optimized(level)) continue;

			uint64_t arrayData = ctx->read<uint64_t>(level + off);
			int32_t arrayCount = ctx->read<int32_t>(level + off + 8);

			if (is_valid_pointer_optimized(arrayData) && arrayCount > 0 && arrayCount < 100000) {
				uint64_t firstEntry = ctx->read<uint64_t>(arrayData);
				if (is_valid_pointer_optimized(firstEntry)) {
					totalActors += arrayCount;
					validLevels++;
				}
			}
		}

		if (totalActors > 0)
			results.push_back({ off, totalActors, validLevels });
	}

	if (results.empty()) {
		DBG_LOG("[Scan] No valid TArray candidates found.");
		return;
	}

	int maxActors = 0;
	for (auto& r : results)
		if (r.totalActors > maxActors) maxActors = r.totalActors;

	DBG_LOG("\n[Scan] === ULevel ActorArray Offset Candidates ===");
	for (auto& r : results) {
		int pct = (maxActors > 0) ? (r.totalActors * 100 / maxActors) : 0;
		DBG_LOG("  0x" << std::hex << r.offset << std::dec
			<< "  actors=" << r.totalActors
			<< "  levels=" << r.validLevels
			<< "  [" << pct << "%]"
			<< (pct == 100 ? " <-- BEST" : ""));
	}
}

inline void ScanViewIndex(uint64_t viewPortAddr, int expectedValue = 3, bool enabled = true) {
	if (!enabled) {
		DBG_LOG("[ScanViewIndex] skipped: disabled");
		return;
	}
	if (!ctx) {
		DBG_LOG("[ScanViewIndex] skipped: ctx is null");
		return;
	}
	if (!viewPortAddr || viewPortAddr < 0x10000ULL || viewPortAddr > 0x7FFFFFFFFFFFULL) {
		DBG_LOG("[ScanViewIndex] skipped: bad ViewPort 0x" << std::hex << viewPortAddr << std::dec);
		return;
	}

	DBG_LOG("\n[ScanViewIndex] ViewPort=0x" << std::hex << viewPortAddr
		<< "  expected=" << std::dec << expectedValue);

	struct Hit { uint32_t offset; int32_t value; };
	std::vector<Hit> matches;
	std::vector<Hit> nearby;

	for (uint32_t off = 0x0; off <= 0x300; off += 0x4) {
		int32_t val = ctx->read<int32_t>(viewPortAddr + off);

		if (val == expectedValue)
			matches.push_back({ off, val });
		else if (val >= 0 && val <= 10 && val != 0)
			nearby.push_back({ off, val });
	}

	if (matches.empty() && nearby.empty()) {
		DBG_LOG("[ScanViewIndex] No candidates found.");
		return;
	}

	DBG_LOG("[ScanViewIndex] === Exact Matches (value==" << expectedValue << ") ===");
	for (auto& h : matches) {
		DBG_LOG("  0x" << std::hex << h.offset << std::dec
			<< "  value=" << h.value);
	}

	if (!nearby.empty()) {
		DBG_LOG("[ScanViewIndex] === Nearby (int 1-10, != " << expectedValue << ") ===");
		for (auto& h : nearby) {
			DBG_LOG("  0x" << std::hex << h.offset << std::dec
				<< "  value=" << h.value);
		}
	}

	DBG_LOG("[ScanViewIndex] Total exact=" << matches.size()
		<< "  nearby=" << nearby.size());
	DBG_LOG("[ScanViewIndex] ==========================================\n");
}

inline float UWorldSettings::GetWorldGravity() const
{
	if (!IsValid())
		return -980.0f;
	float val = ctx->read<float>(address + Offsets::Get().UWorldSettings.WorldGravity);
	if (_isnanf(val) || val < -10000.0f || val > 10000.0f)
		return -980.0f;
	return val;
}


inline std::string UIHoverData::GetItemName() const
{
	if (!IsValid())
		return "";

	uintptr_t uiHoverDataText = ctx->read<uintptr_t>(address);
	if (!uiHoverDataText)
		return "";

	uintptr_t name_ptr = ctx->read<uintptr_t>(uiHoverDataText + 0x10);
	if (!name_ptr)
		return "";

	uintptr_t name_details = ctx->read<uintptr_t>(name_ptr + 0x30);
	if (!name_details)
		return "";

	return TextDec::ReadFString(name_details + 0x10);
}

inline std::string GetItemNameFromHoverTextPtr(uintptr_t uiHoverDataText)
{
	if (!uiHoverDataText || !is_valid_pointer_optimized(uiHoverDataText))
		return "";
	uintptr_t name_ptr = ctx->read<uintptr_t>(uiHoverDataText + 0x10);
	if (!name_ptr || !is_valid_pointer_optimized(name_ptr))
		return "";
	uintptr_t name_details = ctx->read<uintptr_t>(name_ptr + 0x30);
	if (!name_details || !is_valid_pointer_optimized(name_details))
		return "";
	return TextDec::ReadFString(name_details + 0x10);
}

namespace TextDec2
{
	struct FText
	{
		char pad[0x10];
		uint64_t TextDataPtr;

		std::string ToString() const
		{
			if (!TextDataPtr || TextDataPtr < 0x10000) {
				return std::string();
			}

			uint64_t string_ptr = ctx->read<uint64_t>(TextDataPtr + 0x18);
			if (!string_ptr || string_ptr < 0x10000) {
				return std::string();
			}

			std::vector<wchar_t> buffer(256);
			if (!ctx->read_array(string_ptr, buffer.data(), 512)) {
				return std::string();
			}

			size_t len = 0;
			while (len < 256 && buffer[len] != L'\0') {
				len++;
			}

			if (len == 0) {
				return std::string();
			}

			std::wstring wstr(buffer.data(), len);
			return std::string(wstr.begin(), wstr.end());
		}
	};
}
