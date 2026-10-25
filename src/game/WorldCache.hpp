#pragma once
#include "config/build_config.hpp"
#include "SlowCache.hpp"
#include "EspReadyDiag.hpp"
#include "RayVis.hpp"
#include "StringHash.hpp"
#include "draws/Textures.h"
#include "dgb/Spoof.h"
#include <cctype>
#include <cmath>
#include <iostream>
#include <map>
#include <memory>
#include <mutex>
#include <shared_mutex>

inline std::unordered_map<uint64_t, double> g_ActorWeightMap;
inline std::shared_mutex g_ActorWeightMtx;

inline double GetActorWeight(uint64_t actor) {
    SPOOF_FUNC;
    std::shared_lock<std::shared_mutex> lk(g_ActorWeightMtx);
    auto it = g_ActorWeightMap.find(actor);
    return (it != g_ActorWeightMap.end()) ? it->second : 0.0;
}

namespace NT::Store {

    class CScene {
    public:
        struct PlayerBuffer {
            std::vector<PlayerDynamicData> data;
            volatile LONG refCount = 1;
            void AddRef() { InterlockedIncrement(&refCount); }
            void Release() { if (InterlockedDecrement(&refCount) == 0) delete this; }
        };

        CScene() { InitializeCriticalSectionAndSpinCount(&m_playerSwapCS, 4000); }
        ~CScene() { DeleteCriticalSection(&m_playerSwapCS); }

    private:
        PlayerBuffer* volatile m_playerCurrent = nullptr;
        CRITICAL_SECTION m_playerSwapCS;

        std::vector<WorldCacheItem> m_staticWorld;
        std::mutex m_staticWorldMtx;
        volatile LONG m_staticWorldVersion = 0;

        std::vector<WorldDrawInfo> m_dynamicWorld;
        std::shared_mutex m_dynamicWorldMtx;

        int m_cachedEnemies = 0;
        int m_cachedSpectators = 0;
        std::mutex m_statsMtx;

        uint64_t m_lastStaticUWorld = 0;
        uint64_t m_pendingStaticUWorld = 0;
        int m_pendingStaticConfirmCount = 0;
        volatile LONG m_renderEpoch = 0;
        LONG m_dynLastStaticVersion = -1;
        std::vector<WorldCacheItem> m_dynCachedStatic;
        std::mutex m_dynCachedStaticMtx;

        struct VisCacheEntry {
            bool visible = false;
            uint8_t trueStreak = 0;
            uint64_t lastMs = 0;
        };
        std::unordered_map<uint64_t, VisCacheEntry> m_visibilityCache;

        // Asymmetric hysteresis: go NOT-visible immediately (red in cover),
        // require 2 consecutive visible samples before showing visible (white).
        bool UpdateVisibility(uint64_t entity, bool sample, uint64_t nowMs) {
            auto& e = m_visibilityCache[entity];
            e.lastMs = nowMs;
            if (!sample) {
                e.trueStreak = 0;
                e.visible = false;
                return false;
            }
            if (e.trueStreak < 250)
                ++e.trueStreak;
            e.visible = (e.trueStreak >= 2);
            return e.visible;
        }

        // Steam LastRenderTime / bRecentlyRendered are dead (zeros). If the mesh only
        // ticks pose when rendered, worldLoc still replicates behind a wall while the
        // head bone freezes — that is the vis signal.
        bool PoseMovedVisible(uint64_t entity, const Vector3& loc, const Vector3& head,
            bool hasHead, uint64_t nowMs, float* outLocD, float* outHeadD)
        {
            struct Rec {
                Vector3 loc{};
                Vector3 head{};
                bool hasHead = false;
                bool vis = true;
                bool init = false;
                uint64_t lastMs = 0;
            };
            static std::unordered_map<uint64_t, Rec> s_pose;
            if (s_pose.size() > 256)
                s_pose.clear();
            auto& r = s_pose[entity];
            float locD = 0.f, headD = -1.f;
            if (r.init && r.lastMs && nowMs >= r.lastMs) {
                locD = (float)r.loc.distance(loc);
                if (r.hasHead && hasHead)
                    headD = (float)r.head.distance(head);
                const uint64_t dt = nowMs - r.lastMs;
                if (dt >= 12 && dt <= 400 && locD > 40.f) {
                    if (headD >= 0.f && headD < 18.f && headD < locD * 0.22f)
                        r.vis = false;
                    else if (headD >= 22.f)
                        r.vis = true;
                }
            } else if (!r.init) {
                r.vis = true;
            }
            r.loc = loc;
            r.head = head;
            r.hasHead = hasHead;
            r.init = true;
            r.lastMs = nowMs;
            if (outLocD) *outLocD = locD;
            if (outHeadD) *outHeadD = headD;
            return r.vis;
        }

        void IndexPlayers() {
            SPOOF_FUNC;
            if (Entity->GetRaidState() != RaidState::InRaid)
                return;

            auto entitySnapshot = Entity->GetSnapshot();
            PointerCache ptrs = Entity->GetPointerSnapshot();

            uintptr_t localPawn = ptrs.LocalPawn;
            uintptr_t localPS = 0;
            if (is_valid_pointer_optimized(localPawn))
                localPS = ctx->read<uintptr_t>(localPawn + Offsets::Get().UPawn.PlayerState);
            uintptr_t localAck = 0;
            if (is_valid_pointer_optimized(ptrs.PlayerController))
                localAck = ctx->read<uintptr_t>(
                    ptrs.PlayerController + Offsets::Get().UPlayerController.AcknowledgedPawn);
            const LocalAvatarIds localIds = CaptureLocalAvatarIds(
                localPawn, localAck, localPS,
                ptrs.PlayerController, ptrs.PlayerCameraManager);
            Vector3 localPos = ptrs.RelativeLocation;

            if (localPos.IsZero() && (localPawn != 0) && is_valid_pointer_optimized(localPawn)) {
                uintptr_t localRoot = ctx->read<uintptr_t>(localPawn + Offsets::Get().UObject.RootComponent);
                if (is_valid_pointer_optimized(localRoot))
                    localPos = GetSceneComponentWorldLocation(localRoot);
            }

            uint64_t nowMs = GetNowMs();
            CachedCameraData visCam = Entity->GetCameraSnapshot();

            auto slowPlayerSnap = AsyncPlayer->GetSnapshot();

            PlayerBuffer* prev = AcquirePlayerBuffer();
            std::unordered_map<uint64_t, const PlayerDynamicData*> prevPlayerMap;
            if (prev) {
                prevPlayerMap.reserve(prev->data.size());
                for (size_t i = 0; i < prev->data.size(); ++i) {
                    auto& old = prev->data[i];
                    if (!old.npc && !old.loot && !old.container && !old.salvage &&
                        !old.grenade && !old.mine && !old.trap && !old.extraction &&
                        !old.raidercache && !old.corpse && !old.arcprobe)
                    {
                        prevPlayerMap[old.entity] = &old;
                    }
                }
            }

            // Un-pin state for the glitch clamp further down. A far-away read parks itself
            // here as a candidate instead of being thrown away outright, so a position that
            // keeps reporting from the same new place can eventually be adopted.
            struct WorldLocRecovery {
                Vector3 candidate{};
                int agree = 0;
                uint64_t lastLogMs = 0;
            };
            static std::unordered_map<uint64_t, WorldLocRecovery> s_worldLocRecovery;
            if (s_worldLocRecovery.size() > 256)
                s_worldLocRecovery.clear();
            constexpr int kWorldLocAdoptPolls = 3;

            std::vector<PlayerDynamicData> newPlayers;
            newPlayers.reserve(entitySnapshot.size());
            int visYes = 0, visNo = 0;

            for (auto& ent : entitySnapshot) {
                if (Entity->GetRaidState() != RaidState::InRaid)
                    break;

                if (!is_valid_pointer_optimized(ent.Entity))
                    continue;
                if (!is_valid_pointer_optimized(ent.RootComponent))
                    continue;
                if (MatchesLocalAvatar(ent.Entity, ent.PlayerState, localIds))
                    continue;

                PlayerDynamicData pd{};
                pd.stale = ent.stale;
                pd.entity = ent.Entity;
                pd.playerState = ent.PlayerState;
                pd.skeletalMesh = ent.SkeletalMesh;
                pd.rootComponent = ent.RootComponent;
                pd.name = ent.Name;
                pd.squadname = ent.SquadName;
                pd.platform = ent.Platform;
                pd.is_team = ent.is_team;

                if (prev) {
                    auto prevBoneIt = prevPlayerMap.find(ent.Entity);
                    if (prevBoneIt != prevPlayerMap.end()) {
                        const auto& old = *prevBoneIt->second;
                        if (old.bones_valid) {
                            pd.skeletonBones = old.skeletonBones;
                            pd.previous_head_pos = old.previous_head_pos;
                            pd.bones_valid = old.bones_valid;
                            pd.fullSkeleton = old.fullSkeleton;
                            pd.fullSkeletonMs = old.fullSkeletonMs;
                            pd.cachedBones = old.cachedBones;
                            // Staleness stamp for the two bones cachedBones is carried over
                            // for. Leaving it at its default zeroed the render path's only
                            // freshness signal on every rebuild, so the headRootMs != 0 test
                            // in Loop.hpp dropped every player until the next slow-cache pass
                            // re-stamped them.
                            pd.headRootMs = old.headRootMs;
                        }
                        // Keep last health/armor so bars don't blink off when slow cache misses a tick.
                        pd.currentHealth = old.currentHealth;
                        pd.maxHealth = old.maxHealth;
                        pd.currentArmor = old.currentArmor;
                        pd.maxArmor = old.maxArmor;
                        pd.isVisible = old.isVisible;
                        if (pd.platform == 0)
                            pd.platform = old.platform;
                    }
                }

                Vector3 rawWorldLoc = GetSceneComponentWorldLocation(pd.rootComponent);
                {
                    const auto& sc = Offsets::Get().USceneComponent;
                    pd.velocity = ctx->read_stale<Vector3>(pd.rootComponent + sc.ComponentVelocity, pd.stale);
                }

                bool posValid = !rawWorldLoc.IsZero() && is_valid_bone_position(rawWorldLoc);
                bool usedFreshPos = false;

                if (posValid) {
                    auto prevIt = prevPlayerMap.find(pd.entity);
                    if (prevIt != prevPlayerMap.end() && is_valid_bone_position(prevIt->second->worldLoc)) {
                        const Vector3& heldLoc = prevIt->second->worldLoc;
                        const float jump = (float)heldLoc.distance(rawWorldLoc);
                        // Only discard absurd glitch reads — no blending (blending lagged and "jumped").
                        //
                        // The rejection measures against the held value, which makes it one-way
                        // without the agreement counter below: the first read past the threshold
                        // pins worldLoc, and every read after that is measured against the pin so
                        // it is also past the threshold, forever. FallenSparten79 sat at
                        // (125545,525957,-48452) for a whole match while his mesh walked 155m
                        // away. Since headOk (280u), footNearPawn (260u) and
                        // IsCoherentPlayerSkeleton all measure bones against worldLoc, all three
                        // failed for as long as the pin held and the player was dropped from ESP
                        // permanently rather than for the one frame the clamp is meant to cover.
                        //
                        // A torn read does not land in the same wrong place twice; a respawn,
                        // teleport, or a poll gap long enough to outrun 6m does. So keep holding
                        // while the far reads disagree with each other, and adopt once
                        // kWorldLocAdoptPolls of them agree.
                        //
                        // The agreement test asks "does this repeat", and a pawn parked at the
                        // world origin repeats for as long as it stays parked - seconds, far
                        // longer than the adopt window. Observed as [WorldLocPin] adopting
                        // (-2410,-114,53) for DadeXGamingTTV and NotBurtReynolds while their
                        // meshes read the same place, after which both rendered as 5.2km ESP
                        // entries until the engine put them back. Arc's playable space sits
                        // tens of thousands of units out on both axes, so a real pawn is never
                        // found here and holding the last known position is always the better
                        // answer.
                        constexpr double kWorldOriginRadius = 5000.0;
                        const bool candidateAtOrigin =
                            std::fabs(rawWorldLoc.x) < kWorldOriginRadius &&
                            std::fabs(rawWorldLoc.y) < kWorldOriginRadius;

                        if (jump > 600.0f && candidateAtOrigin) {
                            s_worldLocRecovery.erase(pd.entity);
                            pd.worldLoc = heldLoc;
                        }
                        else if (jump > 600.0f) {
                            auto& rec = s_worldLocRecovery[pd.entity];
                            if (rec.agree > 0 && rec.candidate.distance(rawWorldLoc) <= 600.0) {
                                ++rec.agree;
                            } else {
                                rec.candidate = rawWorldLoc;
                                rec.agree = 1;
                            }

                            if (rec.agree >= kWorldLocAdoptPolls) {
                                if (debugConsoleEnabled && (nowMs - rec.lastLogMs) > 5000) {
                                    rec.lastLogMs = nowMs;
                                    char pline[256];
                                    snprintf(pline, sizeof(pline),
                                        "[WorldLocPin] name=%s adopt held=(%.0f,%.0f,%.0f) "
                                        "new=(%.0f,%.0f,%.0f) gap=%.0f",
                                        pd.name.empty() ? "<empty>" : pd.name.c_str(),
                                        heldLoc.x, heldLoc.y, heldLoc.z,
                                        rawWorldLoc.x, rawWorldLoc.y, rawWorldLoc.z, jump);
                                    DebugSystem::LogConsole(pline);
                                }
                                rec.agree = 0;
                                pd.worldLoc = rawWorldLoc;
                            } else {
                                pd.worldLoc = heldLoc;
                            }
                        } else {
                            pd.worldLoc = rawWorldLoc;
                            s_worldLocRecovery.erase(pd.entity);
                        }
                    } else {
                        pd.worldLoc = rawWorldLoc;
                    }
                    usedFreshPos = true;
                } else {
                    // No valid pos this tick — skip (sticky hold was a major ghost source).
                    continue;
                }

                {
                    double horiz = std::hypot(pd.velocity.x, pd.velocity.y);
                    if (!std::isfinite(horiz) || horiz < 8.0) {
                        auto prevIt = prevPlayerMap.find(pd.entity);
                        if (prevIt != prevPlayerMap.end() && prevIt->second->lastUpdateMs &&
                            nowMs > prevIt->second->lastUpdateMs) {
                            const double dt = (double)(nowMs - prevIt->second->lastUpdateMs) / 1000.0;
                            if (dt >= 0.012 && dt <= 0.40) {
                                const double dx = pd.worldLoc.x - prevIt->second->worldLoc.x;
                                const double dy = pd.worldLoc.y - prevIt->second->worldLoc.y;
                                horiz = std::hypot(dx, dy) / dt;
                            }
                        }
                    }
                    if (!std::isfinite(horiz) || horiz < 0.0 || horiz > 2500.0)
                        horiz = 0.0;
                    pd.horizSpeed = (float)horiz;
                }

                // Clock is learned by rate, with UWorld+0x8B0 as the live TimeSeconds pin.
                // Without a clock every mesh reads occluded and ESP stays on the invisible colour.
                if ((visibilityCheck || arcVisibilityCheck) &&
                    g_PlatformVersion != PlatformVersion::Xbox &&
                    is_valid_pointer_optimized(ptrs.UWorld)) {
                    static uint64_t s_lastClockProbeMs = 0;
                    if (Platform::Steam::g_worldClockOffset.load(std::memory_order_relaxed) == 0) {
                        // Needs two samples an interval apart, so it is retried rather than
                        // resolved in one call. Paced so the paired reads stay cheap.
                        if (nowMs - s_lastClockProbeMs > 200) {
                            s_lastClockProbeMs = nowMs;
                            Platform::Steam::ProbeWorldClock(ptrs.UWorld);
                        }
                    } else {
                        const Platform::Steam::WorldClockCandidate c{
                            Platform::Steam::g_worldClockOffset.load(std::memory_order_relaxed),
                            Platform::Steam::g_worldClockIsDouble.load(std::memory_order_relaxed) != 0 };
                        const double worldNow = Platform::Steam::ReadWorldClockAt(ptrs.UWorld, c);
                        if (std::isfinite(worldNow) && worldNow > 1.0)
                            Platform::Steam::g_worldTimeSeconds.store(worldNow, std::memory_order_relaxed);
                    }
                }

                // Learn the render stamp. Only runs while unresolved and only
                // once the clock is live, since the probe grades candidates by
                // how they track it - which is why this never resolved back
                // when the clock itself was still unknown.
                if (visibilityCheck && g_PlatformVersion != PlatformVersion::Xbox &&
                    !Platform::Steam::g_stampResolved.load(std::memory_order_relaxed) &&
                    is_valid_pointer_optimized(pd.skeletalMesh)) {
                    const double worldNow =
                        Platform::Steam::g_worldTimeSeconds.load(std::memory_order_relaxed);

                    // Held off until the raid clock is well past loading. The
                    // first attempt ran at worldNow=2.089, where half the floats
                    // in the object sit near 2.0 and every one of them looks
                    // like a plausible timestamp.
                    if (std::isfinite(worldNow) && worldNow > 45.0) {
                        // Probe the local player's mesh, not a remote one.
                        //
                        // A render stamp only advances while its mesh is actually
                        // being drawn, and the probe grades candidates by whether
                        // they advance. Pinning it to an arbitrary remote player
                        // meant that whenever that player was off-screen or behind
                        // cover - the normal case - nothing in the window moved and
                        // the probe correctly reported no match to a question worth
                        // nothing. Circular, too: picking a rendered mesh needs the
                        // visibility answer we are trying to learn. The local pawn's
                        // mesh is rendered every frame unconditionally, which breaks
                        // the circularity and guarantees the stamp is moving.
                        uintptr_t probeMesh = 0;
                        static uintptr_t s_cycleMesh = 0;
                        static uint64_t s_cycleSinceMs = 0;
                        if (s_cycleMesh && nowMs - s_cycleSinceMs > 4000)
                            s_cycleMesh = 0;
                        if (!s_cycleMesh && is_valid_pointer_optimized(pd.skeletalMesh)) {
                            s_cycleMesh = pd.skeletalMesh;
                            s_cycleSinceMs = nowMs;
                        }
                        probeMesh = s_cycleMesh;
                        if (!probeMesh && is_valid_pointer_optimized(ptrs.LocalPawn)) {
                            const uintptr_t lm = ctx->read<uintptr_t>(
                                ptrs.LocalPawn + Offsets::Get().UPawn.Mesh);
                            if (is_valid_pointer_optimized(lm))
                                probeMesh = lm;
                        }

                        static uint64_t s_lastStampProbeMs = 0;
                        if (probeMesh && nowMs - s_lastStampProbeMs > 250) {
                            s_lastStampProbeMs = nowMs;

                            Platform::Steam::StampCandidate cands[40]{};
                            int candCount = 0;
                            Platform::Steam::StampProbeStats stats{};
                            uint32_t off = 0, key = 0;
                            const auto r = Platform::Steam::ProbeRenderStamp(
                                probeMesh, worldNow, off, key,
                                debugConsoleEnabled ? cands : nullptr,
                                debugConsoleEnabled ? 40 : 0,
                                debugConsoleEnabled ? &candCount : nullptr,
                                debugConsoleEnabled ? &stats : nullptr);

                            if (r == Platform::Steam::StampProbeResult::Found) {
                                Platform::Steam::g_renderStampOffset.store(off, std::memory_order_relaxed);
                                Platform::Steam::g_renderStampKey.store(key, std::memory_order_relaxed);
                                Platform::Steam::g_stampResolved.store(true, std::memory_order_relaxed);
                                if (debugConsoleEnabled) {
                                    char pline[176];
                                    snprintf(pline, sizeof(pline),
                                        "[VisProbe] RESOLVED stampOff=0x%X key=0x%08X worldNow=%.3f",
                                        off, key, worldNow);
                                    DebugSystem::LogConsole(pline);
                                }
                            } else if (r == Platform::Steam::StampProbeResult::Failed) {
                                s_cycleMesh = 0;
                                s_cycleSinceMs = 0;
                            }

                            // Only paired attempts carry information; Pending
                            // is just the first half of a sample.
                            if (debugConsoleEnabled &&
                                r != Platform::Steam::StampProbeResult::Pending) {
                                // Spread across the raid instead of burning the
                                // whole budget in the three seconds after the
                                // clock passes the threshold, as it did last run.
                                static int s_candReports = 0;
                                static uint64_t s_lastCandReportMs = 0;
                                if (s_candReports < 10 &&
                                    (s_lastCandReportMs == 0 || nowMs - s_lastCandReportMs > 5000)) {
                                    ++s_candReports;
                                    s_lastCandReportMs = nowMs;
                                    char hline[256];
                                    snprintf(hline, sizeof(hline),
                                        "[VisCand] localMesh=0x%llX worldNow=%.3f result=%s matches=%d "
                                        "readOk=%d nonzero=%d/%d changed=%d dt=%.3f",
                                        (unsigned long long)probeMesh, worldNow,
                                        r == Platform::Steam::StampProbeResult::Found ? "FOUND" : "FAILED",
                                        candCount, stats.readOk ? 1 : 0, stats.nonZeroDwords,
                                        stats.totalDwords, stats.changedDwords, stats.dt);
                                    DebugSystem::LogConsole(hline);
                                    for (int i = 0; i < candCount; ++i) {
                                        char kdesc[20];
                                        if (cands[i].key == Platform::Steam::kDoubleMarkerKey)
                                            snprintf(kdesc, sizeof(kdesc), "double");
                                        else
                                            snprintf(kdesc, sizeof(kdesc), "0x%08X", cands[i].key);
                                        char cline[224];
                                        snprintf(cline, sizeof(cline),
                                            "[VisCand]   off=0x%04X key=%-10s value=%.4f "
                                            "epochDelta=%.3f advanced=%.4f strict=%d",
                                            cands[i].offset, kdesc, cands[i].value,
                                            cands[i].delta, cands[i].advanced,
                                            cands[i].strict ? 1 : 0);
                                        DebugSystem::LogConsole(cline);
                                    }
                                }
                            }
                        }
                    }
                }

                // Which of the three branches below actually runs. Last raid
                // produced no [VisDiag] at all, which narrows to either the
                // toggle being off or the mesh never resolving - opposite
                // symptoms (everything visible vs everything occluded) that
                // look equally like "the colours do not work".
                if (debugConsoleEnabled) {
                    static bool s_visStateLogged = false;
                    if (!s_visStateLogged) {
                        s_visStateLogged = true;
                        char sline[192];
                        snprintf(sline, sizeof(sline),
                            "[VisState] visibilityCheck=%d hideInvisibleEsp=%d meshValid=%d "
                            "mesh=0x%llX skeletonEsp=%d",
                            visibilityCheck ? 1 : 0, hideInvisibleEsp ? 1 : 0,
                            is_valid_pointer_optimized(pd.skeletalMesh) ? 1 : 0,
                            (unsigned long long)pd.skeletalMesh, visuals5 ? 1 : 0);
                        DebugSystem::LogConsole(sline);
                    }
                }

                if (!visibilityCheck) {
                    pd.isVisible = true;
                } else if (!visCam.valid) {
                    pd.isVisible = true;
                } else {
                    bool sample = true;
                    uint32_t visMethod = 0;
                    float poseLocD = 0.f, poseHeadD = -1.f;
                    if (g_PlatformVersion != PlatformVersion::Xbox) {
                        // Runtime LRTS+XOR key lock (async; no Sleep). Runs until
                        // g_lrts.lrtsOff is set, then ClassifyVisible uses method 5.
                        if (!Platform::Steam::g_lrts.lrtsOff &&
                            is_valid_pointer_optimized(pd.skeletalMesh)) {
                            const double worldNow =
                                Platform::Steam::g_worldTimeSeconds.load(std::memory_order_relaxed);
                            if (std::isfinite(worldNow) && worldNow > 10.0) {
                                const bool wasLocked = Platform::Steam::g_lrts.lrtsOff != 0;
                                Platform::Steam::ScanForLRTS(pd.skeletalMesh, (float)worldNow);
                                if (debugConsoleEnabled && !wasLocked &&
                                    Platform::Steam::g_lrts.lrtsOff) {
                                    char pline[176];
                                    snprintf(pline, sizeof(pline),
                                        "[VisLRTS] RESOLVED lrtsOff=0x%X key=0x%08X lrtOff=0x%X worldNow=%.3f",
                                        Platform::Steam::g_lrts.lrtsOff,
                                        Platform::Steam::g_lrts.keys[0],
                                        Platform::Steam::g_lrts.lrtOff,
                                        worldNow);
                                    DebugSystem::LogConsole(pline);
                                }
                            }
                        }
                        const auto vr = Platform::Steam::ClassifyVisible(pd.skeletalMesh);
                        visMethod = vr.method;
                        if (vr.method)
                            sample = vr.visible;
                        else {
                            Vector3 head{};
                            bool hasHead = false;
                            auto bIt = pd.cachedBones.find(Head);
                            if (bIt != pd.cachedBones.end() && is_valid_bone_position(bIt->second)) {
                                head = bIt->second;
                                hasHead = true;
                            }
                            sample = PoseMovedVisible(pd.entity, pd.worldLoc, head, hasHead,
                                nowMs, &poseLocD, &poseHeadD);
                        }
                    } else {
                        sample = IsVisible(pd.skeletalMesh);
                    }
                    pd.isVisible = UpdateVisibility(pd.entity, sample, nowMs);
                    if (pd.isVisible)
                        visYes++;
                    else
                        visNo++;
                    if (debugConsoleEnabled) {
                        static uint64_t s_poseLogMs = 0;
                        if (nowMs - s_poseLogMs > 4000) {
                            s_poseLogMs = nowMs;
                            char pline[256];
                            snprintf(pline, sizeof(pline),
                                "[VisPose] name=%s method=%u sample=%d stable=%d locD=%.1f headD=%.1f",
                                pd.name.empty() ? "?" : pd.name.c_str(),
                                visMethod, sample ? 1 : 0, pd.isVisible ? 1 : 0,
                                poseLocD, poseHeadD);
                            DebugSystem::LogConsole(pline);
                        }
                    }
                }

                auto it = slowPlayerSnap.find(pd.entity);
                if (it != slowPlayerSnap.end()) {
                    const auto& slowData = it->second;
                    pd.stale = pd.stale || slowData.stale;
                    pd.isZooming = slowData.isZooming;
                    pd.isSprinting = slowData.isSprinting;
                    pd.isDodgeRolling = slowData.isDodgeRolling;
                    pd.isInJump = slowData.isInJump;
                    pd.isReloading = slowData.isReloading;
                    pd.isFiring = slowData.isFiring;
                    pd.characterStance = slowData.characterStance;
                    // Always accept a fresh valid read so bars drop when you deal damage.
                    if (slowData.maxHealth > 0.0) {
                        pd.currentHealth = slowData.currentHealth;
                        pd.maxHealth = slowData.maxHealth;
                    } else if (slowData.currentHealth >= 0.0 && pd.maxHealth > 0.0 &&
                               slowData.currentHealth < pd.currentHealth) {
                        pd.currentHealth = slowData.currentHealth;
                    }
                    if (slowData.maxArmor > 0.0 || slowData.currentArmor > 0.0) {
                        pd.currentArmor = slowData.currentArmor;
                        if (slowData.maxArmor > 0.0)
                            pd.maxArmor = slowData.maxArmor;
                    }
                    pd.isDBNO = slowData.isDbno;
                    pd.isDBNOKnown = slowData.isDbnoKnown;
                    pd.heldItem = slowData.heldItem;
                    pd.weaponLevel = slowData.weaponLevel;
                }

                if (pd.horizSpeed >= 480.f && pd.horizSpeed <= 2500.f)
                    pd.isSprinting = true;
                else if (pd.horizSpeed >= 8.f && pd.horizSpeed < 420.f)
                    pd.isSprinting = false;

                // Drop pawns that no longer belong to this PlayerState (extracted / swapped).
                if (is_valid_pointer_optimized(pd.playerState)) {
                    const auto& psOff = Offsets::Get().UPlayerState;
                    if (psOff.FinishedRound) {
                        const uint8_t finished = ctx->read<uint8_t>(pd.playerState + psOff.FinishedRound);
                        if (ctx->last_status() == TransportStatus::Ok && finished)
                            continue;
                    }
                    uintptr_t ownedPawn = ctx->read_stale<uintptr_t>(
                        pd.playerState + psOff.PawnPrivate, pd.stale);
                    if (!is_valid_pointer_optimized(ownedPawn) && psOff.CurrentPawn)
                        ownedPawn = ctx->read_stale<uintptr_t>(
                            pd.playerState + psOff.CurrentPawn, pd.stale);
                    if (is_valid_pointer_optimized(ownedPawn) && ownedPawn != pd.entity)
                        continue;
                }

                if (usedFreshPos)
                    pd.lastUpdateMs = nowMs;
                // Frozen leftovers stay in this buffer so the bone pass can keep
                // sampling c2w and thaw them. Loop skips drawing them. Dropping them
                // here was the Debug 55 flicker: not in the buffer → freeze set empty
                // → drawn again → frozen again.
                newPlayers.push_back(std::move(pd));
            }

            if (debugConsoleEnabled && visibilityCheck) {
                static uint64_t s_visSumMs = 0;
                if (nowMs - s_visSumMs > 4000) {
                    s_visSumMs = nowMs;
                    char vline[192];
                    snprintf(vline, sizeof(vline),
                        "[VisDiag] visYes=%d visNo=%d flagOff=0x%X",
                        visYes, visNo,
                        g_PlatformVersion != PlatformVersion::Xbox
                            ? Platform::Steam::g_visFlagOff.load(std::memory_order_relaxed) : 0);
                    DebugSystem::LogConsole(vline);
                }
            }

            if (Entity->GetRaidState() != RaidState::InRaid) {
                if (prev) prev->Release();
                return;
            }

            // Actor-scan LivePioneer merge removed — it showed extracted/dead/stale pawns
            // that were no longer in PlayerArray ("ESP of players that aren't there").

            auto isWorldEspItem = [](const PlayerDynamicData& d) -> bool {
                return d.loot || d.container || d.salvage || d.grenade || d.mine || d.trap ||
                       d.extraction || d.raidercache || d.corpse || d.arcprobe || d.npc;
            };

            // No sticky player hold — only draw who is in the current PlayerArray snapshot.

            auto slowWorldSnap = AsyncWorld->GetSnapshot();

            std::vector<WorldDrawInfo> worldDrawSnapshot;
            {
                std::shared_lock<std::shared_mutex> lk(m_dynamicWorldMtx);
                worldDrawSnapshot = m_dynamicWorld;
            }

            std::vector<PlayerDynamicData> worldItems;
            worldItems.reserve(worldDrawSnapshot.empty() && prev ? prev->data.size() : worldDrawSnapshot.size());

            if (worldDrawSnapshot.empty() && prev) {
                // Brief hold only — unlimited copy kept last-raid world ESP in lobby.
                constexpr uint64_t kWorldHoldMs = 900;
                for (const auto& oldPd : prev->data) {
                    if (!isWorldEspItem(oldPd))
                        continue;
                    if (oldPd.lastUpdateMs == 0 || (nowMs - oldPd.lastUpdateMs) > kWorldHoldMs)
                        continue;
                    worldItems.push_back(oldPd);
                }
            }

            for (auto& item : worldDrawSnapshot) {
                if (Entity->GetRaidState() != RaidState::InRaid)
                    break;

                if (!is_valid_pointer_optimized(item.actor))
                    continue;
                // Live pioneers are merged into the player list above, not world ESP.
                if (item.type == WorldItemType::LivePioneer)
                    continue;

                PlayerDynamicData wd{};
                wd.stale = item.stale;
                wd.entity = item.actor;
                wd.worldLoc = item.worldLoc;
                wd.loot = (item.type == WorldItemType::Pickup);
                wd.container = (item.type == WorldItemType::SocketContainer);
                wd.salvage = (item.type == WorldItemType::SalvageContainer);
                wd.grenade = (item.type == WorldItemType::Grenade);
                wd.mine = (item.type == WorldItemType::Mine);
                wd.trap = (item.type == WorldItemType::Trap);
                wd.extraction = (item.type == WorldItemType::Extraction);
                wd.raidercache = (item.type == WorldItemType::RaiderCache);
                wd.npc = (item.type == WorldItemType::Arc);
                if (wd.npc && MatchesLocalAvatar(item.actor, 0, localIds))
                    continue;
                wd.corpse = (item.type == WorldItemType::Corpse);
                wd.arcprobe = (item.type == WorldItemType::Probe);
                wd.threatBit = item.threatBit;
                wd.arcid = item.arcId;
                wd.arcDisplayName = item.displayName;
                wd.arcname = item.actorName;
                wd.ContainerName = item.actorName;
                wd.currentHealth = 0.0;
                wd.maxHealth = 0.0;
                wd.currentArmor = 0.0;
                wd.maxArmor = 0.0;

                // Mesh only — weakspot bones are resolved on demand in the aimbot.
                if (wd.npc) {
                    uintptr_t mesh = ctx->read_stale<uintptr_t>(item.actor + Offsets::Get().UPawn.Mesh, wd.stale);
                    if (!is_valid_pointer_optimized(mesh) && prev) {
                        // Keep last known mesh across brief DMA misses.
                        for (const auto& oldPd : prev->data) {
                            if (oldPd.entity == item.actor && oldPd.npc &&
                                is_valid_pointer_optimized(oldPd.skeletalMesh)) {
                                mesh = oldPd.skeletalMesh;
                                break;
                            }
                        }
                    }
                    if (is_valid_pointer_optimized(mesh)) {
                        wd.skeletalMesh = mesh;
                    }
                    if (!arcVisibilityCheck) {
                        wd.isVisible = true;
                    } else if (!visCam.valid) {
                        wd.isVisible = true;
                    } else {
                        wd.isVisible = UpdateVisibility(wd.entity,
                            IsVisible(wd.skeletalMesh), nowMs);
                    }
                }

                wd.LootName = item.lootName;
                wd.lootamount = item.lootAmount;
                wd.lootCatId = item.lootCatId;
                wd.itemvalue = item.itemValue;
                wd.extractioncat = item.extractionCat;
                if (item.lootWeight > 0.01) {
                    std::unique_lock<std::shared_mutex> wlk(g_ActorWeightMtx);
                    g_ActorWeightMap[item.actor] = item.lootWeight;
                }
                if (wd.extraction && wd.lootCatId == 0xFF) {
                    wd.lootCatId = item.extractionCatId;
                }

                auto swIt = slowWorldSnap.find(item.actor);
                if (swIt != slowWorldSnap.end()) {
                    const auto& slow = swIt->second;
                    wd.stale = wd.stale || slow.stale;
                    wd.destroyed = slow.destroyed;
                    wd.searched = slow.searched;
                    wd.extractionState = slow.extractionState;
                    wd.currentHealth = slow.currentHealth;
                    wd.maxHealth = slow.maxHealth;
                }

                wd.lastUpdateMs = nowMs;
                worldItems.push_back(std::move(wd));
            }

            // Hold world ESP actors that briefly disappear during a slow static rescan.
            if (prev) {
                std::unordered_set<uint64_t> haveWorld;
                haveWorld.reserve(worldItems.size() + 16);
                for (const auto& w : worldItems)
                    haveWorld.insert(w.entity);

                constexpr uint64_t kWorldStickyMs = 900;
                for (const auto& oldPd : prev->data) {
                    if (!isWorldEspItem(oldPd))
                        continue;
                    if (haveWorld.count(oldPd.entity))
                        continue;
                    if (oldPd.lastUpdateMs == 0 || (nowMs - oldPd.lastUpdateMs) > kWorldStickyMs)
                        continue;
                    worldItems.push_back(oldPd);
                    haveWorld.insert(oldPd.entity);
                }
            }

            if (Entity->GetRaidState() != RaidState::InRaid) {
                if (prev) prev->Release();
                // Ensure nothing from this mid-leave pass gets published.
                EnterCriticalSection(&m_playerSwapCS);
                PlayerBuffer* old = m_playerCurrent;
                m_playerCurrent = nullptr;
                LeaveCriticalSection(&m_playerSwapCS);
                if (old) old->Release();
                return;
            }

            std::unordered_set<uint64_t> activeEntities;
            activeEntities.reserve(newPlayers.size());
            for (const auto& p : newPlayers)
                activeEntities.insert(p.entity);

            std::unordered_set<uint64_t> activeActors;
            activeActors.reserve(worldItems.size());
            for (const auto& w : worldItems)
                activeActors.insert(w.entity);

            BoneSyncer->ApplyToCache(newPlayers);

            PlayerBuffer* newBuf = new PlayerBuffer();
            newBuf->data.reserve(newPlayers.size() + worldItems.size());
            for (auto& p : newPlayers)
                newBuf->data.push_back(std::move(p));
            for (auto& w : worldItems)
                newBuf->data.push_back(std::move(w));

            // Left raid while building — drop this buffer, don't revive ghosts.
            if (Entity->GetRaidState() != RaidState::InRaid) {
                newBuf->Release();
                if (prev) prev->Release();
                EnterCriticalSection(&m_playerSwapCS);
                PlayerBuffer* old = m_playerCurrent;
                m_playerCurrent = nullptr;
                LeaveCriticalSection(&m_playerSwapCS);
                if (old) old->Release();
                return;
            }

            int enemyCount = 0;
            for (const auto& pd : newBuf->data) {
                if (!pd.npc && !pd.is_team && (pd.maxHealth <= 0.0 || pd.currentHealth > 0.0))
                    enemyCount++;
            }

            EnterCriticalSection(&m_playerSwapCS);
            PlayerBuffer* old = m_playerCurrent;
            // Always publish — keeping the previous buffer when empty caused phantom ESP.
            m_playerCurrent = newBuf;
            LeaveCriticalSection(&m_playerSwapCS);

            if (old)
                old->Release();

            if (prev)
                prev->Release();

            {
                std::lock_guard<std::mutex> lk2(m_statsMtx);
                m_cachedEnemies = enemyCount;
            }

            {
                auto vIt = m_visibilityCache.begin();
                while (vIt != m_visibilityCache.end()) {
                    const bool keep = activeEntities.count(vIt->first) || activeActors.count(vIt->first);
                    if (!keep)
                        vIt = m_visibilityCache.erase(vIt);
                    else
                        ++vIt;
                }
            }

            AsyncPlayer->PruneStale(activeEntities);
            AsyncWorld->PruneStale(activeActors);
        }

    public:
        void ScanForPCM(uintptr_t uworld, bool allowFullSweep = false) {
            SPOOF_FUNC;
            if (Entity && PcmHasUsableLivePov(Entity->GetPointerSnapshot().PlayerCameraManager))
                return;
            TArray levelsArr;
            if (!ctx->read_array(uworld + Offsets::Get().UWorld.Levels, &levelsArr, sizeof(TArray))) {
                return;
            }
            if (!is_valid_pointer_optimized(levelsArr.Data) || levelsArr.Count <= 0 || levelsArr.Count > 1024) {
                return;
            }

            int scanCount = levelsArr.Count;
            std::vector<uintptr_t> levelPtrs(scanCount);
            if (!ctx->read_array(levelsArr.Data, levelPtrs.data(), scanCount * sizeof(uintptr_t))) {
                return;
            }

            // Exact "PlayerCameraManager" hash plus Blueprint names
            // (BP_PioneerPlayerCameraManager_C). Name bind does not need a live POV;
            // Loop drops frames via IsValidCameraFrame until ViewTarget fills.
            static constexpr uint32_t k_pcmName = TAG_DIGEST("PlayerCameraManager");

            const auto& camOff = Offsets::Get().CameraManager;
            const auto& povOff = Offsets::Get().FMinimalViewInfo;
            const uintptr_t povBase = camOff.ViewTarget + Offsets::Get().FTViewTarget.POV;

            auto defaultFovOk = [&](uintptr_t actor) -> bool {
                const float dfov = ctx->read<float>(actor + camOff.DefaultFOV);
                return std::isfinite(dfov) && dfov >= 40.0f && dfov <= 120.0f;
            };

            auto pcmFieldsOk = [&](uintptr_t actor) -> bool {
                if (!defaultFovOk(actor))
                    return false;
                const float locked = ctx->read<float>(actor + camOff.LockedFov);
                if (!std::isfinite(locked))
                    return false;
                if (locked != 0.0f && (locked < 20.0f || locked > 170.0f))
                    return false;
                return true;
            };

            auto pcmRootInWorld = [&](uintptr_t actor) -> bool {
                const uintptr_t root = ctx->read<uintptr_t>(
                    actor + Offsets::Get().UObject.RootComponent);
                if (!is_valid_pointer_optimized(root))
                    return false;
                return LooksValidWorldPos(GetSceneComponentWorldLocation(root));
            };

            auto nameLooksLikePcm = [&](const std::string& an) -> bool {
                if (an.empty())
                    return false;
                if (TagDigest::Digest(an) == k_pcmName)
                    return true;
                return an.find("CameraManager") != std::string::npos;
            };

            auto considerActor = [&](uint64_t actor, int levelIdx, int actorIdx,
                                     bool checkName, bool allowUnowned) -> bool {
                if (!is_valid_pointer_optimized(actor))
                    return false;

                if (checkName) {
                    const std::string an = NameResolver::GetObjectNameA(ctx->image_base, actor);
                    std::string cn;
                    if (!nameLooksLikePcm(an))
                        cn = NameResolver::GetClassPrivateNameA(ctx->image_base, actor);
                    if (nameLooksLikePcm(an) || nameLooksLikePcm(cn)) {
                        if (PcmHasUsableLivePov(Entity->GetPointerSnapshot().PlayerCameraManager))
                            return true;
#ifdef DEBUG_CONSOLE
                        CamBindDiag::BeginSet(CamBindDiag::Source::Name);
#endif
                        Entity->SetPlayerCameraManager(actor);
                        if (debugConsoleEnabled) {
                            char line[256];
                            snprintf(line, sizeof(line),
                                "[World] PCM found by name: 0x%llX name=%s class=%s level=%d actor=%d",
                                (unsigned long long)actor, an.c_str(), cn.c_str(),
                                levelIdx, actorIdx);
                            DebugSystem::LogConsole(line);
                        }
                        return true;
                    }
                }

                if (LooksLikePlayerCameraManager(actor) ||
                    (allowUnowned && LooksLikePlayerCameraManager(actor, false) && defaultFovOk(actor))) {
                    const uintptr_t pov = actor + povBase;
                    Vector3 loc = ctx->read<Vector3>(pov + povOff.Location);
                    const float fov = ctx->read<float>(pov + povOff.FOV);
                    if (PcmHasUsableLivePov(Entity->GetPointerSnapshot().PlayerCameraManager))
                        return true;
#ifdef DEBUG_CONSOLE
                    CamBindDiag::BeginSet(CamBindDiag::Source::Shape);
#endif
                    Entity->SetPlayerCameraManager(actor);
                    if (debugConsoleEnabled) {
                        char line[256];
                        snprintf(line, sizeof(line),
                            "[World] PCM found by shape: 0x%llX level=%d actor=%d pov=(%.0f,%.0f,%.0f) fov=%.1f%s",
                            (unsigned long long)actor, levelIdx, actorIdx,
                            loc.x, loc.y, loc.z, fov,
                            allowUnowned ? " persistent" : "");
                        DebugSystem::LogConsole(line);
                    }
                    return true;
                }
                return false;
            };

            auto scanLevelActors = [&](uintptr_t level, int levelIdx, int actorCap,
                                       bool checkName, bool allowUnowned, int* budget) -> bool {
                TArray actorArr;
                if (!ctx->read_array(level + Offsets::Get().ULevel.ActorArray, &actorArr, sizeof(TArray)))
                    return false;
                if (!is_valid_pointer_optimized(actorArr.Data) || actorArr.Count <= 0)
                    return false;

                int cap = actorArr.Count;
                if (cap > actorCap)
                    cap = actorCap;
                if (cap > 20000)
                    cap = 20000;

                constexpr int ACTOR_BATCH = 512;
                std::vector<uint64_t> actors(ACTOR_BATCH);
                int actorIdx = 0;
                while (actorIdx < cap) {
                    int batchSize = ACTOR_BATCH;
                    if (actorIdx + batchSize > cap)
                        batchSize = cap - actorIdx;
                    if (!ctx->read_array(actorArr.Data + (size_t)actorIdx * sizeof(uint64_t),
                                         actors.data(), (size_t)batchSize * sizeof(uint64_t))) {
                        actorIdx += batchSize;
                        continue;
                    }
                    for (int j = 0; j < batchSize; ++j) {
                        if (budget && *budget <= 0)
                            return false;
                        const uint64_t actor = actors[j];
                        if (!is_valid_pointer_optimized(actor))
                            continue;
                        if (budget)
                            --(*budget);
                        if (considerActor(actor, levelIdx, actorIdx + j, checkName, allowUnowned))
                            return true;
                    }
                    actorIdx += batchSize;
                }
                return false;
            };

            auto bindUniqueFieldPcm = [&](uintptr_t level, int levelIdx, int* outHits) -> bool {
                TArray actorArr;
                if (!ctx->read_array(level + Offsets::Get().ULevel.ActorArray, &actorArr, sizeof(TArray)))
                    return false;
                if (!is_valid_pointer_optimized(actorArr.Data) || actorArr.Count <= 0)
                    return false;

                int cap = actorArr.Count;
                if (cap > 8192)
                    cap = 8192;

                uintptr_t unique = 0;
                int hits = 0;
                int hitIdx = -1;
                constexpr int ACTOR_BATCH = 512;
                std::vector<uint64_t> actors(ACTOR_BATCH);
                int actorIdx = 0;
                while (actorIdx < cap) {
                    int batchSize = ACTOR_BATCH;
                    if (actorIdx + batchSize > cap)
                        batchSize = cap - actorIdx;
                    if (!ctx->read_array(actorArr.Data + (size_t)actorIdx * sizeof(uint64_t),
                                         actors.data(), (size_t)batchSize * sizeof(uint64_t))) {
                        actorIdx += batchSize;
                        continue;
                    }
                    for (int j = 0; j < batchSize; ++j) {
                        const uint64_t actor = actors[j];
                        if (!is_valid_pointer_optimized(actor))
                            continue;
                        if (!pcmFieldsOk(actor) || !pcmRootInWorld(actor))
                            continue;
                        unique = actor;
                        hitIdx = actorIdx + j;
                        ++hits;
                    }
                    actorIdx += batchSize;
                }
                if (outHits)
                    *outHits = hits;
                if (hits != 1)
                    return false;

                if (PcmHasUsableLivePov(Entity->GetPointerSnapshot().PlayerCameraManager))
                    return true;
#ifdef DEBUG_CONSOLE
                CamBindDiag::BeginSet(CamBindDiag::Source::Field);
#endif
                Entity->SetPlayerCameraManager(unique);
                if (debugConsoleEnabled) {
                    char line[256];
                    snprintf(line, sizeof(line),
                        "[World] PCM found by fields: 0x%llX level=%d actor=%d",
                        (unsigned long long)unique, levelIdx, hitIdx);
                    DebugSystem::LogConsole(line);
                }
                return true;
            };

            // Fields + POV shape on persist. FName on 800-1300 actors every 50-100ms
            // starved DMA so DefaultFOV/root came back 0 (fieldHits=0) for minutes.
            // Names run on fullSweep (~500ms of misses).
            {
                const uintptr_t persistent =
                    ctx->read<uintptr_t>(uworld + Offsets::Get().UWorld.PersistentLevel);
                if (is_valid_pointer_optimized(persistent)) {
                    int fieldHits = 0;
                    if (bindUniqueFieldPcm(persistent, 0, &fieldHits))
                        return;
                    int persistBudget = 8192;
                    if (scanLevelActors(persistent, 0, 8192, allowFullSweep, true, &persistBudget))
                        return;
                    if (debugConsoleEnabled) {
                        static uint64_t s_lastMiss = 0;
                        const uint64_t now = GetTickCount64();
                        if (now - s_lastMiss > 5000) {
                            s_lastMiss = now;
                            TArray actorArr{};
                            ctx->read_array(persistent + Offsets::Get().ULevel.ActorArray, &actorArr, sizeof(TArray));
                            char line[192];
                            snprintf(line, sizeof(line),
                                "[World] PCM miss persist actors=%d data=0x%llX fullSweep=%d fieldHits=%d",
                                actorArr.Count, (unsigned long long)actorArr.Data,
                                allowFullSweep ? 1 : 0, fieldHits);
                            DebugSystem::LogConsole(line);
                        }
                    }
                }
                if (is_valid_pointer_optimized(levelPtrs[0]) &&
                    levelPtrs[0] != persistent) {
                    int lvl0Budget = 8192;
                    if (scanLevelActors(levelPtrs[0], 0, 8192, allowFullSweep, true, &lvl0Budget))
                        return;
                    if (bindUniqueFieldPcm(levelPtrs[0], 0, nullptr))
                        return;
                }
            }

            if (!allowFullSweep)
                return;

            // Resumable rather than restarting at level 0 on every call. A shared budget with a
            // fixed start meant the same early levels consumed all of it on every pass, so on a
            // 129-level streaming map the later levels were never reached even once - the scan
            // was not merely slow, it was deterministically blind to most of the map. Progress
            // carries across calls, so successive passes sweep everything for a cost per pass
            // that does not saturate the DMA link.
            static int s_level = 0;
            static int s_actor = 0;
            static uintptr_t s_world = 0;
            int budget = 1200;

            if (s_world != uworld) {
                s_world = uworld;
                s_level = 0;
                s_actor = 0;
            }
            if (s_level >= scanCount) { s_level = 0; s_actor = 0; }

            while (s_level < scanCount) {
                const int i = s_level;
                if (!is_valid_pointer_optimized(levelPtrs[i])) { ++s_level; s_actor = 0; continue; }

                TArray actorArr;
                if (!ctx->read_array(levelPtrs[i] + Offsets::Get().ULevel.ActorArray, &actorArr, sizeof(TArray))) {
                    ++s_level; s_actor = 0; continue;
                }
                if (!is_valid_pointer_optimized(actorArr.Data) || actorArr.Count <= 0) {
                    ++s_level; s_actor = 0; continue;
                }

                int cap = actorArr.Count;
                if (cap > 20000) cap = 20000;
                if (s_actor >= cap) { ++s_level; s_actor = 0; continue; }

                // Same batching as the world-item scan. A single read of cap*8 bytes was
                // failing on large levels and skipping the entire actor list - which is how
                // two matches never found the camera even though the name hash is correct.
                constexpr int ACTOR_BATCH = 512;
                std::vector<uint64_t> actors(ACTOR_BATCH);

                while (s_actor < cap) {
                    const int batchStart = s_actor;
                    int batchSize = ACTOR_BATCH;
                    if (batchStart + batchSize > cap)
                        batchSize = cap - batchStart;

                    if (!ctx->read_array(actorArr.Data + (size_t)batchStart * sizeof(uint64_t),
                                         actors.data(), (size_t)batchSize * sizeof(uint64_t))) {
                        s_actor = batchStart + batchSize;
                        continue;
                    }

                    for (int j = 0; j < batchSize; ++j) {
                        if (budget <= 0) {
                            s_actor = batchStart + j;
                            return;
                        }
                        const uint64_t actor = actors[j];
                        if (!is_valid_pointer_optimized(actor))
                            continue;
                        --budget;

                        if (considerActor(actor, i, batchStart + j, true, false)) {
                            s_level = 0; s_actor = 0;
                            return;
                        }
                    }
                    s_actor = batchStart + batchSize;
                }

                ++s_level;
                s_actor = 0;
            }

            // Only on a completed sweep. Logging every partial pass turned this one line into
            // the entire console, which buried the diagnostics that explain the failure.
            s_level = 0;
            s_actor = 0;
            if (debugConsoleEnabled) {
                char line[128];
                snprintf(line, sizeof(line),
                    "[World] PCM NOT FOUND after full sweep of %d levels", scanCount);
                DebugSystem::LogConsole(line);
            }
        }

    private:
        uint64_t m_lastPcmScanMs = 0;
        int m_pcmScanFailStreak = 0;

        void IndexStaticScene() {
            SPOOF_FUNC;
            uint64_t nowMs = GetNowMs();

            if (Entity->GetRaidState() != RaidState::InRaid) {
                return;
            }

            PointerCache ptrs = Entity->GetPointerSnapshot();
            uintptr_t uworld = ptrs.UWorld;

            if (!is_valid_pointer_optimized(uworld)) {
                return;
            }

            // Pointer set with empty POV used to stop scanning; Loop then skipped ESP
            // until a UWorld change. Keep persist/fullSweep retry until live POV exists.
            // Do not re-run LooksLike on a live camera (FName starve).
            if (uworld != m_lastStaticUWorld)
                m_pcmScanFailStreak = 0;

            if (uworld != m_lastStaticUWorld && g_PlatformVersion != PlatformVersion::Xbox)
                Platform::Steam::ResetVisibilityProbe();

            if (!PcmHasUsableLivePov(ptrs.PlayerCameraManager)) {
                const bool fullSweep = m_pcmScanFailStreak >= 3;
                const uint64_t interval = fullSweep ? 750ull : 50ull;
                if (nowMs - m_lastPcmScanMs > interval) {
                    m_lastPcmScanMs = nowMs;
                    if (m_pcmScanFailStreak < 1000)
                        ++m_pcmScanFailStreak;
                    ScanForPCM(uworld, fullSweep);
                }
            } else {
                m_pcmScanFailStreak = 0;
            }

            if (uworld != m_lastStaticUWorld) {
                if (m_lastStaticUWorld == 0) {
                    m_lastStaticUWorld = uworld;
                    m_pendingStaticUWorld = 0;
                    m_pendingStaticConfirmCount = 0;
                    ScanForPCM(uworld);
                } else {
                    if (uworld == m_pendingStaticUWorld) {
                        ++m_pendingStaticConfirmCount;
                    } else {
                        m_pendingStaticUWorld = uworld;
                        m_pendingStaticConfirmCount = 1;
                    }

                    // Was 3 — one extra confirm was ~100–200ms of blank ESP on rematch.
                    if (m_pendingStaticConfirmCount < 2) {
                        return;
                    }

                    m_lastStaticUWorld = uworld;
                    m_pendingStaticUWorld = 0;
                    m_pendingStaticConfirmCount = 0;
                    {
                        std::lock_guard<std::mutex> lkDyn(m_dynCachedStaticMtx);
                        m_dynCachedStatic.clear();
                        m_dynLastStaticVersion = -1;
                    }
                    ScanForPCM(uworld);
                }
            } else {
                m_pendingStaticUWorld = 0;
                m_pendingStaticConfirmCount = 0;
            }

            const bool anyWorldFeature =
                Cfg::Scene::WorldESP
                || Cfg::Scene::ItemsESP
                || Cfg::Scene::ARCsESP
                || Cfg::Scene::ContainersESP
                || Cfg::Scene::CorpsesESP
                || Cfg::Scene::RaiderCacheESP
                || Cfg::Scene::SalvageESP
                || Cfg::Scene::GrenadeESP
                || Cfg::Scene::MinesESP
                || Cfg::Scene::TrapsESP
                || Cfg::Scene::ExtractionsESP
                || Cfg::Scene::ArcProbesESP;
            if (!anyWorldFeature) {
                Sleep(50);
                return;
            }

            TArray levelsArr;
            if (!ctx->read_array(uworld + Offsets::Get().UWorld.Levels, &levelsArr, sizeof(TArray)))
                return;
            uintptr_t levelsData = levelsArr.Data;
            int32_t levelsCount = levelsArr.Count;

            std::vector<WorldCacheItem> tempStatic;
            tempStatic.reserve(1024);

            if (!is_valid_pointer_optimized(levelsData) || levelsCount <= 0 || levelsCount > 1024)
                return;

            int safeLevelsCount = (levelsCount > 1024) ? 1024 : levelsCount;
            std::vector<uintptr_t> levelPtrs(safeLevelsCount);
            if (!ctx->read_array(levelsData, levelPtrs.data(), safeLevelsCount * sizeof(uintptr_t)))
                return;

            for (int i = 0; i < safeLevelsCount; ++i) {
                if (Entity->GetRaidState() != RaidState::InRaid)
                    break;

                uintptr_t levelPtr = levelPtrs[i];
                if (!is_valid_pointer_optimized(levelPtr))
                    continue;

                TArray actorArr;
                if (!ctx->read_array(levelPtr + Offsets::Get().ULevel.ActorArray, &actorArr, sizeof(TArray)))
                    continue;
                uintptr_t actorArray = actorArr.Data;
                int32_t actorCount = actorArr.Count;

                if (!is_valid_pointer_optimized(actorArray) || actorCount <= 0)
                    continue;

                int maxActors = (actorCount < 20000) ? actorCount : 20000;

                constexpr int ACTOR_BATCH = 512;
                std::vector<uint64_t> actorBatch(ACTOR_BATCH);

                for (int batchStart = 0; batchStart < maxActors; batchStart += ACTOR_BATCH) {
                    if (Entity->GetRaidState() != RaidState::InRaid)
                        break;

                    int batchEnd = batchStart + ACTOR_BATCH;
                    if (batchEnd > maxActors) batchEnd = maxActors;
                    int batchSize = batchEnd - batchStart;

                    if (!ctx->read_array(actorArray + batchStart * sizeof(uint64_t), actorBatch.data(), batchSize * sizeof(uint64_t)))
                        break;

                for (int j = 0; j < batchSize; ++j) {
                    if (Entity->GetRaidState() != RaidState::InRaid)
                        break;

                    uint64_t actor = actorBatch[j];
                    if (!is_valid_pointer_optimized(actor))
                        continue;

                    std::string actorName = NameResolver::GetObjectNameA(ctx->image_base, actor);
                    if (actorName.empty())
                        continue;

                    static std::unordered_map<std::string, uint32_t> s_nameHashCache;
                    static std::map<std::pair<std::string, size_t>, uint32_t> s_prefixHashCache;
                    static std::unordered_map<std::string, ArcInfoResolved> s_arcResolveCache;
                    static std::mutex s_hashCacheMtx;

                    if (InterlockedExchange(&m_needStaticCacheClear, 0)) {
                        std::lock_guard<std::mutex> lk(s_hashCacheMtx);
                        s_nameHashCache.clear();
                        s_prefixHashCache.clear();
                        s_arcResolveCache.clear();
                    }

                    uint32_t nameHash;
                    {
                        std::lock_guard<std::mutex> lk(s_hashCacheMtx);
                        auto it = s_nameHashCache.find(actorName);
                        if (it != s_nameHashCache.end())
                            nameHash = it->second;
                        else {
                            nameHash = TagDigest::Digest(actorName);
                            s_nameHashCache[actorName] = nameHash;
                        }
                    }
                    auto getPrefixHash = [&](size_t len) -> uint32_t {
                        if (actorName.size() < len) return 0u;
                        std::lock_guard<std::mutex> lk(s_hashCacheMtx);
                        auto key = std::make_pair(actorName, len);
                        auto it = s_prefixHashCache.find(key);
                        if (it != s_prefixHashCache.end())
                            return it->second;
                        uint32_t h = TagDigest::DigestPrefix(actorName, len);
                        s_prefixHashCache[key] = h;
                        return h;
                    };
                    static constexpr uint32_t k_pcm = TAG_DIGEST("PlayerCameraManager");
                    static constexpr uint32_t k_extraction = TAG_DIGEST("Extraction");
                    static constexpr uint32_t k_gmlm = TAG_DIGEST("GameModeLocationMarker");
                    static constexpr uint32_t k_salvage_prefix = TAG_DIGEST("BP_SalvageContainer");
                    static constexpr uint32_t k_socket_prefix = TAG_DIGEST("BP_SocketContainer");
                    static constexpr uint32_t k_extraction_prefix = TAG_DIGEST("BP_SalvageExtractionPoint");
                    static constexpr uint32_t k_pickup = TAG_DIGEST("Pickup");
                    static constexpr uint32_t k_placeable_pickup_prefix = TAG_DIGEST("BP_Placeable_Pickup");
                    static constexpr uint32_t k_pickupbase_prefix = TAG_DIGEST("BP_PickupBase");
                    static constexpr uint32_t k_raider_prefix = TAG_DIGEST("BP_RaiderCache");
                    static constexpr uint32_t k_pioneer = TAG_DIGEST("BP_PioneerCharacter_C");
                    static constexpr uint32_t k_pioneer_prefix = TAG_DIGEST("BP_PioneerCharacter");
                    static constexpr uint32_t k_jumpmine = TAG_DIGEST("BP_WItem_Deployable_JumpMine_Explosive_C");
                    static constexpr uint32_t k_stickygrenade = TAG_DIGEST("BP_WItem_StickyRemoteGrenade_C");
                    static constexpr uint32_t k_hegrenade = TAG_DIGEST("BP_WItem_HEGrenade_C");
                    static constexpr uint32_t k_hegrenade_item = TAG_DIGEST("BP_ItemActor_HEGrenade_C");
                    static const std::string s_deploy = std::string(X("BP_WItem_Deployable"));
                    static const std::string s_trap = std::string(X("Trap"));
                    static constexpr uint32_t k_probe_crashed = TAG_DIGEST("BP_ProbeCrashed_02_C");
                    static constexpr uint32_t k_probe_spawner = TAG_DIGEST("BP_Probe02_SpawnOrchestrator_C");
                    static constexpr uint32_t k_probe_02 = TAG_DIGEST("BP_Probe_02_C");

                    const bool isProbeActor =
                        (nameHash == k_probe_crashed || nameHash == k_probe_spawner || nameHash == k_probe_02) ||
                        (actorName.find("ProbeCrashed") != std::string::npos) ||
                        (actorName.find("Probe02_SpawnOrchestrator") != std::string::npos) ||
                        (actorName.find("SpawnOrchestrator") != std::string::npos &&
                         actorName.find("Probe02") != std::string::npos) ||
                        (actorName.find("Probe_02_C") != std::string::npos &&
                         actorName.find("Crashed") == std::string::npos &&
                         actorName.find("Orchestrator") == std::string::npos);

                    if (nameHash == k_pcm ||
                        actorName.find("CameraManager") != std::string::npos) {
                        if (!PcmHasUsableLivePov(Entity->GetPointerSnapshot().PlayerCameraManager)) {
#ifdef DEBUG_CONSOLE
                            CamBindDiag::BeginSet(CamBindDiag::Source::StaticName);
#endif
                            Entity->SetPlayerCameraManager(actor);
                        }
                        continue;
                    }

                    WorldCacheItem item{};
                    item.stale = false;
                    item.actor = actor;
                    item.arcId = -1;
                    item.actorName = actorName;

                    // A carried or dropped throwable is not a threat - it is
                    // only dangerous while someone has it in hand. Corpses keep
                    // their inventory item actors alive in the level, which is
                    // why a dead player used to wear a Smoke Grenade tag.
                    //
                    // Fails open on a cold or stalled registry: with no held
                    // data at all, "nothing is held" would hide every live
                    // grenade in the raid.
                    const bool threatIsItemActor = IsThreatItemActor(actorName);
                    const bool threatSuppressed =
                        threatIsItemActor &&
                        HeldItems::Fresh() &&
                        !HeldItems::IsHeld((uint64_t)actor);

                    // Every actor whose name would reach a threat branch, item
                    // actor or not. The open question this answers: whether a
                    // grenade in flight spawns as BP_WItem_* (unaffected by the
                    // suppression above) or stays BP_Actor_Throwable_* and is
                    // being hidden. One entry per distinct class per 5s.
                    if (debugConsoleEnabled &&
                        (actorName.find("Grenade") != std::string::npos ||
                         actorName.find("Mine") != std::string::npos ||
                         actorName.find("Trap") != std::string::npos)) {
                        static std::mutex s_threatLogMtx;
                        static std::unordered_map<std::string, uint64_t> s_threatLogMs;
                        bool emit = false;
                        {
                            std::lock_guard<std::mutex> lk(s_threatLogMtx);
                            uint64_t& last = s_threatLogMs[actorName];
                            if (last == 0 || nowMs - last > 5000) { last = nowMs; emit = true; }
                        }
                        if (emit) {
                            char tline[320];
                            snprintf(tline, sizeof(tline),
                                "[ThreatDiag] actor=0x%llX name=\"%s\" itemActor=%d "
                                "regFresh=%d held=%d suppressed=%d",
                                (unsigned long long)actor, actorName.c_str(),
                                threatIsItemActor ? 1 : 0,
                                HeldItems::Fresh() ? 1 : 0,
                                HeldItems::IsHeld((uint64_t)actor) ? 1 : 0,
                                threatSuppressed ? 1 : 0);
                            DebugSystem::LogConsole(tline);
                        }
                    }

                    auto looksLikeExtraction = [&]() -> bool {
                        if (nameHash == k_extraction || nameHash == k_gmlm)
                            return true;
                        if (actorName.size() >= 25 && getPrefixHash(25) == k_extraction_prefix)
                            return true;
                        // Broader name match — elevators/metros/fans often aren't exact prefix hits.
                        std::string lower = actorName;
                        for (char& c : lower) c = (char)tolower((unsigned char)c);
                        if (lower.find("widget") != std::string::npos) return false;
                        if (lower.find("uihover") != std::string::npos) return false;
                        if (lower.find("salvageextraction") != std::string::npos) return true;
                        if (lower.find("extractionpoint") != std::string::npos) return true;
                        if (lower.find("extractpoint") != std::string::npos) return true;
                        if (lower.find("extractionlocation") != std::string::npos) return true;
                        if (lower.find("gamemodelocationmarker") != std::string::npos) return true;
                        if (lower.find("extraction") != std::string::npos &&
                            (lower.find("elevator") != std::string::npos ||
                             lower.find("metro") != std::string::npos ||
                             lower.find("hatch") != std::string::npos ||
                             lower.find("fan") != std::string::npos ||
                             lower.find("gas") != std::string::npos ||
                             lower.find("major") != std::string::npos ||
                             lower.find("minor") != std::string::npos ||
                             lower.find("raider") != std::string::npos))
                            return true;
                        return false;
                    };

                    if (looksLikeExtraction()) {
                        item.type = WorldItemType::Extraction;
                        item.displayName = item.actorName;
                    }
                    else if (actorName.size() >= 19 && getPrefixHash(19) == k_salvage_prefix) {
                        item.type = WorldItemType::SalvageContainer;
                        item.displayName = item.actorName;
                    }
                    else if (actorName.size() >= 18 && getPrefixHash(18) == k_socket_prefix) {
                        item.type = WorldItemType::SocketContainer;
                        item.displayName = item.actorName;
                    }
                    else if (nameHash == k_pickup ||
                             (actorName.size() >= 19 && getPrefixHash(19) == k_placeable_pickup_prefix) ||
                             (actorName.size() >= 13 && getPrefixHash(13) == k_pickupbase_prefix)) {
                        item.type = WorldItemType::Pickup;
                        std::string pretty = PrettyHeldItemName(actorName);
                        if (!pretty.empty())
                            item.displayName = pretty;
                        else if (IsRawBlueprintName(actorName))
                            item.displayName = ResolveThrowableLabel(actorName, "Item");
                        else
                            item.displayName = actorName;
                    }
                    else if (actorName.size() >= 14 && getPrefixHash(14) == k_raider_prefix) {
                        item.type = WorldItemType::RaiderCache;
                        item.displayName = item.actorName;
                    }
                    else if (nameHash == k_jumpmine ||
                             actorName.find("Mine") != std::string::npos) {
                        if (threatSuppressed) continue;
                        item.type = WorldItemType::Mine;
                        item.displayName = ResolveThrowableLabel(actorName, "Mine");
                        item.threatBit = ResolveThreatFilterBit(actorName, ThreatCategory::Mine);
                    }
                    else if (nameHash == k_stickygrenade || nameHash == k_hegrenade ||
                             nameHash == k_hegrenade_item ||
                             actorName.find("Grenade") != std::string::npos) {
                        if (threatSuppressed) continue;
                        item.type = WorldItemType::Grenade;
                        item.displayName = ResolveThrowableLabel(actorName, "Grenade");
                        item.threatBit = ResolveThreatFilterBit(actorName, ThreatCategory::Grenade);
                    }
                    else if ((actorName.rfind(s_deploy, 0) == 0 && actorName.find(s_trap) != std::string::npos) ||
                             actorName.find("Trap") != std::string::npos) {
                        if (threatSuppressed) continue;
                        item.type = WorldItemType::Trap;
                        item.displayName = ResolveThrowableLabel(actorName, "Trap");
                        item.threatBit = ResolveThreatFilterBit(actorName, ThreatCategory::Trap);
                    }
                    else if (isProbeActor) {
                        item.type = WorldItemType::Probe;
                        item.displayName = "Probe";
                    }
                    else if (nameHash == k_pioneer ||
                             (actorName.size() >= 19 && getPrefixHash(19) == k_pioneer_prefix)) {
                        uintptr_t playerState = ctx->read<uintptr_t>(actor + Offsets::Get().UPawn.PlayerState);
                        if (!playerState || !is_valid_pointer_optimized(playerState)) {
                            item.type = WorldItemType::Corpse;
                            item.displayName = item.actorName;
                        }
                        else {
                            // Require PlayerState↔Pawn ownership. Orphan pawns with a leftover
                            // PlayerState pointer were being shown as live players on the map.
                            uintptr_t ownedPawn = ctx->read<uintptr_t>(
                                playerState + Offsets::Get().UPlayerState.PawnPrivate);
                            if (ownedPawn == actor) {
                                item.type = WorldItemType::LivePioneer;
                                item.displayName = item.actorName;
                            } else {
                                item.type = WorldItemType::Corpse;
                                item.displayName = item.actorName;
                            }
                        }
                    }
                    else {
                        ArcInfoResolved arcInfo;
                        {
                            std::lock_guard<std::mutex> lk(s_hashCacheMtx);
                            auto itArc = s_arcResolveCache.find(actorName);
                            if (itArc != s_arcResolveCache.end()) {
                                arcInfo = itArc->second;
                            } else {
                                arcInfo = ResolveArcInfo(actorName);
                                if (arcInfo.index != -1)
                                    s_arcResolveCache[actorName] = arcInfo;
                            }
                        }
                        // index -2 = unknown ARC-like actor (still show in ESP)
                        if (arcInfo.index != -1) {
                            item.type = WorldItemType::Arc;
                            item.displayName = arcInfo.display ? arcInfo.display : "ARC";
                            item.arcId = arcInfo.index;
                        }
                        else {
                            continue;
                        }
                    }

                    uintptr_t root = ctx->read_stale<uintptr_t>(actor + Offsets::Get().UObject.RootComponent, item.stale);
                    if (is_valid_pointer_optimized(root)) {
                        item.worldLoc = GetSceneComponentWorldLocation(root);
                        if (item.worldLoc.IsZero()) {
                            const auto& sc = Offsets::Get().USceneComponent;
                            size_t relOff = (size_t)sc.RelativeLocation;
                            size_t chunkSize = sizeof(Vector3);
                            if (!ctx->read_array(root + relOff, &item.worldLoc, chunkSize))
                                item.stale = true;
                            item.worldLoc = ctx->read_stale<Vector3>(root + relOff, item.stale);
                        }
                    }
                    // Corpses and ARCs: fall back to skeletal mesh when root is missing/bad.
                    if ((item.type == WorldItemType::Corpse || item.type == WorldItemType::Arc) &&
                        (item.worldLoc.IsZero() || !is_valid_bone_position(item.worldLoc))) {
                        uintptr_t mesh = ctx->read_stale<uintptr_t>(actor + Offsets::Get().UPawn.Mesh, item.stale);
                        if (is_valid_pointer_optimized(mesh)) {
                            Vector3 meshLoc = GetSceneComponentWorldLocation(mesh);
                            if (meshLoc.IsZero()) {
                                const auto& sc = Offsets::Get().USceneComponent;
                                meshLoc = ctx->read_stale<Vector3>(mesh + sc.RelativeLocation, item.stale);
                            }
                            if (!meshLoc.IsZero() && is_valid_bone_position(meshLoc))
                                item.worldLoc = meshLoc;
                        }
                    }
                    if (!is_valid_pointer_optimized(root) &&
                        item.type != WorldItemType::Corpse &&
                        item.type != WorldItemType::Arc)
                        continue;
                    if (item.worldLoc.IsZero())
                        continue;
                    if (!is_valid_bone_position(item.worldLoc))
                        continue;

                    if (item.type == WorldItemType::Pickup ||
                        item.type == WorldItemType::SocketContainer ||
                        item.type == WorldItemType::SalvageContainer ||
                        item.type == WorldItemType::RaiderCache)
                    {
                        UObject obj(actor);
                        if (obj.IsValid()) {
                            UIHoverData hover = obj.GetUIHoverData();
                            if (hover.IsValid()) {
                                const auto& oHover = Offsets::Get().UIHoverData;
                                const size_t hoverChunk = (size_t)(oHover.DataAsset + sizeof(uintptr_t));
                                uint8_t hoverBuf[64];
                                if (hoverChunk <= sizeof(hoverBuf) && ctx->read_array((uintptr_t)hover, hoverBuf, hoverChunk)) {
                                    uintptr_t uiHoverDataText = *(uintptr_t*)hoverBuf;
                                    item.lootAmount = *(int32_t*)(hoverBuf + oHover.LootAmount);
                                    uintptr_t dataAssetPtr = *(uintptr_t*)(hoverBuf + oHover.DataAsset);
                                    item.lootName = GetItemNameFromHoverTextPtr(uiHoverDataText);
                                    if (dataAssetPtr && is_valid_pointer_optimized(dataAssetPtr)) {
                                        UItemDataAsset dataAsset(dataAssetPtr);
                                        if (dataAsset.IsValid()) {
                                            item.lootCatId = dataAsset.GetItemCategory();
                                            item.lootWeight = dataAsset.GetWeight();
                                        }
                                    }
                                } else {
                                    item.stale = true;
                                    item.lootName = hover.GetItemName();
                                    int32_t lootAmt = 0;
                                    uintptr_t dataAssetPtr = 0;
                                    if (ReadUIHoverLootAndDataAsset((uintptr_t)hover, lootAmt, dataAssetPtr)) {
                                        item.lootAmount = lootAmt;
                                        if (dataAssetPtr && is_valid_pointer_optimized(dataAssetPtr)) {
                                            UItemDataAsset dataAsset(dataAssetPtr);
                                            if (dataAsset.IsValid()) {
                                                item.lootCatId = dataAsset.GetItemCategory();
                                                item.lootWeight = dataAsset.GetWeight();
                                            }
                                        }
                                    } else {
                                        item.lootAmount = hover.GetLootAmount();
                                        UItemDataAsset dataAsset = hover.GetDataAsset();
                                        if (dataAsset.IsValid()) {
                                            item.lootCatId = dataAsset.GetItemCategory();
                                            item.lootWeight = dataAsset.GetWeight();
                                        }
                                    }
                                }
                                if (!item.lootName.empty())
                                    item.itemValue = GetItemValue(item.lootName);
                            }
                        }
                        if (ctx->last_status() != TransportStatus::Ok)
                            item.stale = true;
                    }

                    if (item.type == WorldItemType::Extraction)
                    {
                        ASalvageExtractionPointBase ext(actor);
                        std::string tagName;
                        if (ext.IsValid()) {
                            uint32_t tagIndex = 0;
                            uintptr_t dataAssetPtr = ext.GetExtractionDataAsset();
                            if (is_valid_pointer_optimized(dataAssetPtr)) {
                                tagIndex = ctx->read_stale<uint32_t>(
                                    dataAssetPtr + Offsets::Get().UExtractionDataAsset.DialogueTag +
                                    Offsets::Get().FGameplayTag.TagName, item.stale);
                            }

                            if (tagIndex != 0 && tagIndex != 0xFFFFFFFF) {
                                std::wstring tagW = DecryptFNameEntry(ctx->image_base, tagIndex);
                                tagName.reserve(tagW.size());
                                for (wchar_t wc : tagW) {
                                    if (wc > 0 && wc < 128) tagName.push_back(static_cast<char>(wc));
                                }
                            }
                        }
                        if (ctx->last_status() != TransportStatus::Ok)
                            item.stale = true;

                        std::string normalized = NormalizeExtractionTag(tagName);
                        // Fall back to actor name so Elevator/Metro/etc. classify even without tag data.
                        if (normalized.empty())
                            normalized = NormalizeExtractionTag(actorName);
                        if (normalized.empty())
                            normalized = std::string("Extraction");
                        item.extractionCat = normalized;
                        item.extractionCatId = GetExtractionTypeId(item.extractionCat);
                        // Second pass on raw actor name if still "Other".
                        if (item.extractionCatId == 8)
                            item.extractionCatId = GetExtractionTypeId(actorName);
                    }

                    item.lastSeenMs = nowMs;
                    tempStatic.push_back(std::move(item));
                }
                }
            }

            if (Entity->GetRaidState() != RaidState::InRaid)
                return;

            // Sticky merge: full actor-name scans are slow; publishing a thin pass
            // makes world ESP vanish/snap every ~10-15s when the scan finishes.
            std::vector<WorldCacheItem> prevStatic;
            {
                std::lock_guard<std::mutex> lk(m_staticWorldMtx);
                prevStatic = m_staticWorld;
            }

            if (tempStatic.empty() && !prevStatic.empty())
                return;

            // Reject obviously broken rescans (DMA miss / name resolve flop).
            if (!prevStatic.empty() && tempStatic.size() * 3 < prevStatic.size() && prevStatic.size() > 24)
                return;

            std::unordered_map<uint64_t, WorldCacheItem> merged;
            merged.reserve(prevStatic.size() + tempStatic.size());

            for (auto& p : prevStatic) {
                if (p.lastSeenMs == 0)
                    p.lastSeenMs = nowMs;
                // LivePioneer must not linger — they are not used for player ESP anymore,
                // but long sticky still polluted world scans.
                const uint64_t holdMs = (p.type == WorldItemType::LivePioneer) ? 400ull : 5000ull;
                if ((nowMs - p.lastSeenMs) <= holdMs)
                    merged[p.actor] = std::move(p);
            }

            for (auto& n : tempStatic) {
                auto it = merged.find(n.actor);
                if (it != merged.end()) {
                    const bool moving = (n.type == WorldItemType::Arc ||
                                         n.type == WorldItemType::Grenade ||
                                         n.type == WorldItemType::LivePioneer);
                    if (!moving && is_valid_bone_position(it->second.worldLoc) &&
                        is_valid_bone_position(n.worldLoc))
                    {
                        const float jump = (float)it->second.worldLoc.distance(n.worldLoc);
                        // Static loot shouldn't teleport — keep previous on glitch reads.
                        if (jump > 1500.0f)
                            n.worldLoc = it->second.worldLoc;
                    }
                }
                n.lastSeenMs = nowMs;
                merged[n.actor] = std::move(n);
            }

            std::vector<WorldCacheItem> published;
            published.reserve(merged.size());
            for (auto& kv : merged)
                published.push_back(std::move(kv.second));

            {
                std::lock_guard<std::mutex> lk(m_staticWorldMtx);
                m_staticWorld = std::move(published);
                InterlockedIncrement(&m_staticWorldVersion);
            }
        }

        void IndexDynamicScene() {
            SPOOF_FUNC;
            if (Entity->GetRaidState() != RaidState::InRaid)
                return;

            LONG curVer = m_staticWorldVersion;
            if (curVer != m_dynLastStaticVersion) {
                std::lock_guard<std::mutex> lk(m_staticWorldMtx);
                std::lock_guard<std::mutex> lkDyn(m_dynCachedStaticMtx);
                m_dynCachedStatic = m_staticWorld;
                m_dynLastStaticVersion = curVer;
            }

            std::vector<WorldCacheItem> snapshot;
            {
                std::lock_guard<std::mutex> lkDyn(m_dynCachedStaticMtx);
                snapshot = m_dynCachedStatic;
            }

            if (snapshot.empty())
                return;

            auto slowWorldSnap = AsyncWorld->GetSnapshot();

            PointerCache ptrs = Entity->GetPointerSnapshot();
            Vector3 localPos = ptrs.RelativeLocation;

            std::vector<WorldDrawInfo> tempDyn;
            tempDyn.reserve(snapshot.size());

            for (auto& it : snapshot) {
                if (Entity->GetRaidState() != RaidState::InRaid)
                    break;

                bool stale = it.stale;
                bool needsLivePos = (it.type == WorldItemType::Arc ||
                                     it.type == WorldItemType::Grenade ||
                                     it.type == WorldItemType::LivePioneer);
                Vector3 worldLoc = it.worldLoc;

                if (needsLivePos && (it.actor != 0) && is_valid_pointer_optimized(it.actor)) {
                    uintptr_t root = ctx->read_stale<uintptr_t>(it.actor + Offsets::Get().UObject.RootComponent, stale);
                    if (is_valid_pointer_optimized(root)) {
                        Vector3 liveLoc = GetSceneComponentWorldLocation(root);
                        if (liveLoc.IsZero() || !is_valid_bone_position(liveLoc)) {
                            if (!ctx->read_array(root + Offsets::Get().USceneComponent.RelativeLocation, &liveLoc, sizeof(Vector3))) {
                                stale = true;
                            }
                            liveLoc = ctx->read_stale<Vector3>(root + Offsets::Get().USceneComponent.RelativeLocation, stale);
                        }
                        if (!liveLoc.IsZero() && is_valid_bone_position(liveLoc))
                            worldLoc = liveLoc;
                    }
                }

                float distM = localPos.distance(worldLoc) / 100.0f;

                bool searched = it.searched;
                uint8_t extractionState = 0;
                bool destroyed = false;
                auto slowIt = slowWorldSnap.find(it.actor);
                if (slowIt != slowWorldSnap.end()) {
                    searched = slowIt->second.searched;
                    extractionState = slowIt->second.extractionState;
                    destroyed = slowIt->second.destroyed;
                }

                WorldDrawInfo info;
                info.stale = stale;
                info.actor = it.actor;
                info.worldLoc = worldLoc;
                info.type = it.type;
                info.displayName = it.displayName;
                info.actorName = it.actorName;
                info.arcId = it.arcId;
                info.searched = searched;
                info.distM = distM;
                info.lootName = it.lootName;
                info.lootAmount = it.lootAmount;
                info.lootCatId = it.lootCatId;
                info.itemValue = it.itemValue;
                info.lootWeight = it.lootWeight;
                info.extractionCat = it.extractionCat;
                info.extractionCatId = it.extractionCatId;
                info.threatBit = it.threatBit;

                if (slowIt != slowWorldSnap.end()) {
                    info.stale = info.stale || slowIt->second.stale;
                    info.currentHealth = slowIt->second.currentHealth;
                    info.maxHealth = slowIt->second.maxHealth;
                }

                tempDyn.push_back(std::move(info));
            }

            if (Entity->GetRaidState() == RaidState::InRaid) {
                std::unique_lock<std::shared_mutex> lk(m_dynamicWorldMtx);
                m_dynamicWorld = std::move(tempDyn);
            }
        }

    public:
        PlayerBuffer* AcquirePlayerBuffer() {
            SPOOF_FUNC;
            EnterCriticalSection(&m_playerSwapCS);
            PlayerBuffer* buf = m_playerCurrent;
            if (buf) buf->AddRef();
            LeaveCriticalSection(&m_playerSwapCS);
            return buf;
        }

        std::vector<WorldDrawInfo> GetWorldSnapshot() {
            std::shared_lock<std::shared_mutex> lk(m_dynamicWorldMtx);
            return m_dynamicWorld;
        }

        std::vector<WorldDrawInfo>* GetDynamicWorldPtr() { return &m_dynamicWorld; }
        std::shared_mutex* GetDynamicWorldMtx() { return &m_dynamicWorldMtx; }

        size_t GetWorldItemCount() {
            std::shared_lock<std::shared_mutex> lk(m_dynamicWorldMtx);
            return m_dynamicWorld.size();
        }

        int GetEnemyCount() {
            std::lock_guard<std::mutex> lk(m_statsMtx);
            return m_cachedEnemies;
        }

        int GetSpectatorCount() {
            std::lock_guard<std::mutex> lk(m_statsMtx);
            return m_cachedSpectators;
        }

        void ClearAll() {
            SPOOF_FUNC;
            {
                EnterCriticalSection(&m_playerSwapCS);
                PlayerBuffer* old = m_playerCurrent;
                m_playerCurrent = nullptr;
                LeaveCriticalSection(&m_playerSwapCS);
                if (old) old->Release();
            }
            {
                std::lock_guard<std::mutex> lk(m_staticWorldMtx);
                m_staticWorld.clear();
                InterlockedIncrement(&m_staticWorldVersion);
            }
            {
                std::lock_guard<std::mutex> lkDyn(m_dynCachedStaticMtx);
                m_dynCachedStatic.clear();
                m_dynLastStaticVersion = -1;
            }
            {
                std::unique_lock<std::shared_mutex> lk(m_dynamicWorldMtx);
                m_dynamicWorld.clear();
            }
            {
                std::lock_guard<std::mutex> lk(m_statsMtx);
                m_cachedEnemies = 0;
                m_cachedSpectators = 0;
            }
            m_visibilityCache.clear();
            RayVis::Clear();
            AsyncPlayer->Clear();
            AsyncWorld->Clear();
            BoneSyncer->Clear();
        }

        LONG GetRenderEpoch() const {
            return m_renderEpoch;
        }

        void ForceReset() {
            SPOOF_FUNC;
            m_lastStaticUWorld = 0;
            m_pendingStaticUWorld = 0;
            m_pendingStaticConfirmCount = 0;
            m_lastPcmScanMs = 0;
            m_pcmScanFailStreak = 0;
            InterlockedIncrement(&m_renderEpoch);
            InterlockedExchange(&m_needStaticCacheClear, 1);
            ClearAll();
            {
                std::unique_lock<std::shared_mutex> wlk(g_ActorWeightMtx);
                g_ActorWeightMap.clear();
            }
        }

        volatile LONG m_needStaticCacheClear = 0;

        void IndexPlayersWithSEH() {
            if (!ctx || ctx->cr3 == 0) return;
            IndexPlayers();
        }

        void IndexStaticSceneWithSEH() {
            if (!ctx || ctx->cr3 == 0) return;
            IndexStaticScene();
        }

        void IndexDynamicSceneWithSEH() {
            if (!ctx || ctx->cr3 == 0) return;
            IndexDynamicScene();
        }

        void LaunchSceneScanner() {
            SPOOF_FUNC;
            CreateThread(nullptr, 0, [](LPVOID self) -> DWORD {
                auto* w = static_cast<CScene*>(self);
                for (;;) {
                    if (Entity->GetRaidState() != RaidState::InRaid) {
                        Sleep(100);
                        continue;
                    }
                    w->IndexStaticSceneWithSEH();
                    PointerCache p = Entity->GetPointerSnapshot();
                    bool needsFastRetry = !is_valid_pointer_optimized(p.UWorld)
                                       || !PcmHasUsableLivePov(p.PlayerCameraManager);
                    // Slow full scans hitch ESP when published; don't thrash every 500ms.
                    Sleep(needsFastRetry ? 50 : 2000);
                }
                return 0;
            }, this, 0, nullptr);
        }

        void LaunchAsyncUpdater() {
            SPOOF_FUNC;
            CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
                for (;;) {
                    if (Entity->GetRaidState() != RaidState::InRaid) {
                        Sleep(100);
                        continue;
                    }
                    AsyncPlayer->CacheWithSEH();
                    AsyncWorld->CacheWithSEH();
                    Sleep(250);
                }
                return 0;
            }, nullptr, 0, nullptr);
        }

        // Runs player merge / bones / live world positions off the Present thread.
        void LaunchFastCacheUpdater() {
            SPOOF_FUNC;
            CreateThread(nullptr, 0, [](LPVOID self) -> DWORD {
                auto* w = static_cast<CScene*>(self);
                for (;;) {
                    static bool s_clearedOutOfRaid = false;
                    if (!Entity || Entity->GetRaidState() != RaidState::InRaid) {
                        // Drop any leftover buffers while in lobby / menu.
                        if (!s_clearedOutOfRaid) {
                            w->ClearAll();
                            s_clearedOutOfRaid = true;
                        }
                        Sleep(50);
                        continue;
                    }
                    s_clearedOutOfRaid = false;
                    try {
                        // Bones first (reads previous player buffer), then merge applies them.
                        if (BoneSyncer)
                            BoneSyncer->CacheWithSEH();
                        w->IndexPlayersWithSEH();
                        w->IndexDynamicSceneWithSEH();
                    } catch (...) {
                    }
                    // ~120 Hz cache is enough for smooth ESP at 144/240 display refresh.
                    Sleep(8);
                }
                return 0;
            }, this, 0, nullptr);
        }
    };

    inline CScene* Scene = nullptr;

    inline void SpawnWorkers() {
        SPOOF_FUNC;
        Entity = new CState();
        AsyncPlayer = new CAsyncPlayerSync();
        AsyncWorld = new CAsyncWorldSync();
        BoneSyncer = new CBoneSync();
        Scene = new CScene();

        AsyncWorld->SetWorldSource(Scene->GetDynamicWorldPtr(), Scene->GetDynamicWorldMtx());
        BoneSyncer->SetPlayerSource(
            []() -> void* { return (void*)Scene->AcquirePlayerBuffer(); },
            [](void* buf) { if (buf) ((CScene::PlayerBuffer*)buf)->Release(); },
            [](void* buf) -> const std::vector<PlayerDynamicData>* {
                return buf ? &((CScene::PlayerBuffer*)buf)->data : nullptr;
            }
        );

        Scene->LaunchSceneScanner();
        Scene->LaunchAsyncUpdater();
        Scene->LaunchFastCacheUpdater();
        // RayVis world-mesh harvest never found occluders on this CL (pub=0 after
        // millions of actors). Vischeck is mesh bRecentlyRendered, not CPU rays.
    }

    inline void HaltWorkers() {
        if (Scene) Scene->ClearAll();
    }

}
