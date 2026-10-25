#include "OffsetHealth.hpp"

#include "CacheTypes.hpp"
#include "sdk/Offsets.hpp"
#include "sdk/SDK.hpp"
#include "sdk/Decryptions.hpp"
#include "sdk/GObjectsRecovery.hpp"
#include "memory/interface/ctx.hpp"
#include "dgb/Spoof.h"

#include <cstring>

namespace OffsetHealth {

static void AddIssue(Result& r, OffsetGroup g, Severity s, const std::string& field, const std::string& msg) {
    r.issues.push_back({ g, s, field, msg });
    if (s == Severity::Critical)
        r.healthy = false;
}

static bool PeHeaderLooksValid(uint64_t imageBase) {
    if (!imageBase || imageBase < 0x10000ULL || imageBase > 0x00007FFFFFFFFFFFULL)
        return false;
    auto [mz, status] = ctx->read_status<uint16_t>(imageBase);
    if (status != TransportStatus::Ok)
        return false;
    return mz == 0x5A4D; // "MZ"
}

static bool GObjectsLooksValid(uint64_t imageBase) {
    uint32_t count = GetGObjectsCount(imageBase);
    uint64_t chunks = GetGObjectsChunksPtr(imageBase);
    if (count == 0 || count > 900000)
        return false;
    if (!is_valid_pointer_optimized(chunks))
        return false;
    return true;
}

// Both scans race the game's own load sequence: EmbarkCommonGameViewportClient
// is not allocated into GObjects until the level finishes loading, and the
// local player/controller chain populates a beat after that. A single failed
// attempt at boot is therefore normal, not necessarily a broken offset -
// InitializeViewPort() (CacheTypes.hpp) already retries up to 30x for this
// same reason. These retry budgets are intentionally shorter since by the
// time RunHealthCheck runs the user has already activated from a game lobby.
static constexpr int kScanMaxAttempts = 15;
static constexpr DWORD kScanRetryDelayMs = 1000;

static bool TryResolveLocalPlayerControllerOnce(uint64_t viewport, uint64_t& outPlayerController) {
    SPOOF_FUNC;
    outPlayerController = 0;
    const auto& off = Offsets::Get();

    auto [gameInstance, giStatus] = ctx->read_status<uint64_t>(viewport + off.UViewPort.GameInstance);
    if (giStatus != TransportStatus::Ok || !is_valid_pointer_optimized(gameInstance))
        return false;

    auto [localPlayers, lpStatus] = ctx->read_status<uint64_t>(gameInstance + off.UGameInstance.LocalPlayers);
    if (lpStatus != TransportStatus::Ok || !is_valid_pointer_optimized(localPlayers))
        return false;

    auto [localPlayer, lpcStatus] = ctx->read_status<uint64_t>(localPlayers);
    if (lpcStatus != TransportStatus::Ok || !is_valid_pointer_optimized(localPlayer))
        return false;

    auto [pc, pcStatus] = ctx->read_status<uint64_t>(localPlayer + off.ULocalPlayer.PlayerController);
    if (pcStatus != TransportStatus::Ok || !is_valid_pointer_optimized(pc))
        return false;

    // Verify it has the local-player flag (if offset is non-zero).
    if (off.PlayerControllerFlags.bIsLocalPlayerController != 0) {
        auto [flag, flagStatus] = ctx->read_status<uint8_t>(pc + off.PlayerControllerFlags.bIsLocalPlayerController);
        if (flagStatus == TransportStatus::Ok && flag == 0)
            return false; // not the local controller
    }

    outPlayerController = pc;
    return true;
}

bool ScanForViewportClient(uint64_t& outViewport, uint32_t& outIndex) {
    SPOOF_FUNC;
    for (int attempt = 0; attempt < kScanMaxAttempts; ++attempt) {
        if (TryRefreshViewPortOnce(outViewport, outIndex))
            return true;
        if (attempt + 1 < kScanMaxAttempts)
            Sleep(kScanRetryDelayMs);
    }
    return false;
}

bool ScanForLocalPlayerController(uint64_t& outPlayerController) {
    SPOOF_FUNC;
    outPlayerController = 0;

    for (int attempt = 0; attempt < kScanMaxAttempts; ++attempt) {
        uint64_t viewport = 0;
        uint32_t viewportIndex = 0;
        if (TryRefreshViewPortOnce(viewport, viewportIndex) &&
            TryResolveLocalPlayerControllerOnce(viewport, outPlayerController))
            return true;

        if (attempt + 1 < kScanMaxAttempts)
            Sleep(kScanRetryDelayMs);
    }
    return false;
}

static void CheckZeroPlaceholder(Result& r, OffsetGroup g, const char* name, uintptr_t value) {
    if (value == 0x0)
        AddIssue(r, g, Severity::Warning, name, "offset is 0x0 (placeholder or embedded-at-base)");
}

static void CheckPointerOffset(Result& r, OffsetGroup g, const char* name, uintptr_t base, uintptr_t offset) {
    if (!is_valid_pointer_optimized(base))
        return;
    auto [ptr, status] = ctx->read_status<uint64_t>(base + offset);
    if (status != TransportStatus::Ok)
        AddIssue(r, g, Severity::Critical, name, "read failed");
    else if (!is_valid_pointer_optimized(ptr) && ptr != 0)
        AddIssue(r, g, Severity::Warning, name, "returned non-zero invalid pointer");
}

Result RunHealthCheck(uint64_t imageBase) {
    SPOOF_FUNC;
    Result r;

    const auto& off = Offsets::Get();

    // 1. PE / image base
    if (!PeHeaderLooksValid(imageBase)) {
        AddIssue(r, OffsetGroup::Image, Severity::Critical, "imageBase", "image base missing or bad PE header");
        return r; // further checks require a sane image
    }
    {
        // Report base and size against the values the SDK dump recorded for this build
        // (base 0x140000000, size 0x116E7000, CL-1341255). Every RVA in this file is
        // relative to this base, so if it is not PioneerGame.exe then GWorld, GObjects
        // and the FName pool are all being read from the wrong module.
        const uint32_t e_lfanew = ctx->read<uint32_t>(imageBase + 0x3C);
        const uint32_t sizeOfImage = ctx->read<uint32_t>(imageBase + e_lfanew + 0x50);
        char msg[192];
        snprintf(msg, sizeof(msg),
            "base=0x%llX sizeOfImage=0x%X (dump recorded base=0x140000000 size=0x116E7000) match=%d",
            (unsigned long long)imageBase, sizeOfImage,
            (sizeOfImage == 0x116E7000u) ? 1 : 0);
        AddIssue(r, OffsetGroup::Image, Severity::Info, "imageBase", msg);
    }

    // 2. UWorld via GWorld, checked before GObjects because it decides how bad a GObjects
    // failure actually is.
    //
    // GObjects used to be non-negotiable: the viewport was found by walking it and matching
    // object names, so losing it lost UWorld and everything under it. Reading GWorld
    // straight from the image removes that dependency for the player path entirely. What a
    // broken GObjects decryption now costs is FName resolution, which means world ESP
    // cannot identify actor classes - a real loss, but not one worth refusing to start
    // over. Keeping it Critical did exactly that: healthy=false aborts entry_sequence()
    // before any of the ESP runs, which is why the console showed five offset lines and
    // then silence.
    uint64_t gworld = GetGWorld(imageBase);
    bool gworldOk = (gworld != 0);
    {
        char msg[192];
        if (gworldOk) {
            const uint64_t indirect = Platform::Steam::g_gworldIndirectOffset;
            snprintf(msg, sizeof(msg),
                "OK - UWorld=0x%llX indirect=+0x%llX pl=0x%X ow=0x%X",
                (unsigned long long)gworld,
                (unsigned long long)indirect,
                Platform::Steam::g_gworldFoundPL,
                Platform::Steam::g_gworldFoundOW);
            AddIssue(r, OffsetGroup::UWorld, Severity::Info, "GWorld", msg);
        } else {
            char dump[512]{};
            Platform::Steam::DumpGWorldRva(imageBase, dump, sizeof(dump));
            AddIssue(r, OffsetGroup::UWorld, Severity::Critical, "GWorldDump", dump);

            uint64_t foundRva = 0;
            const uint64_t scanned = ScanForGWorld(imageBase, foundRva);
            if (scanned) {
                gworld = scanned;
                gworldOk = true;
                snprintf(msg, sizeof(msg),
                    "configured RVA rejected; found working GWorld at RVA 0x%llX -> UWorld=0x%llX pl=0x%X ow=0x%X",
                    (unsigned long long)foundRva, (unsigned long long)scanned,
                    Platform::Steam::g_gworldFoundPL,
                    Platform::Steam::g_gworldFoundOW);
                AddIssue(r, OffsetGroup::UWorld, Severity::Warning, "GWorld", msg);
            } else {
                AddIssue(r, OffsetGroup::UWorld, Severity::Critical, "GWorld",
                         "structural sweep found nothing; falling back to offset-free probe");

                // The structural test assumes PersistentLevel/OwningWorld/Levels offsets,
                // so an empty result cannot distinguish "no GWorld in the image" from "one
                // of those three offsets is wrong for this build". Report raw UObject-like
                // candidates so both can be judged from the data instead of guessed at.
                Platform::Steam::GWorldCandidate cands[12]{};
                // Qualified: the parameter type lives in Platform::Steam, so ADL would
                // otherwise make this ambiguous with the platform-specific overload.
                const int n = ::ProbeGWorldCandidates(imageBase, cands, 12);
                if (n == 0) {
                    AddIssue(r, OffsetGroup::UWorld, Severity::Critical, "GWorldProbe",
                             "no image pointer references an off-image UObject; GWorld is "
                             "not stored as a plain pointer (encrypted or double-deref)");
                } else {
                    for (int i = 0; i < n; ++i) {
                        char msg[224];
                        snprintf(msg, sizeof(msg),
                            "rva=0x%llX obj=0x%llX ptrs=%d pl@0x110=0x%llX pl@0xF0=0x%llX pl@0x128=0x%llX pl@0x158=0x%llX",
                            (unsigned long long)cands[i].rva,
                            (unsigned long long)cands[i].object,
                            cands[i].offImagePointers,
                            (unsigned long long)cands[i].at0x110,
                            (unsigned long long)cands[i].at0xF0,
                            (unsigned long long)cands[i].at0x128,
                            (unsigned long long)cands[i].at0x158);
                        AddIssue(r, OffsetGroup::UWorld, Severity::Info, "GWorldProbe", msg);
                    }
                }
            }
        }
    }

    // 3. GObjects
    if (!GObjectsLooksValid(imageBase)) {
        AddIssue(r, OffsetGroup::GObjects,
                 gworldOk ? Severity::Warning : Severity::Critical, "GObjects",
                 gworldOk
                     ? "decryption broken; player ESP runs off GWorld but world ESP/name resolution will not"
                     : "count or chunks pointer invalid and no GWorld fallback; cannot resolve UWorld");

        // The message above cannot say which of the five stages broke, and the
        // failure modes need different fixes: a bad base pointer means the
        // enc/xor RVAs moved, a bad count with a good base means only the count
        // pipeline constants moved, and a good vtable with no chunks points at
        // the Unicorn emulation rather than at any offset. Report each stage.
        const uint64_t basePtr = GetGObjectsBasePtr(imageBase);
        const uint32_t count = GetGObjectsCount(imageBase);
        uint64_t vtable = 0;
        uint64_t funcPtr = 0;
        if (is_valid_pointer_optimized(basePtr)) {
            const uint64_t vtOff = (g_PlatformVersion == PlatformVersion::Xbox)
                ? (uint64_t)XBOX_GOBJECTS_VTABLE_OFFSET : (uint64_t)STEAM_GOBJECTS_VTABLE_OFFSET;
            const uint64_t fnIdx = (g_PlatformVersion == PlatformVersion::Xbox)
                ? (uint64_t)XBOX_GOBJECTS_VTABLE_FN_INDEX : (uint64_t)STEAM_GOBJECTS_VTABLE_FN_INDEX;
            vtable = ctx->read<uint64_t>(basePtr + vtOff);
            if (is_valid_pointer_optimized(vtable))
                funcPtr = ctx->read<uint64_t>(vtable + fnIdx);
        }
        const uint64_t chunks = GetGObjectsChunksPtr(imageBase);

        char msg[240];
        snprintf(msg, sizeof(msg),
            "stage base=0x%llX(%d) count=%u(%d) vtable=0x%llX(%d) fn=0x%llX(%d) chunks=0x%llX(%d) plat=%s",
            (unsigned long long)basePtr, is_valid_pointer_optimized(basePtr) ? 1 : 0,
            count, (count > 0 && count <= 900000) ? 1 : 0,
            (unsigned long long)vtable, is_valid_pointer_optimized(vtable) ? 1 : 0,
            (unsigned long long)funcPtr, is_valid_pointer_optimized(funcPtr) ? 1 : 0,
            (unsigned long long)chunks, is_valid_pointer_optimized(chunks) ? 1 : 0,
            (g_PlatformVersion == PlatformVersion::Xbox) ? "xbox" : "steam");
        AddIssue(r, OffsetGroup::GObjects, Severity::Info, "GObjectsStage", msg);

        // Only worth running when the base pointer itself is the failure. If
        // the base decrypts but a later stage breaks, the constants this
        // recovers are already correct and the search would just confirm them
        // at the cost of a full image sweep.
        //
        // Parked. The 2026-08-21 run swept 440M candidates and validated none,
        // which means the encrypt pipeline changed shape rather than just its
        // rotation constant - the sweep cannot reach it and costs ~4 minutes of
        // startup to prove that again every launch. Player ESP runs off GWorld
        // meanwhile. Re-enable when a dump supplies the new encrypted-base RVA.
        constexpr bool kRunRecoveryScan = false;
        if (kRunRecoveryScan && !is_valid_pointer_optimized(basePtr)) {
            const GObjectsRecovery::Result rec = GObjectsRecovery::Run(imageBase);
            char rmsg[288];
            if (rec.found) {
                snprintf(rmsg, sizeof(rmsg),
                    "RECOVERED tier=%d enc=0x%llX xorKey=0x%llX rot=%d shuflo=0x%X base=0x%llX vtable=0x%llX (%s) tested=%llu probed=%llu",
                    rec.tier, (unsigned long long)rec.encRva, (unsigned long long)rec.xorKeyRva,
                    rec.rot64, rec.shufloImm, (unsigned long long)rec.basePtr,
                    (unsigned long long)rec.vtable, rec.note,
                    (unsigned long long)rec.candidatesTested, (unsigned long long)rec.probesValidated);
            } else {
                snprintf(rmsg, sizeof(rmsg),
                    "scan failed (%s) tested=%llu probed=%llu",
                    rec.note, (unsigned long long)rec.candidatesTested,
                    (unsigned long long)rec.probesValidated);
            }
            AddIssue(r, OffsetGroup::GObjects, Severity::Info, "GObjectsRecovery", rmsg);
        }
    }

    // 3. Viewport client signature scan (retries internally to survive the
    // load-sequence race; only reaches here if every retry was exhausted).
    uint64_t viewport = 0;
    uint32_t viewportIndex = 0;
    if (!ScanForViewportClient(viewport, viewportIndex)) {
        AddIssue(r, OffsetGroup::UViewPort, Severity::Warning, "viewport",
                 "signature scan failed after retries; viewport will be resolved by InitializeViewPort");
    }

    // The viewport route is now only a secondary confirmation, so its failure is no longer
    // fatal on its own - GWorld above already decided whether UWorld is reachable.
    if (viewport != 0) {
        auto [uworld, uworldStatus] = ctx->read_status<uint64_t>(viewport + off.UViewPort.Uworld);
        if (uworldStatus != TransportStatus::Ok || !is_valid_pointer_optimized(uworld)) {
            AddIssue(r, OffsetGroup::UWorld,
                     gworldOk ? Severity::Warning : Severity::Critical, "UWorld",
                     "viewport->UWorld unreadable or invalid");
        }
    }

    // 5. Local player controller signature scan (retries internally; see note above).
    uint64_t localPC = 0;
    if (!ScanForLocalPlayerController(localPC)) {
        AddIssue(r, OffsetGroup::UPlayerController, Severity::Warning, "localPC",
                 "signature scan failed after retries; will be resolved during normal init");
    } else {
        // Validate a few key controller fields.
        CheckPointerOffset(r, OffsetGroup::UPlayerController, "AcknowledgedPawn", localPC, off.UPlayerController.AcknowledgedPawn);
        auto [rot, rotStatus] = ctx->read_status<FRotator>(localPC + off.UPlayerController.ControlRotation);
        if (rotStatus != TransportStatus::Ok)
            AddIssue(r, OffsetGroup::UPlayerController, Severity::Warning, "ControlRotation", "read failed");
    }

    // 6. Flag 0x0 placeholders that still exist in the offset table.
    CheckZeroPlaceholder(r, OffsetGroup::UObject, "UObject.UIHoverData", off.UObject.UIHoverData);
    CheckZeroPlaceholder(r, OffsetGroup::Arc, "FGameplayTag.TagName", off.FGameplayTag.TagName);

    // 7. Validate some fragile non-zero offsets by reading stable roots.
    if (localPC != 0) {
        CheckPointerOffset(r, OffsetGroup::UPlayerController, "AcknowledgedPawn", localPC, off.UPlayerController.AcknowledgedPawn);
    }
    if (viewport != 0) {
        CheckPointerOffset(r, OffsetGroup::UGameInstance, "GameInstance", viewport, off.UViewPort.GameInstance);
    }

    // 8. Sanity-check ARC health offsets are non-zero (they are struct-in-struct, but 0 means dead reads).
    if (off.Arc.ActorHealthComponent == 0x0)
        AddIssue(r, OffsetGroup::Arc, Severity::Critical, "Arc.ActorHealthComponent", "offset is zero");
    if (off.Arc.HealthService == 0x0)
        AddIssue(r, OffsetGroup::Arc, Severity::Critical, "Arc.HealthService", "offset is zero");
    if (off.Arc.HGS_BaseGroup == 0x0)
        AddIssue(r, OffsetGroup::Arc, Severity::Critical, "Arc.HGS_BaseGroup", "offset is zero");

    return r;
}

} // namespace OffsetHealth
