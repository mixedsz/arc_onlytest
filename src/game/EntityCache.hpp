#pragma once
#include "CacheTypes.hpp"
#include "EspReadyDiag.hpp"
#include "sdk/Decryptions.hpp"
#include "settings/Settings.hpp"
#include <cctype>
#include <cstring>
#include <iostream>
#include <iomanip>
#include "dgb/Spoof.h"

namespace NT::Store {

    enum class RaidState {
        WaitingForWorld,
        Transitioning,
        InRaid
    };

    class CState {
    private:
        std::vector<Game> m_entities;
        std::mutex m_entityMtx;

        PointerCache m_pointerCache;
        std::mutex m_pointerMtx;

        CachedCameraData m_camera;
        std::mutex m_cameraMtx;

        volatile LONG m_raidState = (LONG)RaidState::WaitingForWorld;
        uint64_t m_lastUWorld = 0;
        std::string m_currentMapName;

        struct CachedPlayerName {
            std::string name;
            std::wstring squadName;
            uint8_t platform = 0;
        };
        std::unordered_map<uintptr_t, CachedPlayerName> m_playerNameCache;
        std::unordered_map<uintptr_t, int> m_squadIdByPtr;
        int m_nextSquadId = 1;

        int AssignSquadId(uintptr_t squadPtr)
        {
            if (!is_valid_pointer_optimized(squadPtr))
                return 0;
            auto it = m_squadIdByPtr.find(squadPtr);
            if (it != m_squadIdByPtr.end())
                return it->second;
            const int id = m_nextSquadId++;
            m_squadIdByPtr[squadPtr] = id;
            return id;
        }

        static std::wstring SquadLabelFromId(int id)
        {
            if (id <= 0)
                return L"";
            char nbuf[24];
            snprintf(nbuf, sizeof(nbuf), "Team %d", id);
            return std::wstring(nbuf, nbuf + strlen(nbuf));
        }

        uintptr_t m_cachedPlayerController = 0;
        uintptr_t m_cachedLocalPawn = 0;
        uintptr_t m_pcmFieldOffset = 0;
        int m_pointerChainRefreshCounter = 0;
        static constexpr int POINTER_CHAIN_FULL_REFRESH_INTERVAL = 10;
        static constexpr uintptr_t kPcmProbeBegin = 0x3B0;
        static constexpr uintptr_t kPcmProbeEnd = 0xE00;

        uint64_t ReadUWorldFromViewPort() {
            SPOOF_FUNC;

            // GWorld first. The viewport is discovered by walking GObjects and matching
            // object names, and this patch broke the GObjects decryption outright
            // ([OffsetHealth]: "count or chunks pointer invalid"), so that walk yields
            // nothing and every read below it fails - which is why the overlay went
            // completely dark rather than partially wrong. GWorld is a plain pointer in the
            // image and depends on none of that machinery.
            if (ctx && ctx->image_base) {
                uint64_t gw = GetGWorld(ctx->image_base);
                if (gw)
                    return gw;
            }

            PointerCache localPtrs;
            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                localPtrs = m_pointerCache;
            }
            if (!localPtrs.ViewPort || localPtrs.ViewPort < 0x10000ULL)
                return 0;

            for (int attempt = 0; attempt < 3; ++attempt) {
                uint64_t uw = ctx->read<uint64_t>(localPtrs.ViewPort + Offsets::Get().UViewPort.Uworld);
                if (uw && uw >= 0x10000ULL && uw <= 0x7FFFFFFFFFFFULL)
                    return uw;
                if (attempt < 2)
                    Sleep(1);
            }
            return 0;
        }

    public:
        bool IsTransitioning() const {
            return (RaidState)m_raidState != RaidState::InRaid;
        }

        RaidState GetRaidState() const {
            return (RaidState)m_raidState;
        }

        void InvalidateRaidState() {
            SPOOF_FUNC;
            InterlockedExchange(&m_raidState, (LONG)RaidState::Transitioning);

            uintptr_t oldPcm = 0;
            {
                std::lock_guard<std::mutex> lk(m_entityMtx);
                m_entities.clear();
            }
            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                oldPcm = m_pointerCache.PlayerCameraManager;
                uint64_t savedVP = m_pointerCache.ViewPort;
                uint32_t savedVPIdx = m_pointerCache.ViewPortIndex;
                m_pointerCache = PointerCache{};
                m_pointerCache.ViewPort = savedVP;
                m_pointerCache.ViewPortIndex = savedVPIdx;
            }

            m_playerNameCache.clear();
            m_squadIdByPtr.clear();
            m_nextSquadId = 1;
            m_cachedPlayerController = 0;
            m_cachedLocalPawn = 0;
            m_pcmFieldOffset = 0;
            m_lastUWorld = 0;
            m_currentMapName.clear();
            m_pointerChainRefreshCounter = 0;

            {
                std::lock_guard<std::mutex> lk(m_cameraMtx);
                m_camera = CachedCameraData{};
            }
            ResetCameraLastGood();
#ifdef DEBUG_CONSOLE
            CamBindDiag::BeginSet(CamBindDiag::Source::None);
            CamBindDiag::OnPcmStore(oldPcm, 0);
            CamBindDiag::OnPcPublished(0);
            CamBindDiag::OnPawnPublished(0);
            EspReadyDiag::Reset();
            CamBindDiag::ResetGens();
#endif
        }

        void SetTransitioningDone() {
            InterlockedExchange(&m_raidState, (LONG)RaidState::WaitingForWorld);
        }

        // An unresolvable name is not evidence of a lobby. This used to return true for an
        // empty string, which was safe while FName resolution worked and fatal once it did
        // not: with names broken every world reads as "" and gets rejected, so a raid with
        // 18 players in the PlayerArray never entered InRaid. Callers now decide what to do
        // with an unknown name rather than having it silently mean "menu".
        static bool IsNonRaidMap(const std::string& name) {
            if (name.empty())
                return false;

            std::string lower = name;
            for (char& c : lower)
                c = (char)tolower((unsigned char)c);

            // Exact / common lobby & frontend levels.
            static const char* exact[] = {
                "mainmenu", "endofround", "bootlevel", "frontend", "lobby",
                "matchmaking", "characterselect", "character_select",
            };
            for (const char* bl : exact) {
                if (lower == bl)
                    return true;
            }

            // Substring catch-alls (map selection / post-match screens).
            static const char* partial[] = {
                "mainmenu", "frontend", "lobby", "matchmak", "characterselect",
                "endof", "bootlevel", "outpost_hub", "socialhub", "menulevel",
            };
            for (const char* p : partial) {
                if (lower.find(p) != std::string::npos)
                    return true;
            }

            return false;
        }

        // Finds a pawn's skeletal mesh when it is not at the configured offset. One pawn per
        // raid reads back 0xFF02130BFF02130B at UPawn.Mesh with stale=0 - a successful read of
        // a field that is simply not a pointer - and that player is then dropped from the bone
        // pass and never gets a skeleton for the whole match.
        //
        // Guessing a second constant is what this avoids. A SkeletalMeshComponent is
        // identifiable rather than guessable: it is the only thing hanging off a pawn that has
        // both a readable world transform and a bone array that decrypts to a plausible bone
        // count. Anything that satisfies both is the mesh.
        static uintptr_t ProbeMeshOffset(uintptr_t pawn, uintptr_t& outOffset) {
            outOffset = 0;
            if (!is_valid_pointer_optimized(pawn))
                return 0;

            auto validate = [](uintptr_t cand) -> bool {
                if (!is_valid_pointer_optimized(cand))
                    return false;
                if (GetSceneComponentWorldLocation(cand).IsZero())
                    return false;
                BoneArrayDebugInfo dbg = NameResolver::GetBoneArrayDebugInfo(cand);
                return dbg.valid && dbg.numBones >= 20 && dbg.numBones <= 300 &&
                    is_sane_bone_array_pointer(dbg.readArrayPtr);
            };

            // Whatever offset worked last time is tried first, so the sweep below runs once
            // for the build rather than once per affected pawn.
            static std::atomic<uintptr_t> s_learnedOffset{ 0 };
            const uintptr_t learned = s_learnedOffset.load(std::memory_order_relaxed);
            if (learned) {
                const uintptr_t cand = ctx->read<uintptr_t>(pawn + learned);
                if (validate(cand)) {
                    outOffset = learned;
                    return cand;
                }
            }

            // One batched read of the pawn body, examined locally afterwards. Reading each
            // candidate offset individually would be hundreds of round trips against the DMA
            // link for a single pawn.
            constexpr uintptr_t kProbeBegin = 0x300;
            constexpr uintptr_t kProbeEnd = 0x900;
            constexpr size_t kProbeSpan = (size_t)(kProbeEnd - kProbeBegin);
            std::vector<uint8_t> buf(kProbeSpan);
            if (!ctx->read_array(pawn + kProbeBegin, buf.data(), kProbeSpan))
                return 0;

            for (size_t off = 0; off + sizeof(uintptr_t) <= kProbeSpan; off += sizeof(uintptr_t)) {
                uintptr_t cand = 0;
                memcpy(&cand, buf.data() + off, sizeof(cand));
                if (!validate(cand))
                    continue;
                outOffset = kProbeBegin + off;
                s_learnedOffset.store(outOffset, std::memory_order_relaxed);
                return cand;
            }
            return 0;
        }

        // LevelCollections is a TArray of FLevelCollection — pick fullest PlayerArray.
        bool ResolveGameStatePlayers(uint64_t uworld, uintptr_t& outGameState, TArray& outPlayerArr) {
            SPOOF_FUNC;
            outGameState = 0;
            outPlayerArr = {};

            // Throttled trace of each link. This is the first thing that runs after UWorld
            // resolves and it fails silently as a unit - CheckInRaid just returns false and
            // the state machine keeps polling - so a bad LevelCollections, GameState or
            // PlayerArray offset all look identical from outside. Printing the intermediate
            // values says which of the three moved.
            static uint64_t s_lastGsLog = 0;
            const uint64_t nowGs = GetTickCount64();
            const bool traceGs = debugConsoleEnabled && (nowGs - s_lastGsLog) > 10000;
            if (traceGs)
                s_lastGsLog = nowGs;

            // Direct GameState first. LevelCollections at 0x370 reads back as garbage on
            // this build (data=0x202000000000098, count=1873032000), so the collection walk
            // has no chance; UWorld::GameState sits one pointer past AuthorityGameMode and
            // reaches the same object without touching it. Accepted only if its PlayerArray
            // looks sane, which is the same test the collection walk applies.
            const uintptr_t directGsOff = Offsets::Get().UWorld.GameState;
            if (directGsOff != 0) {
                uintptr_t gs = ctx->read<uintptr_t>(uworld + directGsOff);
                if (is_valid_pointer_optimized(gs)) {
                    TArray arr{};
                    if (ctx->read_array(gs + Offsets::Get().AGameState.PlayerArray, &arr, sizeof(TArray)) &&
                        arr.Data && arr.Data >= 0x10000ULL && arr.Count > 0 && arr.Count <= 512) {
                        outGameState = gs;
                        outPlayerArr = arr;
                        if (traceGs) {
                            char gline[192];
                            snprintf(gline, sizeof(gline),
                                "[GSResolve] direct GameState@0x%llX -> 0x%llX players=%d",
                                (unsigned long long)directGsOff,
                                (unsigned long long)gs, arr.Count);
                            DebugSystem::LogConsole(gline);
                        }
                        return true;
                    }
                    if (traceGs) {
                        char gline[192];
                        snprintf(gline, sizeof(gline),
                            "[GSResolve] direct GameState@0x%llX -> 0x%llX but PlayerArray@0x%llX bad "
                            "(data=0x%llX count=%d)",
                            (unsigned long long)directGsOff, (unsigned long long)gs,
                            (unsigned long long)Offsets::Get().AGameState.PlayerArray,
                            (unsigned long long)arr.Data, arr.Count);
                        DebugSystem::LogConsole(gline);
                    }
                } else if (traceGs) {
                    char gline[160];
                    snprintf(gline, sizeof(gline),
                        "[GSResolve] direct GameState@0x%llX invalid (0x%llX)",
                        (unsigned long long)directGsOff, (unsigned long long)gs);
                    DebugSystem::LogConsole(gline);
                }
            }

            TArray levelCollectionsArr{};
            if (!ctx->read_array(uworld + Offsets::Get().UWorld.LevelCollections, &levelCollectionsArr, sizeof(TArray))) {
                if (traceGs)
                    DebugSystem::LogConsole("[GSResolve] LevelCollections read failed");
                return false;
            }
            if (traceGs) {
                // PersistentLevel is included as a control: it is a plain pointer at a known
                // offset, so if it reads sane the UWorld pointer is right and only the
                // collection offset is wrong, rather than the whole base being bad.
                uintptr_t persistentLevel =
                    ctx->read<uintptr_t>(uworld + Offsets::Get().UWorld.PersistentLevel);
                char gline[256];
                snprintf(gline, sizeof(gline),
                    "[GSResolve] uworld=0x%llX persistentLevel@0x%llX=0x%llX (sane=%d) "
                    "LevelCollections@0x%llX data=0x%llX count=%d",
                    (unsigned long long)uworld,
                    (unsigned long long)Offsets::Get().UWorld.PersistentLevel,
                    (unsigned long long)persistentLevel,
                    is_valid_pointer_optimized(persistentLevel) ? 1 : 0,
                    (unsigned long long)Offsets::Get().UWorld.LevelCollections,
                    (unsigned long long)levelCollectionsArr.Data, levelCollectionsArr.Count);
                DebugSystem::LogConsole(gline);
            }
            // Don't require Max >= Count — Max is often unreliable across builds.
            if (!is_valid_pointer_optimized(levelCollectionsArr.Data) ||
                levelCollectionsArr.Count <= 0 || levelCollectionsArr.Count > 16)
                return false;

            const uintptr_t gsOff = Offsets::Get().FLevelCollection.GameState;
            const size_t strides[] = { 0x28, 0x30, 0x38, 0x40 };
            int bestCount = 0;

            for (size_t stride : strides) {
                const int n = (levelCollectionsArr.Count > 8) ? 8 : levelCollectionsArr.Count;
                for (int ci = 0; ci < n; ++ci) {
                    uintptr_t gs = ctx->read<uintptr_t>(levelCollectionsArr.Data + (size_t)ci * stride + gsOff);
                    if (!is_valid_pointer_optimized(gs))
                        continue;
                    TArray arr{};
                    if (!ctx->read_array(gs + Offsets::Get().AGameState.PlayerArray, &arr, sizeof(TArray)))
                        continue;
                    if (!arr.Data || arr.Data < 0x10000ULL || arr.Count <= 0 || arr.Count > 512)
                        continue;
                    if (arr.Count > bestCount) {
                        bestCount = arr.Count;
                        outGameState = gs;
                        outPlayerArr = arr;
                    }
                }
                if (bestCount > 0)
                    break;
            }

            // Fallback: treat Data as first FLevelCollection base (legacy).
            if (bestCount <= 0) {
                uintptr_t gs = ctx->read<uintptr_t>(levelCollectionsArr.Data + gsOff);
                if (is_valid_pointer_optimized(gs)) {
                    TArray arr{};
                    if (ctx->read_array(gs + Offsets::Get().AGameState.PlayerArray, &arr, sizeof(TArray)) &&
                        arr.Data && arr.Data >= 0x10000ULL && arr.Count > 0 && arr.Count <= 512) {
                        outGameState = gs;
                        outPlayerArr = arr;
                        bestCount = arr.Count;
                    }
                }
            }

            if (traceGs) {
                char gline[192];
                snprintf(gline, sizeof(gline),
                    "[GSResolve] gsOff=0x%llX playerArrOff=0x%llX -> gameState=0x%llX playerCount=%d",
                    (unsigned long long)gsOff,
                    (unsigned long long)Offsets::Get().AGameState.PlayerArray,
                    (unsigned long long)outGameState, bestCount);
                DebugSystem::LogConsole(gline);
            }

            return bestCount > 0 && is_valid_pointer_optimized(outGameState);
        }

        void ClearLocalBindOnWorldChange() {
            ResetCameraLastGood();
            m_pcmFieldOffset = 0;
            m_cachedPlayerController = 0;
            m_cachedLocalPawn = 0;
            {
                std::lock_guard<std::mutex> lk(m_cameraMtx);
                m_camera = CachedCameraData{};
            }
            uintptr_t oldPcm = 0;
            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                oldPcm = m_pointerCache.PlayerCameraManager;
                m_pointerCache.PlayerCameraManager = 0;
                m_pointerCache.PlayerController = 0;
                m_pointerCache.LocalPawn = 0;
            }
#ifdef DEBUG_CONSOLE
            CamBindDiag::BeginSet(CamBindDiag::Source::None);
            CamBindDiag::OnPcmStore(oldPcm, 0);
            CamBindDiag::OnPcPublished(0);
            CamBindDiag::OnPawnPublished(0);
            EspReadyDiag::ResetRaidMilestones();
#endif
        }

        // AcknowledgedPawn is 0 until possess. A real PC still has a local flag,
        // a PlayerState back-pointer, a Character, or an identifiable camera manager.
        bool LooksLikePlayerController(uintptr_t cand, uintptr_t expectedPs = 0) {
            SPOOF_FUNC;
            if (!LooksLikeCanonicalUObjectPtr(cand))
                return false;
            const auto& off = Offsets::Get();
            if (expectedPs && g_PlatformVersion != PlatformVersion::Xbox) {
                if (ctx->read<uintptr_t>(cand + Offsets::Steam::UPlayerController_PlayerState) == expectedPs)
                    return true;
            }
            const uintptr_t flagOff = off.PlayerControllerFlags.bIsLocalPlayerController;
            if (flagOff && ctx->read<uint8_t>(cand + flagOff) != 0)
                return true;
            const uintptr_t ack = ctx->read<uintptr_t>(cand + off.UPlayerController.AcknowledgedPawn);
            if (is_valid_pointer_optimized(ack))
                return true;
            if (g_PlatformVersion == PlatformVersion::Xbox)
                return false;
            const uintptr_t character = ctx->read<uintptr_t>(
                cand + Offsets::Steam::UPlayerController_Character);
            if (is_valid_pointer_optimized(character))
                return true;
            const uintptr_t pcm = ctx->read<uintptr_t>(
                cand + Offsets::Steam::UPlayerController_PlayerCameraManager);
            return LooksLikeOwnedPlayerCameraManager(pcm, cand) ||
                LooksLikePlayerCameraManager(pcm, false);
        }

        bool PcmFieldLooksOk(uintptr_t pc) {
            SPOOF_FUNC;
            if (g_PlatformVersion == PlatformVersion::Xbox || !is_valid_pointer_optimized(pc))
                return false;
            const uintptr_t pcm = ctx->read<uintptr_t>(
                pc + Offsets::Steam::UPlayerController_PlayerCameraManager);
            if (!is_valid_pointer_optimized(pcm))
                return false;
            return PcmLooksBindable(pcm, pc);
        }

        // PlayerController -> PlayerCameraManager is not a UPROPERTY. Probe the controller
        // block and accept either a live POV or a PCOwner back-pointer plus DefaultFOV so
        // the camera can be bound before possess fills ViewTarget. Unowned spawn cameras
        // (PCOwner still 0) are accepted on a live POV so rematch is not gated on possess.
        //
        // Do not cache `pc` until it looks like a PlayerController or a PCM bind succeeds.
        // Debug 47: a fake GameInstance produced pc=0xAD8F9680 (LocalPlayers.Count=7).
        // Caching it here skipped PlayerArray bind for the whole spawn delay.
        bool BindPcmFromPlayerController(uintptr_t pc) {
            SPOOF_FUNC;
            if (!LooksLikeCanonicalUObjectPtr(pc))
                return false;

            const bool pcLooksReal = LooksLikePlayerController(pc) || PcmFieldLooksOk(pc);
            if (pcLooksReal)
                m_cachedPlayerController = pc;

            auto accept = [&](uintptr_t cand) -> bool {
                return LooksLikePlayerCameraManager(cand) ||
                    LooksLikeOwnedPlayerCameraManager(cand, pc) ||
                    LooksLikePlayerCameraManager(cand, false);
            };
            auto commit = [&](uintptr_t cand) {
                m_cachedPlayerController = pc;
#ifdef DEBUG_CONSOLE
                CamBindDiag::BeginSet(CamBindDiag::Source::PcField);
#endif
                SetPlayerCameraManager(cand);
            };

            {
                uintptr_t existing = 0;
                {
                    std::lock_guard<std::mutex> lk(m_pointerMtx);
                    existing = m_pointerCache.PlayerCameraManager;
                }
                if (accept(existing) &&
                    (pcLooksReal || LooksLikeOwnedPlayerCameraManager(existing, pc))) {
                    m_cachedPlayerController = pc;
                    return true;
                }
            }

            if (m_pcmFieldOffset) {
                const uintptr_t cand = ctx->read<uintptr_t>(pc + m_pcmFieldOffset);
                if (accept(cand) || PcmLooksBindable(cand, pc, false)) {
                    commit(cand);
                    return true;
                }
                m_pcmFieldOffset = 0;
            }

            if (g_PlatformVersion != PlatformVersion::Xbox) {
                const uintptr_t known = Offsets::Steam::UPlayerController_PlayerCameraManager;
                const uintptr_t cand = ctx->read<uintptr_t>(pc + known);
                if (accept(cand) || PcmLooksBindable(cand, pc, true)) {
                    m_pcmFieldOffset = known;
                    commit(cand);
                    if (debugConsoleEnabled) {
                        char line[192];
                        snprintf(line, sizeof(line),
                            "[LocalChain] pc=0x%llX pcm=0x%llX at PlayerController+0x%llX (known)",
                            (unsigned long long)pc, (unsigned long long)cand,
                            (unsigned long long)known);
                        DebugSystem::LogConsole(line);
                    }
                    return true;
                }
            }

            for (uintptr_t probe = kPcmProbeBegin; probe <= kPcmProbeEnd; probe += 8) {
                const uintptr_t cand = ctx->read<uintptr_t>(pc + probe);
                if (!accept(cand) && !PcmLooksBindableFromProbe(cand, pc))
                    continue;
                m_pcmFieldOffset = probe;
                commit(cand);
                if (debugConsoleEnabled) {
                    char line[192];
                    snprintf(line, sizeof(line),
                        "[LocalChain] pc=0x%llX pcm=0x%llX found at PlayerController+0x%llX",
                        (unsigned long long)pc, (unsigned long long)cand,
                        (unsigned long long)probe);
                    DebugSystem::LogConsole(line);
                }
                return true;
            }
            return false;
        }

        // AcknowledgedPawn is often still 0 while the character streams in. PlayerArray
        // already has the local PlayerState / PawnPrivate at that point.
        uintptr_t FindLocalPawnFromStates(uintptr_t pc, const uintptr_t* states, int n) {
            SPOOF_FUNC;
            if (!is_valid_pointer_optimized(pc) || !states || n <= 0)
                return 0;

            const auto& off = Offsets::Get();
            auto pawnUsable = [&](uintptr_t pawn) -> bool {
                if (!is_valid_pointer_optimized(pawn))
                    return false;
                return is_valid_pointer_optimized(
                    ctx->read<uintptr_t>(pawn + off.UObject.RootComponent));
            };

            const uintptr_t ack =
                ctx->read<uintptr_t>(pc + off.UPlayerController.AcknowledgedPawn);
            if (pawnUsable(ack))
                return ack;

            uintptr_t pawnProbeEnd = off.UPawn.PlayerState;
            if (pawnProbeEnd < 0x2C0 || pawnProbeEnd > 0x500)
                pawnProbeEnd = 0x3C0;
            // APawn::Controller is 0x3E8; PlayerState is 0x3D0. Stopping at PlayerState
            // never reads the controller field, so PlayerArray bind found 0 PCs.
            if (pawnProbeEnd < 0x420)
                pawnProbeEnd = 0x420;
            const uintptr_t rootOff = off.UObject.RootComponent;
            const uintptr_t ownerProbeEnd =
                (rootOff > 0xE0 && rootOff < 0x400) ? rootOff : 0x220;

            for (int i = 0; i < n; ++i) {
                const uintptr_t ps = states[i];
                if (!is_valid_pointer_optimized(ps))
                    continue;
                const uintptr_t pawn = ctx->read<uintptr_t>(ps + off.UPlayerState.PawnPrivate);
                if (!pawnUsable(pawn))
                    continue;

                bool match = false;
                for (uintptr_t p = 0xE0; p < ownerProbeEnd; p += 8) {
                    if (ctx->read<uintptr_t>(ps + p) == pc) {
                        match = true;
                        break;
                    }
                }
                if (!match) {
                    for (uintptr_t p = 0x250; p < pawnProbeEnd; p += 8) {
                        if (ctx->read<uintptr_t>(pawn + p) == pc) {
                            match = true;
                            break;
                        }
                    }
                }
                if (match)
                    return pawn;
            }
            return 0;
        }

        // Viewport and OwningGameInstance are both 0 after this patch (GObjects decryption
        // is dead, so InitializeViewPort never runs). GameState.PlayerArray is already
        // populated at join - walk it for the local APlayerController, then the PCM field.
        bool BindLocalFromPlayerArray(uint64_t uworld) {
            SPOOF_FUNC;
            uintptr_t gameState = 0;
            TArray playerArr{};
            if (!ResolveGameStatePlayers(uworld, gameState, playerArr))
                return false;

            int n = playerArr.Count;
            if (n <= 0)
                return false;
            if (n > 64)
                n = 64;

            std::vector<uintptr_t> states((size_t)n);
            if (!ctx->read_array(playerArr.Data, states.data(), (size_t)n * sizeof(uintptr_t)))
                return false;

            const auto& off = Offsets::Get();
            static uintptr_t s_pawnControllerOff = 0;

            auto controllerFromPawn = [&](uintptr_t pawn) -> uintptr_t {
                if (!is_valid_pointer_optimized(pawn))
                    return 0;
                auto pcOwnsPawn = [&](uintptr_t cand) -> bool {
                    if (!is_valid_pointer_optimized(cand))
                        return false;
                    if (ctx->read<uintptr_t>(cand + off.UPlayerController.AcknowledgedPawn) == pawn)
                        return true;
                    if (g_PlatformVersion == PlatformVersion::Xbox)
                        return false;
                    if (ctx->read<uintptr_t>(cand + Offsets::Steam::UPlayerController_Character) == pawn)
                        return true;
                    const uintptr_t psPawn = ctx->read<uintptr_t>(pawn + off.UPawn.PlayerState);
                    const uintptr_t psPc = ctx->read<uintptr_t>(
                        cand + Offsets::Steam::UPlayerController_PlayerState);
                    return is_valid_pointer_optimized(psPawn) && psPawn == psPc;
                };
                if (s_pawnControllerOff) {
                    const uintptr_t cand = ctx->read<uintptr_t>(pawn + s_pawnControllerOff);
                    if (pcOwnsPawn(cand))
                        return cand;
                    s_pawnControllerOff = 0;
                }
                if (g_PlatformVersion != PlatformVersion::Xbox) {
                    const uintptr_t cand = ctx->read<uintptr_t>(pawn + Offsets::Steam::UPawn_Controller);
                    if (pcOwnsPawn(cand)) {
                        s_pawnControllerOff = Offsets::Steam::UPawn_Controller;
                        return cand;
                    }
                }
                uintptr_t probeEnd = off.UPawn.PlayerState;
                if (probeEnd < 0x2C0 || probeEnd > 0x500)
                    probeEnd = 0x3C0;
                if (probeEnd < 0x420)
                    probeEnd = 0x420;
                for (uintptr_t p = 0x250; p < probeEnd; p += 8) {
                    const uintptr_t cand = ctx->read<uintptr_t>(pawn + p);
                    if (!pcOwnsPawn(cand))
                        continue;
                    s_pawnControllerOff = p;
                    return cand;
                }
                return 0;
            };

            auto looksLikePlayerController = [&](uintptr_t cand, uintptr_t expectedPs = 0) -> bool {
                return LooksLikePlayerController(cand, expectedPs);
            };

            auto ownerControllerFromPlayerState = [&](uintptr_t ps) -> uintptr_t {
                const uintptr_t rootOff = off.UObject.RootComponent;
                const uintptr_t probeEnd = (rootOff > 0xE0 && rootOff < 0x400) ? rootOff : 0x220;
                const uintptr_t localFlagOff = off.PlayerControllerFlags.bIsLocalPlayerController;
                if (g_PlatformVersion != PlatformVersion::Xbox) {
                    const uintptr_t owner = ctx->read<uintptr_t>(ps + Offsets::Steam::UObject_Owner);
                    if (looksLikePlayerController(owner, ps))
                        return owner;
                }
                for (uintptr_t p = 0xE0; p < probeEnd; p += 8) {
                    const uintptr_t cand = ctx->read<uintptr_t>(ps + p);
                    if (!looksLikePlayerController(cand, ps))
                        continue;
                    if (!localFlagOff || ctx->read<uint8_t>(cand + localFlagOff) != 0)
                        return cand;
                    if (g_PlatformVersion != PlatformVersion::Xbox &&
                        ctx->read<uintptr_t>(cand + Offsets::Steam::UPlayerController_PlayerState) == ps)
                        return cand;
                }
                return 0;
            };

            auto isLocalPc = [&](uintptr_t pc) -> bool {
                const uintptr_t flagOff = off.PlayerControllerFlags.bIsLocalPlayerController;
                if (!flagOff)
                    return false;
                return ctx->read<uint8_t>(pc + flagOff) != 0;
            };

            std::vector<uintptr_t> pcs;
            pcs.reserve((size_t)n);
            std::vector<uintptr_t> weakPcs;
            weakPcs.reserve((size_t)n * 2);
            uintptr_t localPc = 0;
            uintptr_t localPawnHint = 0;
            uintptr_t dumpPs = 0, dumpPawn = 0, dumpCtrl = 0, dumpOwner = 0;

            for (int i = 0; i < n; ++i) {
                const uintptr_t ps = states[(size_t)i];
                if (!is_valid_pointer_optimized(ps))
                    continue;

                uintptr_t pc = 0;
                const uintptr_t pawn = ctx->read<uintptr_t>(ps + off.UPlayerState.PawnPrivate);
                uintptr_t ctrl = 0;
                if (is_valid_pointer_optimized(pawn)) {
                    pc = controllerFromPawn(pawn);
                    if (g_PlatformVersion != PlatformVersion::Xbox)
                        ctrl = ctx->read<uintptr_t>(pawn + Offsets::Steam::UPawn_Controller);
                }
                if (!pc)
                    pc = ownerControllerFromPlayerState(ps);

                const uintptr_t owner = (g_PlatformVersion != PlatformVersion::Xbox)
                    ? ctx->read<uintptr_t>(ps + Offsets::Steam::UObject_Owner)
                    : 0;
                if (!dumpPs) {
                    dumpPs = ps;
                    dumpPawn = pawn;
                    dumpCtrl = ctrl;
                    dumpOwner = owner;
                }

                if (pc) {
                    pcs.push_back(pc);
                    if (isLocalPc(pc)) {
                        localPc = pc;
                        if (is_valid_pointer_optimized(pawn))
                            localPawnHint = pawn;
                    }
                    continue;
                }

                if (LooksLikeCanonicalUObjectPtr(ctrl))
                    weakPcs.push_back(ctrl);
                if (LooksLikeCanonicalUObjectPtr(owner) && owner != ctrl)
                    weakPcs.push_back(owner);
            }

            auto cacheLocalPawn = [&](uintptr_t pc) {
                uintptr_t pawn = localPawnHint;
                if (!is_valid_pointer_optimized(pawn))
                    pawn = FindLocalPawnFromStates(pc, states.data(), n);
                if (!is_valid_pointer_optimized(pawn))
                    return;
                m_cachedLocalPawn = pawn;
            };

            if (localPc && BindPcmFromPlayerController(localPc)) {
                cacheLocalPawn(localPc);
                return true;
            }

            for (uintptr_t pc : pcs) {
                if (pc == localPc)
                    continue;
                if (BindPcmFromPlayerController(pc)) {
                    cacheLocalPawn(pc);
                    return true;
                }
            }

            if (localPc) {
                m_cachedPlayerController = localPc;
                cacheLocalPawn(localPc);
                return true;
            }

            for (uintptr_t cand : weakPcs) {
                if (BindPcmFromPlayerController(cand)) {
                    cacheLocalPawn(cand);
                    if (debugConsoleEnabled) {
                        char line[192];
                        snprintf(line, sizeof(line),
                            "[LocalChain] PlayerArray weak pc=0x%llX pcm bound",
                            (unsigned long long)cand);
                        DebugSystem::LogConsole(line);
                    }
                    return true;
                }
            }

            // Spawn: ack/pawn/flag still empty so BindPcm fails, but PlayerArray already
            // has exactly one controller. Cache it so ScanForPCM / possess can fill the
            // rest. Do not write PC+0x4E0 here when PcmLooksBindable failed — Debug 50
            // dumped a different pointer than the persist PlayerCameraManager.
            auto cacheUniquePc = [&](uintptr_t pc, const char* tag) -> bool {
                if (!LooksLikeCanonicalUObjectPtr(pc))
                    return false;
                m_cachedPlayerController = pc;
                cacheLocalPawn(pc);
#ifdef DEBUG_CONSOLE
                if (debugConsoleEnabled) {
                    static uintptr_t s_lastLoggedPc = 0;
                    static uint64_t s_lastLogMs = 0;
                    const uint64_t now = GetTickCount64();
                    const bool first = (s_lastLoggedPc != pc);
                    if (first || (now - s_lastLogMs) >= 1000) {
                        s_lastLoggedPc = pc;
                        s_lastLogMs = now;
                        char line[192];
                        snprintf(line, sizeof(line),
                            "[LocalChain] PlayerArray %s pc=0x%llX cached without live PCM",
                            tag, (unsigned long long)pc);
                        DebugSystem::LogConsole(line);
                    }
                }
#endif
                return true;
            };
            if (pcs.size() == 1 && cacheUniquePc(pcs[0], "unique"))
                return true;
            if (pcs.empty() && weakPcs.size() == 1 && cacheUniquePc(weakPcs[0], "weak"))
                return true;

            if (debugConsoleEnabled) {
                static uint64_t s_lastPaFail = 0;
                const uint64_t now = GetTickCount64();
                if (now - s_lastPaFail > 5000) {
                    s_lastPaFail = now;
                    uintptr_t ownerPs = 0, ack = 0, character = 0, pcm = 0;
                    uint8_t flag = 0;
                    if (is_valid_pointer_optimized(dumpOwner) &&
                        g_PlatformVersion != PlatformVersion::Xbox)
                    {
                        ownerPs = ctx->read<uintptr_t>(
                            dumpOwner + Offsets::Steam::UPlayerController_PlayerState);
                    }
                    const uintptr_t samplePc = !pcs.empty()
                        ? pcs[0]
                        : (!weakPcs.empty()
                            ? weakPcs[0]
                            : (is_valid_pointer_optimized(dumpCtrl) ? dumpCtrl : dumpOwner));
                    if (is_valid_pointer_optimized(samplePc)) {
                        ack = ctx->read<uintptr_t>(
                            samplePc + off.UPlayerController.AcknowledgedPawn);
                        if (g_PlatformVersion != PlatformVersion::Xbox) {
                            character = ctx->read<uintptr_t>(
                                samplePc + Offsets::Steam::UPlayerController_Character);
                            pcm = ctx->read<uintptr_t>(
                                samplePc + Offsets::Steam::UPlayerController_PlayerCameraManager);
                        }
                        const uintptr_t flagOff = off.PlayerControllerFlags.bIsLocalPlayerController;
                        if (flagOff)
                            flag = ctx->read<uint8_t>(samplePc + flagOff);
                    }
                    char line[384];
                    snprintf(line, sizeof(line),
                        "[LocalChain] PlayerArray bind failed states=%d pcs=%d weak=%d "
                        "localPc=0x%llX ctrlOff=0x%llX ps=0x%llX pawn=0x%llX ctrl=0x%llX "
                        "owner=0x%llX ownerPS=0x%llX ack=0x%llX char=0x%llX flag=%u pcm=0x%llX",
                        n, (int)pcs.size(), (int)weakPcs.size(),
                        (unsigned long long)localPc,
                        (unsigned long long)s_pawnControllerOff,
                        (unsigned long long)dumpPs,
                        (unsigned long long)dumpPawn,
                        (unsigned long long)dumpCtrl,
                        (unsigned long long)dumpOwner,
                        (unsigned long long)ownerPs,
                        (unsigned long long)ack,
                        (unsigned long long)character,
                        (unsigned)flag,
                        (unsigned long long)pcm);
                    DebugSystem::LogConsole(line);
                }
            }
            return false;
        }

        // Name-free route to the local controller and camera.
        //
        // ScanForPCM finds the PlayerCameraManager by walking level actors and matching
        // their names, so with FName resolution broken it fails on every pass - 582 times
        // in one session - and UpdatePlayerList bails before publishing a single entity.
        // The full struct dump gives a path that needs no names at all:
        // World -> OwningGameInstance -> LocalPlayers[0] -> PlayerController.
        //
        // The last hop, PlayerController -> PlayerCameraManager, is not in either dump, so
        // it is found by testing rather than assumed: a real camera manager has a plausible
        // world position at POV_Location and a sane field of view at POV_FOV, which no
        // unrelated pointer field satisfies.
        bool ResolveLocalChainFromWorld(uint64_t uworld) {
            SPOOF_FUNC;
            const auto& off = Offsets::Get();

            // Every hop is traced. The first version returned silently at each failure,
            // which produced no [LocalChain] line at all and left no way to tell whether
            // the function had run, or which link broke - the same blind spot that cost a
            // test cycle when it was sitting behind an unreachable gate.
            static uint64_t s_lastChainTrace = 0;
            const uint64_t nowChain = GetTickCount64();
            const bool trace = debugConsoleEnabled && (nowChain - s_lastChainTrace) > 5000;
            if (trace)
                s_lastChainTrace = nowChain;
            auto fail = [&](const char* where, uint64_t value) -> bool {
                if (trace) {
                    char line[192];
                    snprintf(line, sizeof(line), "[LocalChain] stopped at %s (0x%llX)",
                        where, (unsigned long long)value);
                    DebugSystem::LogConsole(line);
                }
                return false;
            };

            if (!is_valid_pointer_optimized(uworld))
                return fail("uworld", uworld);

            // UWorld::OwningGameInstance reads null on this build, and that single dead read
            // takes the whole ESP with it: no game instance means no controller, and
            // UpdatePlayerList used to return before it ever touched the PlayerArray.
            //
            // A fresh rematch is the worst case: GameState already has 11-17 players, but
            // AcknowledgedPawn is still null while the character is streaming in. Requiring
            // pawn+root here rejected every GameInstance candidate for the whole raid, which
            // is why the first rematch sat at realPlayers=0 and the next lobby cycle
            // "fixed" it only when the pawn happened to exist early enough. LocalPlayers +
            // PlayerController is enough to identify the GameInstance; the pawn is optional.
            //
            // Viewport GameInstance is tried first: UWorld+OwningGameInstance is often 0
            // after a rematch, while the viewport pointer that already yields GWorld still
            // holds a live UGameInstance at UViewPort.GameInstance.
            auto chainFromGameInstance = [&](uintptr_t gi, uintptr_t* outPc) -> bool {
                if (!is_valid_pointer_optimized(gi))
                    return false;
                TArray lps{};
                if (!ctx->read_array(gi + off.UGameInstance.LocalPlayers, &lps, sizeof(TArray)))
                    return false;
                // A real UGameInstance has LocalPlayers.Count of 1 (splitscreen 2).
                // Debug 47 accepted Count=7 from a random UWorld pointer and bound
                // pc=0xAD8F9680, which has no PlayerCameraManager and blocked PlayerArray.
                if (!is_valid_pointer_optimized(lps.Data) || lps.Count < 1 || lps.Count > 2)
                    return false;
                if (lps.Max < lps.Count || lps.Max > 4)
                    return false;
                const uintptr_t lp = ctx->read<uintptr_t>(lps.Data);
                if (!is_valid_pointer_optimized(lp))
                    return false;
                const uintptr_t pc = ctx->read<uintptr_t>(lp + off.ULocalPlayer.PlayerController);
                if (!is_valid_pointer_optimized(pc))
                    return false;
                if (!LooksLikePlayerController(pc) && !PcmFieldLooksOk(pc))
                    return false;
                if (outPc)
                    *outPc = pc;
                return true;
            };

            uintptr_t gameInstance = 0;
            uintptr_t vpGi = 0;
            uint64_t viewPort = 0;
            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                viewPort = m_pointerCache.ViewPort;
            }
            if (is_valid_pointer_optimized(viewPort) && off.UViewPort.GameInstance != 0) {
                vpGi = ctx->read<uintptr_t>(viewPort + off.UViewPort.GameInstance);
                if (chainFromGameInstance(vpGi, nullptr))
                    gameInstance = vpGi;
            }
            if (trace) {
                char line[224];
                snprintf(line, sizeof(line),
                    "[LocalChain] viewport=0x%llX vpGi=0x%llX giFromVp=%d",
                    (unsigned long long)viewPort, (unsigned long long)vpGi,
                    gameInstance != 0 ? 1 : 0);
                DebugSystem::LogConsole(line);
            }

            if (!gameInstance && off.UWorld.OwningGameInstance != 0) {
                const uintptr_t configured =
                    ctx->read<uintptr_t>(uworld + off.UWorld.OwningGameInstance);
                if (chainFromGameInstance(configured, nullptr))
                    gameInstance = configured;
                else if (g_PlatformVersion != PlatformVersion::Xbox) {
                    const uintptr_t dec = Platform::Steam::DecryptOwningGameInstance(
                        uworld, (uint32_t)off.UWorld.OwningGameInstance);
                    if (chainFromGameInstance(dec, nullptr)) {
                        gameInstance = dec;
                        if (trace) {
                            char line[192];
                            snprintf(line, sizeof(line),
                                "[LocalChain] GameInstance decrypted at UWorld+0x%llX = 0x%llX",
                                (unsigned long long)off.UWorld.OwningGameInstance,
                                (unsigned long long)dec);
                            DebugSystem::LogConsole(line);
                        }
                    } else {
                        const uintptr_t decOld =
                            Platform::Steam::DecryptOwningGameInstance(uworld, 0x2F0);
                        if (chainFromGameInstance(decOld, nullptr)) {
                            gameInstance = decOld;
                            if (trace)
                                DebugSystem::LogConsole("[LocalChain] GameInstance decrypted at UWorld+0x2F0");
                        }
                    }
                }
            }

            if (!gameInstance) {
                static std::atomic<uintptr_t> s_giOffset{ 0 };
                const uintptr_t knownOff = s_giOffset.load(std::memory_order_relaxed);
                if (knownOff) {
                    const uintptr_t gi = ctx->read<uintptr_t>(uworld + knownOff);
                    if (chainFromGameInstance(gi, nullptr))
                        gameInstance = gi;
                    else
                        s_giOffset.store(0, std::memory_order_relaxed);
                }

                if (!gameInstance) {
                    for (uintptr_t probe = 0x100; probe <= 0x800; probe += 8) {
                        const uintptr_t gi = ctx->read<uintptr_t>(uworld + probe);
                        if (!chainFromGameInstance(gi, nullptr))
                            continue;
                        gameInstance = gi;
                        s_giOffset.store(probe, std::memory_order_relaxed);
                        if (trace) {
                            char line[192];
                            snprintf(line, sizeof(line),
                                "[LocalChain] GameInstance found at UWorld+0x%llX = 0x%llX",
                                (unsigned long long)probe, (unsigned long long)gi);
                            DebugSystem::LogConsole(line);
                        }
                        break;
                    }
                }
            }

            if (!is_valid_pointer_optimized(gameInstance)) {
                if (BindLocalFromPlayerArray(uworld)) {
                    if (trace)
                        DebugSystem::LogConsole("[LocalChain] resolved from PlayerArray (no GameInstance)");
                    return true;
                }
                return fail("GameInstance", gameInstance);
            }

            TArray localPlayers{};
            if (!ctx->read_array(gameInstance + off.UGameInstance.LocalPlayers, &localPlayers, sizeof(TArray)))
                return fail("LocalPlayers read", 0);
            if (!is_valid_pointer_optimized(localPlayers.Data) ||
                localPlayers.Count < 1 || localPlayers.Count > 2 ||
                localPlayers.Max < localPlayers.Count || localPlayers.Max > 4) {
                if (BindLocalFromPlayerArray(uworld)) {
                    if (trace)
                        DebugSystem::LogConsole("[LocalChain] resolved from PlayerArray (GI LocalPlayers rejected)");
                    return true;
                }
                return fail("LocalPlayers array", localPlayers.Data);
            }

            const uintptr_t localPlayer = ctx->read<uintptr_t>(localPlayers.Data);
            if (!is_valid_pointer_optimized(localPlayer))
                return fail("LocalPlayer[0]", localPlayer);

            const uintptr_t pc =
                ctx->read<uintptr_t>(localPlayer + off.ULocalPlayer.PlayerController);
            if (!is_valid_pointer_optimized(pc)) {
                if (BindLocalFromPlayerArray(uworld)) {
                    if (trace)
                        DebugSystem::LogConsole("[LocalChain] resolved from PlayerArray (PC still 0 on LocalPlayer)");
                    return true;
                }
                return fail("PlayerController", pc);
            }

            if (trace) {
                char line[224];
                snprintf(line, sizeof(line),
                    "[LocalChain] gi=0x%llX localPlayers=%d lp=0x%llX pc=0x%llX",
                    (unsigned long long)gameInstance, localPlayers.Count,
                    (unsigned long long)localPlayer, (unsigned long long)pc);
                DebugSystem::LogConsole(line);
            }

            if (BindPcmFromPlayerController(pc))
                return true;

            if (trace) {
                char line[192];
                snprintf(line, sizeof(line),
                    "[LocalChain] pc=0x%llX resolved but no PlayerCameraManager field matched "
                    "in 0x3B0..0xE00",
                    (unsigned long long)pc);
                DebugSystem::LogConsole(line);
            }

            if (BindLocalFromPlayerArray(uworld)) {
                if (trace)
                    DebugSystem::LogConsole("[LocalChain] resolved from PlayerArray (GI PC had no PCM)");
                return true;
            }

            // Keep a real spawn PC even without PCM so pawn-root camera fallback can run.
            // Do not keep a fake PC - BindPcmFromPlayerController no longer caches those.
            return LooksLikePlayerController(pc) || PcmFieldLooksOk(pc);
        }

        bool CheckInRaid() {
            SPOOF_FUNC;
            uint64_t uworld = ReadUWorldFromViewPort();
            if (!uworld || uworld < 0x10000ULL)
                return false;

            uintptr_t gameState = 0;
            TArray playerArr{};
            if (!ResolveGameStatePlayers(uworld, gameState, playerArr))
                return false;

            std::string uworldName = NameResolver::GetObjectNameA(ctx->image_base, uworld);

            // The UWorld's own name is the cheapest end-to-end test of FName resolution
            // there is: it should read as a map name. Logged once per distinct result so a
            // blank or garbled value is obvious, because everything that identifies objects
            // by name - raid detection here, the camera scan, world ESP classes - fails
            // silently and identically when the decode is wrong.
            if (debugConsoleEnabled) {
                static std::string s_lastLoggedName = "\x01";
                if (uworldName != s_lastLoggedName) {
                    s_lastLoggedName = uworldName;
                    char nline[192];
                    snprintf(nline, sizeof(nline), "[NameCheck] UWorld=0x%llX name=\"%s\" len=%zu",
                        (unsigned long long)uworld, uworldName.c_str(), uworldName.size());
                    DebugSystem::LogConsole(nline);
                }
            }

            if (IsNonRaidMap(uworldName))
                return false;

            // With no name to go on, the PlayerArray size is the discriminator: the
            // frontend world holds only the local player, while a live raid holds the whole
            // lobby - measured at 1 versus 16-18. Only used when the name is missing, so
            // normal name-based rejection is unaffected once FName resolution is repaired.
            if (uworldName.empty() && playerArr.Count < 2)
                return false;

            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                m_pointerCache.UWorld = uworld;
                m_pointerCache.lastUpdateMs = GetNowMs();
            }

#ifdef DEBUG_CONSOLE
            if (m_lastUWorld != uworld)
                CamBindDiag::OnWorldIdentity(uworld);
#endif
            if (m_lastUWorld && m_lastUWorld != uworld)
                ClearLocalBindOnWorldChange();

            m_lastUWorld = uworld;
            m_currentMapName = uworldName;

            // Resolved here rather than in UpdatePlayerList, because the render loop only
            // calls UpdatePlayerList once PlayerCameraManager is already valid - so putting
            // the code that finds the camera inside it meant it could never run. This path
            // executes every 200ms regardless.
            ResolveLocalChainFromWorld(uworld);

            InterlockedExchange(&m_raidState, (LONG)RaidState::InRaid);
#ifdef DEBUG_CONSOLE
            EspReadyDiag::OnGeneration(uworld);
            EspReadyDiag::NoteRaidStart();
#endif
            return true;
        }

        bool CheckInRaidLight() {
            SPOOF_FUNC;
            uint64_t uworld = ReadUWorldFromViewPort();
            if (!uworld || uworld < 0x10000ULL)
                return false;

            // Must reject lobby/menu here — previously this only checked GameState,
            // so leaving a match kept InRaid and ESP stayed on screen.
            std::string uworldName = NameResolver::GetObjectNameA(ctx->image_base, uworld);
            if (IsNonRaidMap(uworldName))
                return false;

            uintptr_t gameState = 0;
            TArray playerArr{};
            if (!ResolveGameStatePlayers(uworld, gameState, playerArr))
                return false;

            // Same fallback as CheckInRaid: without a name, leaving a match is detected by
            // the PlayerArray collapsing back to just the local player.
            if (uworldName.empty() && playerArr.Count < 2)
                return false;

            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                m_pointerCache.UWorld = uworld;
                m_pointerCache.lastUpdateMs = GetNowMs();
            }

#ifdef DEBUG_CONSOLE
            if (m_lastUWorld != uworld)
                CamBindDiag::OnWorldIdentity(uworld);
#endif
            if (m_lastUWorld && m_lastUWorld != uworld)
                ClearLocalBindOnWorldChange();

            m_lastUWorld = uworld;
            m_currentMapName = uworldName;

            // Same reason as CheckInRaid: this is a path that actually runs while the
            // camera is still missing.
            if (!is_valid_pointer_optimized(m_pointerCache.PlayerCameraManager) ||
                m_cachedPlayerController == 0)
                ResolveLocalChainFromWorld(uworld);

            return true;
        }

        void UpdatePlayerList() {
            SPOOF_FUNC;
            uint64_t nowMs = GetNowMs();

            uint64_t uworld = ReadUWorldFromViewPort();
            if (!uworld || uworld < 0x10000ULL)
                return;

            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                m_pointerCache.UWorld = uworld;
                m_pointerCache.lastUpdateMs = nowMs;
            }

            PointerCache localPtrs;
            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                localPtrs = m_pointerCache;
            }

            uintptr_t gameState = 0;
            TArray playerArr{};
            if (!ResolveGameStatePlayers(uworld, gameState, playerArr))
                return;

            uintptr_t playerController = 0, localPawn = 0;
            bool needFullChainRead = (m_cachedPlayerController == 0) ||
                                     (m_pointerChainRefreshCounter >= POINTER_CHAIN_FULL_REFRESH_INTERVAL);

            if (needFullChainRead) {
                m_pointerChainRefreshCounter = 0;

                // Fill the controller and camera from the world before falling back to the
                // camera-manager route below, which depends on ScanForPCM having succeeded.
                if (!is_valid_pointer_optimized(localPtrs.PlayerCameraManager) ||
                    m_cachedPlayerController == 0) {
                    ResolveLocalChainFromWorld(uworld);
                    localPtrs = GetPointerSnapshot();
                }

                // PCM/controller are preferred for local-pawn identity, but they must not
                // gate the PlayerArray walk. GameState already has every remote player; the
                // rematch bug was returning here with realPlayers=0 while playerCount was 17.
                uint64_t pcm = localPtrs.PlayerCameraManager;
                if (pcm && pcm >= 0x10000ULL) {
                    playerController = ctx->read<uintptr_t>(pcm + Offsets::Get().CameraManager.PCOwner);
                    if (playerController && playerController >= 0x10000ULL)
                        m_cachedPlayerController = playerController;
                    else
                        playerController = m_cachedPlayerController;
                } else {
                    playerController = m_cachedPlayerController;
                }
            } else {
                m_pointerChainRefreshCounter++;
                playerController = m_cachedPlayerController;
                if (!is_valid_pointer_optimized(localPtrs.PlayerCameraManager) &&
                    is_valid_pointer_optimized(playerController))
                    BindPcmFromPlayerController(playerController);
            }

            if (is_valid_pointer_optimized(static_cast<uint64_t>(playerController))) {
                uintptr_t pawnRead = ctx->read<uintptr_t>(playerController + Offsets::Get().UPlayerController.AcknowledgedPawn);
                if (is_valid_pointer_optimized(static_cast<uint64_t>(pawnRead))) {
                    // Require a sane root — rejects mid-transition garbage.
                    uintptr_t rootCheck = ctx->read<uintptr_t>(pawnRead + Offsets::Get().UObject.RootComponent);
                    if (is_valid_pointer_optimized(static_cast<uint64_t>(rootCheck))) {
                        localPawn = pawnRead;
                        m_cachedLocalPawn = localPawn;
                    } else if (is_valid_pointer_optimized(static_cast<uint64_t>(m_cachedLocalPawn))) {
                        localPawn = m_cachedLocalPawn;
                    }
                } else if (is_valid_pointer_optimized(static_cast<uint64_t>(m_cachedLocalPawn))) {
                    localPawn = m_cachedLocalPawn;
                }
            } else if (is_valid_pointer_optimized(static_cast<uint64_t>(m_cachedLocalPawn))) {
                localPawn = m_cachedLocalPawn;
            }

            uintptr_t localPlayerState = 0;
            if (is_valid_pointer_optimized(static_cast<uint64_t>(localPawn))) {
                localPlayerState = ctx->read<uintptr_t>(localPawn + Offsets::Get().UPawn.PlayerState);
            }

            int safePCount = (playerArr.Count > 512) ? 512 : playerArr.Count;
            std::vector<uintptr_t> playerStatePtrs(safePCount);
            if (!ctx->read_array(playerArr.Data, playerStatePtrs.data(), safePCount * sizeof(uintptr_t)))
                return;

            const auto& offsets = Offsets::Get();

            if (!is_valid_pointer_optimized(static_cast<uint64_t>(localPawn)) &&
                is_valid_pointer_optimized(static_cast<uint64_t>(playerController))) {
                localPawn = FindLocalPawnFromStates(playerController, playerStatePtrs.data(), safePCount);
                if (is_valid_pointer_optimized(static_cast<uint64_t>(localPawn))) {
                    m_cachedLocalPawn = localPawn;
                    if (!is_valid_pointer_optimized(static_cast<uint64_t>(localPlayerState)))
                        localPlayerState = ctx->read<uintptr_t>(localPawn + offsets.UPawn.PlayerState);
                }
            }

            if (!is_valid_pointer_optimized(static_cast<uint64_t>(localPlayerState)) && is_valid_pointer_optimized(static_cast<uint64_t>(localPawn))) {
                for (int i = 0; i < safePCount; ++i) {
                    uintptr_t ps = playerStatePtrs[i];
                    if (!is_valid_pointer_optimized(static_cast<uint64_t>(ps)))
                        continue;
                    uintptr_t pp = ctx->read<uintptr_t>(ps + offsets.UPlayerState.PawnPrivate);
                    if (pp == localPawn) {
                        localPlayerState = ps;
                        break;
                    }
                }
            }

            if (!is_valid_pointer_optimized(static_cast<uint64_t>(localPlayerState)) &&
                is_valid_pointer_optimized(static_cast<uint64_t>(playerController)) &&
                g_PlatformVersion != PlatformVersion::Xbox) {
                localPlayerState = ctx->read<uintptr_t>(
                    playerController + Offsets::Steam::UPlayerController_PlayerState);
            }

            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                m_pointerCache.PlayerController = playerController;
                m_pointerCache.LocalPawn = localPawn;
                m_pointerCache.valid = true;
                m_pointerCache.lastUpdateMs = nowMs;
                m_pointerCache.PlayerArray = playerArr.Data;
                m_pointerCache.PlayerArraySize = playerArr.Count;
            }
#ifdef DEBUG_CONSOLE
            CamBindDiag::OnPcPublished(playerController);
            CamBindDiag::OnPawnPublished(localPawn);
#endif

            Vector3 localRelativeLocation = Vector3(0, 0, 0);
            uintptr_t localSquadPtr = 0;

            if (is_valid_pointer_optimized(static_cast<uint64_t>(localPawn))) {
                uintptr_t localRoot = ctx->read<uintptr_t>(localPawn + Offsets::Get().UObject.RootComponent);
                if (localRoot && localRoot >= 0x10000ULL)
                    localRelativeLocation = GetSceneComponentWorldLocation(localRoot);
                if (localPlayerState && localPlayerState >= 0x10000ULL) {
                    UPlayerState ps(localPlayerState);
                    localSquadPtr = ps.GetSquad();
                    if (is_valid_pointer_optimized(localSquadPtr))
                        AssignSquadId(localSquadPtr);
                }
                {
                    std::lock_guard<std::mutex> lk(m_pointerMtx);
                    m_pointerCache.RelativeLocation = localRelativeLocation;
                }
            }

            std::unordered_map<uintptr_t, Game> existingByPawn;
            {
                std::lock_guard<std::mutex> lk(m_entityMtx);
                existingByPawn.reserve(m_entities.size());
                for (auto& e : m_entities)
                    existingByPawn[e.Entity] = e;
            }

            uintptr_t ackPawn = 0;
            if (is_valid_pointer_optimized(static_cast<uint64_t>(playerController)))
                ackPawn = ctx->read<uintptr_t>(
                    playerController + Offsets::Get().UPlayerController.AcknowledgedPawn);
            uintptr_t pcmNow = 0;
            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                pcmNow = m_pointerCache.PlayerCameraManager;
            }
            const LocalAvatarIds localIds = CaptureLocalAvatarIds(
                localPawn, ackPawn, localPlayerState, playerController, pcmNow);

            std::vector<Game> tempEntities;
            tempEntities.reserve(safePCount);

            for (int i = 0; i < safePCount; ++i) {
                if ((RaidState)m_raidState != RaidState::InRaid)
                    break;

                uintptr_t playerState = playerStatePtrs[i];
                if (!is_valid_pointer_optimized(playerState))
                    continue;

                // Dump PioneerPlayerState::bFinishedRound. Extracted players stay in
                // PlayerArray with a frozen pawn (Debug 51 ticksStatic in the hundreds).
                if (offsets.UPlayerState.FinishedRound) {
                    const uint8_t finished = ctx->read<uint8_t>(
                        playerState + offsets.UPlayerState.FinishedRound);
                    if (ctx->last_status() == TransportStatus::Ok && finished)
                        continue;
                }

                bool stale = false;
                uintptr_t pawn = ctx->read_stale<uintptr_t>(playerState + offsets.UPlayerState.PawnPrivate, stale);
                if (!is_valid_pointer_optimized(pawn) && offsets.UPlayerState.CurrentPawn) {
                    pawn = ctx->read_stale<uintptr_t>(
                        playerState + offsets.UPlayerState.CurrentPawn, stale);
                }

                if (!is_valid_pointer_optimized(pawn))
                    continue;
                // Both gates, always. A stale non-null localPlayerState used to
                // skip the pawn check and leak the local character into ESP.
                if (MatchesLocalAvatar(pawn, playerState, localIds))
                    continue;

                uintptr_t mesh = 0, root = 0, healthComp = 0;
                {
                    uintptr_t meshOff = offsets.UPawn.Mesh;
                    uintptr_t rootOff = offsets.UObject.RootComponent;
                    uintptr_t healthOff = offsets.UPawn.HealthComponent;
                    uintptr_t minOff = meshOff;
                    if (rootOff < minOff) minOff = rootOff;
                    if (healthOff < minOff) minOff = healthOff;
                    uintptr_t maxOff = meshOff;
                    if (rootOff > maxOff) maxOff = rootOff;
                    if (healthOff > maxOff) maxOff = healthOff;
                    size_t span = (size_t)(maxOff - minOff + sizeof(uintptr_t));

                    if (span <= 4096) {
                        std::vector<uint8_t> pawnBuf(span);
                        if (ctx->read_array(pawn + minOff, pawnBuf.data(), span)) {
                            mesh = *(uintptr_t*)(pawnBuf.data() + (meshOff - minOff));
                            root = *(uintptr_t*)(pawnBuf.data() + (rootOff - minOff));
                            healthComp = *(uintptr_t*)(pawnBuf.data() + (healthOff - minOff));
                        } else {
                            stale = true;
                            mesh = ctx->read_stale<uintptr_t>(pawn + meshOff, stale);
                            root = ctx->read_stale<uintptr_t>(pawn + rootOff, stale);
                            healthComp = ctx->read_stale<uintptr_t>(pawn + healthOff, stale);
                        }
                    } else {
                        mesh = ctx->read_stale<uintptr_t>(pawn + meshOff, stale);
                        root = ctx->read_stale<uintptr_t>(pawn + rootOff, stale);
                        healthComp = ctx->read_stale<uintptr_t>(pawn + healthOff, stale);
                    }
                }

                // Root is required — do NOT keep previous entry (that left ghosts on the map).
                if (!is_valid_pointer_optimized(root))
                    continue;
                const uintptr_t meshRaw = mesh;
                Vector3 meshC2w{};
                if (!is_valid_pointer_optimized(mesh)) {
                    auto prevIt = existingByPawn.find(pawn);
                    if (prevIt != existingByPawn.end() && is_valid_pointer_optimized(prevIt->second.SkeletalMesh)) {
                        uintptr_t prevMesh = prevIt->second.SkeletalMesh;
                        // Only reuse if ComponentToWorld still looks like a live mesh.
                        Vector3 c2w = GetSceneComponentWorldLocation(prevMesh);
                        if (!c2w.IsZero())
                            mesh = prevMesh;
                    }
                } else {
                    // A fresh pointer whose ComponentToWorld reads zero is preferred over a
                    // previous mesh that still reads live, but it is never discarded outright.
                    // Zeroing it here cost two players their skeleton for an entire raid:
                    // without a mesh they are dropped from the bone pass, which leaves
                    // bones_valid false, which forces the position-only fallback, and that
                    // fallback deliberately draws no pose. The recovery below only helps a
                    // pawn that already had a good mesh, so a player failing on their first
                    // pass could never come back. One transform read is also a far weaker
                    // test than what the bone pass applies anyway - array decrypt, LOD slot
                    // scoring, geometry coherence - and that pass already copes with a mesh
                    // that cannot produce a pose.
                    meshC2w = GetSceneComponentWorldLocation(mesh);
                    if (meshC2w.IsZero()) {
                        auto prevIt = existingByPawn.find(pawn);
                        if (prevIt != existingByPawn.end() &&
                            is_valid_pointer_optimized(prevIt->second.SkeletalMesh) &&
                            !GetSceneComponentWorldLocation(prevIt->second.SkeletalMesh).IsZero())
                            mesh = prevIt->second.SkeletalMesh;
                    }
                }

                // The configured offset held a non-pointer for this pawn, so go and find the
                // mesh. Budgeted per pawn: a pawn that genuinely has no mesh - already dead, or
                // some class that carries none - must not be swept on every pass forever.
                uintptr_t probedOffset = 0;
                if (!is_valid_pointer_optimized(mesh)) {
                    struct ProbeState { int attempts = 0; uint64_t lastMs = 0; };
                    static std::unordered_map<uintptr_t, ProbeState> s_meshProbe;
                    if (s_meshProbe.size() > 256)
                        s_meshProbe.clear();
                    auto& st = s_meshProbe[pawn];
                    const uint64_t nowProbe = GetTickCount64();
                    if (st.attempts < 6 && (nowProbe - st.lastMs) > 1000) {
                        st.lastMs = nowProbe;
                        st.attempts++;
                        const uintptr_t probed = ProbeMeshOffset(pawn, probedOffset);
                        if (is_valid_pointer_optimized(probed)) {
                            mesh = probed;
                            st.attempts = 0;
                            // Logged once per discovered offset: if this reports the same value
                            // every raid, it can become a second configured offset and the sweep
                            // stops being needed at all.
                            if (debugConsoleEnabled) {
                                static uintptr_t s_lastReported = 0;
                                if (probedOffset != s_lastReported) {
                                    s_lastReported = probedOffset;
                                    char pline[192];
                                    snprintf(pline, sizeof(pline),
                                        "[MeshProbe] found mesh at pawn+0x%llX (configured 0x%llX) mesh=0x%llX",
                                        (unsigned long long)probedOffset,
                                        (unsigned long long)Offsets::Get().UPawn.Mesh,
                                        (unsigned long long)probed);
                                    DebugSystem::LogConsole(pline);
                                }
                            }
                        }
                    }
                }

                // A pawn that reaches here with no mesh loses its skeleton entirely, and until
                // now that happened silently - withMesh simply came up short. Reporting the raw
                // read separates the two causes: a zero or non-pointer meshRaw means the field
                // is genuinely absent at UPawn.Mesh and the offset needs a fallback, while a
                // pointer-valid meshRaw reaching here means something downstream rejected it.
                if (debugConsoleEnabled && !is_valid_pointer_optimized(mesh)) {
                    static uint64_t s_lastMeshResolveMs = 0;
                    const uint64_t nowMr = GetTickCount64();
                    if ((nowMr - s_lastMeshResolveMs) > 3000) {
                        s_lastMeshResolveMs = nowMr;
                        char mline[256];
                        snprintf(mline, sizeof(mline),
                            "[MeshResolve] pawn=0x%llX meshOff=0x%llX meshRaw=0x%llX rawSane=%d "
                            "c2w=(%.0f,%.0f,%.0f) stale=%d",
                            (unsigned long long)pawn,
                            (unsigned long long)Offsets::Get().UPawn.Mesh,
                            (unsigned long long)meshRaw,
                            is_valid_pointer_optimized(meshRaw) ? 1 : 0,
                            meshC2w.x, meshC2w.y, meshC2w.z, stale ? 1 : 0);
                        DebugSystem::LogConsole(mline);
                    }
                }

                Game entity;
                entity.Entity = pawn;
                entity.PlayerState = playerState;
                entity.SkeletalMesh = mesh;
                entity.RootComponent = root;
                entity.IsPioneer = true;
                entity.HealthComponent = healthComp;
                entity.stale = stale;

                auto nameIt = m_playerNameCache.find(playerState);
                UPlayerState ps(playerState);
                const uintptr_t squadPtr = ps.GetSquad();
                entity.SquadPtr = squadPtr;
                entity.TeamIndex = AssignSquadId(squadPtr);

                if (nameIt != m_playerNameCache.end()) {
                    entity.Name = nameIt->second.name;
                    entity.SquadName = nameIt->second.squadName;
                    entity.Platform = nameIt->second.platform;
                    {
                        const uint8_t fresh = ps.GetPlayerPlatform();
                        if (fresh != 0)
                            entity.Platform = fresh;
                        nameIt->second.platform = entity.Platform;
                    }
                } else {
                    entity.Name = ps.GetPlayerName();
                    const bool nameRead = (ctx->last_status() == TransportStatus::Ok);
                    if (!nameRead)
                        entity.stale = true;
                    entity.SquadName = ps.GetSquadName();
                    entity.Platform = ps.GetPlayerPlatform();

                    if (debugConsoleEnabled && entity.Name.empty()) {
                        static uint64_t s_lastNameDiag = 0;
                        const uint64_t nowNd = GetTickCount64();
                        if (nowNd - s_lastNameDiag > 3000) {
                            s_lastNameDiag = nowNd;
                            const uintptr_t nameOff = Offsets::Get().UPlayerState.PlayerName;
                            const uintptr_t p0 = ctx->read<uintptr_t>(playerState + nameOff);
                            const int32_t n0 = ctx->read<int32_t>(playerState + nameOff + 8);
                            const uintptr_t p1 = ctx->read<uintptr_t>(playerState + nameOff + 0x18);
                            const int32_t n1 = ctx->read<int32_t>(playerState + nameOff + 0x18 + 8);
                            const uintptr_t p2 = ctx->read<uintptr_t>(playerState + 0x4A8);
                            const int32_t n2 = ctx->read<int32_t>(playerState + 0x4A8 + 8);
                            char nline[256];
                            snprintf(nline, sizeof(nline),
                                "[NameDiag] ps=0x%llX @0x%llX ptr=0x%llX len=%d @+0x18 ptr=0x%llX len=%d @0x4A8 ptr=0x%llX len=%d",
                                (unsigned long long)playerState,
                                (unsigned long long)nameOff,
                                (unsigned long long)p0, (int)n0,
                                (unsigned long long)p1, (int)n1,
                                (unsigned long long)p2, (int)n2);
                            DebugSystem::LogConsole(nline);
                        }
                    }

                    // Only commit a name that actually arrived. GetPlayerName() returns ""
                    // both for a failed read and for a name that has not replicated yet,
                    // which is the normal state for the first passes after someone joins.
                    // This cache was previously written unconditionally and is never
                    // refreshed, so that "" pinned a late joiner as nameless for the rest
                    // of the raid - and the bone-cache ghost filter drops every entity with
                    // an empty name, so they were never given bones and never rendered.
                    // Leaving a miss uncached costs one retry on the next pass; the failure
                    // path in GetPlayerName() bails after two reads.
                    if (nameRead && !entity.Name.empty())
                        m_playerNameCache[playerState] = { entity.Name, entity.SquadName, entity.Platform };
                }

                if (entity.SquadName.empty() || entity.SquadName == L"NULL")
                    entity.SquadName = SquadLabelFromId(entity.TeamIndex);
                entity.is_team = (is_valid_pointer_optimized(localSquadPtr) &&
                    is_valid_pointer_optimized(squadPtr) && squadPtr == localSquadPtr);

                if (debugConsoleEnabled && is_valid_pointer_optimized(squadPtr)) {
                    static uint64_t s_squadDiagMs = 0;
                    const uint64_t nowSd = GetTickCount64();
                    if ((nowSd - s_squadDiagMs) > 5000) {
                        s_squadDiagMs = nowSd;
                        std::string titleUtf8(entity.SquadName.begin(), entity.SquadName.end());
                        char sline[256];
                        snprintf(sline, sizeof(sline),
                            "[SquadDiag] name=%s title=%s seed=%d ptr=0x%llX id=%d team=%d",
                            entity.Name.empty() ? "<empty>" : entity.Name.c_str(),
                            titleUtf8.c_str(),
                            (int)ps.GetSquadSeed(),
                            (unsigned long long)squadPtr,
                            entity.TeamIndex,
                            entity.is_team ? 1 : 0);
                        DebugSystem::LogConsole(sline);
                    }
                }

                tempEntities.push_back(std::move(entity));
            }

            const size_t entityCount = tempEntities.size();
            int withMesh = 0;
            for (const auto& e : tempEntities)
                if (is_valid_pointer_optimized(e.SkeletalMesh)) withMesh++;

            if (debugConsoleEnabled) {
                static uint64_t s_lastEntLog = 0;
                uint64_t nowEl = GetTickCount64();
                if ((nowEl - s_lastEntLog) > 2000) {
                    s_lastEntLog = nowEl;
                    const char* raidStr =
                        ((RaidState)m_raidState == RaidState::InRaid) ? "InRaid" :
                        ((RaidState)m_raidState == RaidState::Transitioning) ? "Transition" : "Lobby";
                    char line[256];
                    snprintf(line, sizeof(line),
                        "[EntityCache] raid=%s playerArray=%d entities=%zu withMesh=%d localPawn=0x%llX",
                        raidStr, safePCount, entityCount, withMesh,
                        (unsigned long long)localPawn);
                    DebugSystem::LogConsole(line);
                }
            }

            if ((RaidState)m_raidState == RaidState::InRaid) {
                std::lock_guard<std::mutex> lk(m_entityMtx);
                m_entities = std::move(tempEntities);
            }
#ifdef DEBUG_CONSOLE
            if ((RaidState)m_raidState == RaidState::InRaid && entityCount > 0)
                EspReadyDiag::NoteSnapshot();
#endif
        }

        // Disabled: actor-scan supplements created phantom players (corpses / stale pawns).
        void SupplementPioneers(std::vector<Game> /*extras*/) {
        }

        std::vector<Game> GetSnapshot() {
            SPOOF_FUNC;
            std::lock_guard<std::mutex> lk(m_entityMtx);
            return m_entities;
        }

        size_t GetEntityCount() {
            std::lock_guard<std::mutex> lk(m_entityMtx);
            return m_entities.size();
        }

        PointerCache GetPointerSnapshot() {
            SPOOF_FUNC;
            std::lock_guard<std::mutex> lk(m_pointerMtx);
            return m_pointerCache;
        }

        void SetViewPort(uint64_t vp, uint32_t vpIndex) {
            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                m_pointerCache.ViewPort = vp;
                m_pointerCache.ViewPortIndex = vpIndex;
            }

        }

        void SetPlayerCameraManager(uint64_t pcm) {
            uintptr_t oldPcm = 0;
            {
                std::lock_guard<std::mutex> lk(m_pointerMtx);
                oldPcm = m_pointerCache.PlayerCameraManager;
                m_pointerCache.PlayerCameraManager = pcm;
            }
#ifdef DEBUG_CONSOLE
            CamBindDiag::OnPcmStore(oldPcm, pcm);
#endif
        }

        void UpdateCamera(const Vector3& loc, const FRotator& rot, float fov, float aspect, uint64_t uworld = 0) {
            SPOOF_FUNC;
            std::lock_guard<std::mutex> lk(m_cameraMtx);
            m_camera.location = loc;
            m_camera.rotation = rot;
            m_camera.fov = fov;
            m_camera.aspectRatio = aspect;
            m_camera.valid = true;
            m_camera.lastUpdateMs = GetNowMs();
            m_camera.uworld = uworld;
        }

        CachedCameraData GetCameraSnapshot() {
            SPOOF_FUNC;
            std::lock_guard<std::mutex> lk(m_cameraMtx);
            return m_camera;
        }

        void ForceReset() {
            SPOOF_FUNC;
            InvalidateRaidState();
            InterlockedExchange(&m_raidState, (LONG)RaidState::WaitingForWorld);
        }

    };

    inline CState* Entity = nullptr;

}
