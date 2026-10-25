#pragma once
#include "config/build_config.hpp"
#include <Windows.h>
#include <string>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <shared_mutex>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdio>
#include <cctype>
#include <iostream>
#include "sdk/SDK.hpp"
#include "settings/Types.h"

struct SkeletonWorldBones {
    Vector3 neck, chest, pelvis;
    Vector3 l_shoulder, l_elbow, l_hand;
    Vector3 r_shoulder, r_elbow, r_hand;
    Vector3 l_thigh, l_knee, l_foot;
    Vector3 r_thigh, r_knee, r_foot;
};

struct SkeletonScreenBones {
    ImVec2 neck, chest, pelvis;
    ImVec2 l_shoulder, l_elbow, l_hand;
    ImVec2 r_shoulder, r_elbow, r_hand;
    ImVec2 l_thigh, l_knee, l_foot;
    ImVec2 r_thigh, r_knee, r_foot;
};

struct PlayerDynamicData {
    uint64_t  entity;
    uint64_t  playerState;
    uintptr_t skeletalMesh;
    uintptr_t rootComponent;
    ImVec2    head2D;
    ImVec2    foot2D;
    float     height;
    float     width;
    bool      isVisible;
    Vector3   worldLoc;
    Vector3   velocity;
    float     horizSpeed = 0.f;
    bool      isDBNO;
    // Whether isDBNO reflects a read that actually landed and passed validation. Without
    // this, "not downed" and "no idea" are the same value, and the death rule in
    // SlowCache.hpp treats the second as the first - which is how knocked players were
    // being declared dead.
    bool      isDBNOKnown = false;
    bool      isBot;
    bool      stale = false;

    std::string name;
    std::wstring squadname;
    uint8_t    platform = 0;
    bool       is_team;
    int        kills;
    int        level;

    double currentArmor = 0.0;
    double maxArmor = 0.0;
    double currentHealth = 0.0;
    double maxHealth = 0.0;

    bool isZooming = false;
    bool isSprinting = false;
    bool isDodgeRolling = false;
    bool isInJump = false;
    bool isReloading = false;
    bool isFiring = false;
    uint8_t characterStance = 0;

    bool npc = false;
    bool drone = false;
    bool destroyed = false;
    bool hasBones = false;
    bool loot = false;
    bool container = false;
    bool salvage = false;
    bool grenade = false;
    bool mine = false;
    bool trap = false;
    // Bit position in whichever of the three threat masks matches the flag
    // above. Resolved once during caching so the draw path does not re-parse
    // the blueprint name every frame.
    int threatBit = -1;
    bool extraction = false;
    bool raidercache = false;
    bool arcprobe = false;
    bool corpse = false;
    bool searched = false;

    std::string arcname;
    std::string arcDisplayName;
    std::string lootcat;
    std::string extractioncat;
    std::string ContainerName;
    std::string LootName;
    uint8_t lootCatId = 0xFF;
    uint8_t extractionState = 0;
    std::string heldItem;
    std::string weaponLevel;
    int itemvalue = -1;
    int lootamount = 0;
    Vector3 npcLoc;
    int64_t arcid = -1;
    uint64_t lastUpdateMs = 0;

    std::unordered_map<Bones, Vector3> cachedBones;
    std::unordered_map<int, Vector3> weakspots;
    float yaw;

    SkeletonWorldBones skeletonBones;
    bool bones_valid = false;
    bool fullSkeleton = false; // true when limb bones are drawable
    uint64_t fullSkeletonMs = 0; // last time a coherent full skeleton was sampled
    bool bonesPredicted = false; // true when fullSkeleton is being held from a stale (last-known-good) sample
    uint64_t headRootMs = 0; // last time cachedBones[Head]/[Root] were actually refreshed (real bone or worldLoc-approx) - Head/Root are never erased, so this is the only staleness signal for them
    Vector3 previous_head_pos;
};

enum class WorldItemType {
    None,
    Extraction,
    SalvageContainer,
    SocketContainer,
    Pickup,
    RaiderCache,
    Grenade,
    Mine,
    Trap,
    Arc,
    Corpse,
    Probe,
    LivePioneer
};

struct WorldCacheItem {
    uint64_t      actor;
    Vector3       worldLoc;
    WorldItemType type;
    std::string   displayName;
    std::string   actorName;
    int64_t       arcId;
    bool          searched;

    std::string   lootName;
    int           lootAmount = 0;
    uint8_t       lootCatId = 0xFF;
    int           itemValue = -1;
    double        lootWeight = 0.0;

    std::string   extractionCat;
    uint8_t       extractionCatId = 8;
    // Bit position within the mask for this item's threat category; -1 when
    // the item is not a grenade, mine or trap.
    int           threatBit = -1;
    uint64_t      lastSeenMs = 0;
    bool          stale = false;
};

struct WorldDrawInfo {
    uint64_t      actor;
    Vector3       worldLoc;
    WorldItemType type;
    std::string   displayName;
    std::string   actorName;
    int64_t       arcId;
    bool          searched;
    float         distM;

    std::string   lootName;
    int           lootAmount = 0;
    uint8_t       lootCatId = 0xFF;
    int           itemValue = -1;
    double        lootWeight = 0.0;
    std::string   extractionCat;
    uint8_t       extractionCatId = 8;
    int           threatBit = -1;
    double        currentHealth = 0.0;
    double        maxHealth = 0.0;
    bool          stale = false;
};

struct CachedCameraData {
    Vector3 location;
    FRotator rotation;
    float fov = 90.0f;
    float aspectRatio = 1.0f;
    bool valid = false;
    uint64_t lastUpdateMs = 0;
    uint64_t uworld = 0;
};

struct PointerCache {
    uint64_t ViewPort = 0;
    uint32_t ViewPortIndex = 0;
    uint64_t UWorld = 0;
    uint64_t GameInstance = 0;
    uint64_t LocalPlayer = 0;
    uint64_t PlayerController = 0;
    uint64_t LocalPawn = 0;
    uint64_t PlayerCameraManager = 0;
    uintptr_t PlayerArray = 0;
    int32_t PlayerArraySize = 0;
    Vector3 RelativeLocation;
    bool valid = false;
    uint64_t lastUpdateMs = 0;
};

struct SlowPlayerData {
    uint64_t lastUpdateMs = 0;
    bool isZooming = false;
    bool isSprinting = false;
    bool isDodgeRolling = false;
    bool isInJump = false;
    bool isReloading = false;
    bool isFiring = false;
    uint8_t characterStance = 0;
    double currentHealth = 0.0;
    double maxHealth = 0.0;
    double currentArmor = 0.0;
    double maxArmor = 0.0;
    bool isDbno = false;
    bool isDbnoKnown = false;
    std::string heldItem;
    std::string weaponLevel;
    bool stale = false;
};

struct SlowWorldData {
    bool destroyed = false;
    bool searched = false;
    uint8_t extractionState = 0;
    double currentHealth = 0.0;
    double maxHealth = 0.0;
    bool stale = false;
};

struct FindObjectResult {
    uint64_t ptr = 0;
    uint32_t index = 0;
};

inline bool is_valid_bone_position(const Vector3& pos) {
    if (pos.x == 0.0 && pos.y == 0.0 && pos.z == 0.0)
        return false;
    if (std::isnan(pos.x) || std::isnan(pos.y) || std::isnan(pos.z))
        return false;
    if (std::isinf(pos.x) || std::isinf(pos.y) || std::isinf(pos.z))
        return false;
    return pos.x > -10000000.0 && pos.x < 10000000.0 &&
        pos.y > -10000000.0 && pos.y < 10000000.0 &&
        pos.z > -10000000.0 && pos.z < 10000000.0;
}

inline uint8_t GetExtractionTypeId(const std::string& name)
{
    std::string lower = name;
    for (char& c : lower) c = (char)tolower((unsigned char)c);

    if (lower.find("elevator") != std::string::npos) return 0;
    if (lower.find("fan") != std::string::npos) return 1;
    if (lower.find("gaspipe") != std::string::npos || lower.find("gas_pipe") != std::string::npos ||
        lower.find("gas pipe") != std::string::npos) return 2;
    if (lower.find("hatch") != std::string::npos) return 3;
    if (lower.find("major") != std::string::npos) return 4;
    if (lower.find("metro") != std::string::npos) return 5;
    if (lower.find("minor") != std::string::npos) return 6;
    if (lower.find("base") != std::string::npos) return 7;
    return 8;
}

inline std::string NormalizeExtractionTag(const std::string& raw)
{
    if (raw.empty())
        return raw;
    std::string s = raw;
    static const std::string prefix = std::string(X("DialogueBucketParameter.POI.Extraction."));
    if (s.rfind(prefix, 0) == 0) {
        s.erase(0, prefix.size());
    }
    static const std::string sp0 = std::string(X("ExtractionType_"));
    static const std::string sp1 = std::string(X("Extraction_"));
    static const std::string sp2 = std::string(X("ExtractionPoint_"));
    static const std::string sp3 = std::string(X("ExtractionLocation_"));
    static const std::string sp4 = std::string(X("ExtractionLocationType_"));
    static const std::string sp5 = std::string(X("ExtractionDataAsset_"));
    const std::string* prefixes[] = { &sp0, &sp1, &sp2, &sp3, &sp4, &sp5 };
    for (const std::string* p : prefixes) {
        if (s.size() > p->size() && s.compare(0, p->size(), *p) == 0) {
            s = s.substr(p->size());
            break;
        }
    }
    static const std::string sSuffix = std::string(X("_C"));
    if (s.size() > 2 && s.compare(s.size() - 2, 2, sSuffix) == 0)
        s.resize(s.size() - 2);
    for (char& c : s) {
        if (c == '_' || c == '.')
            c = ' ';
    }
    return s;
}

inline std::string WeaponQualityToLevel(int quality)
{
    static const std::string q0 = std::string(X("I"));
    static const std::string q1 = std::string(X("II"));
    static const std::string q2 = std::string(X("III"));
    static const std::string q3 = std::string(X("IV"));
    switch (quality) {
    case 0: return q0;
    case 1: return q1;
    case 2: return q2;
    case 3: return q3;
    default: return "";
    }
}

inline uint64_t GetNowMs() {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();
}
// The GObjects index EmbarkCommonGameViewportClient lands at drifts as the
// game's object table grows across patches/sessions - there is no fixed band
// to scan. We scan the full live range every time, but remember the last
// resolved index across calls/threads so repeat lookups (refresh + boot) hit
// in O(1) instead of re-walking the whole table.
inline std::atomic<uint32_t> g_CachedViewportIndex{ 0 };
inline std::atomic<bool> g_HasCachedViewportIndex{ false };

// One-shot diagnostic: EmbarkCommonGameViewportClient can no longer be found
// in GObjects (renamed by a game update). Dumps every class name containing
// "viewport"/"gameviewport" so the new name can be identified from the log.
inline std::atomic<bool> g_ViewportDiagnosticDone{ false };

// One-shot diagnostic: ViewportScan returning 0 matches could mean the class
// was renamed to something without "viewport" in it, OR that class-name
// resolution itself is broken (e.g. tightened pointer validation swallowing
// otherwise-valid reads). Dumps the first 20 valid UObjects' raw class name
// data so we can tell which case we're in before chasing the wrong fix.
inline std::atomic<bool> g_ObjectNameDumpDone{ false };

inline void DumpFirstNObjectNames(uint32_t numElements, uint64_t chunksPtr) {
    if (g_ObjectNameDumpDone.load(std::memory_order_relaxed))
        return;

    DBG_LOG("[DumpNames] starting raw class name dump\n");
    uint32_t dumped = 0;

    for (uint32_t i = 0; i < numElements && dumped < 20; ++i) {
        uint64_t obj = GetObjectFromGObjects(ctx->image_base, chunksPtr, static_cast<int32_t>(i));
        if (!obj || !is_valid_pointer_optimized(obj)) continue;

        uint64_t classPtr = NameResolver::ClassPrivate(ctx->image_base, obj);
        std::string className = NameResolver::GetClassPrivateNameA(ctx->image_base, obj);

        DBG_LOG("[DumpNames] index=" << i << " obj=0x" << std::hex << obj
            << " class=0x" << classPtr << std::dec << " name=\"" << className << "\"\n");
        ++dumped;
    }

    DBG_LOG("[DumpNames] complete: " << dumped << " objects dumped\n");
    g_ObjectNameDumpDone.store(true, std::memory_order_relaxed);
}

inline bool TryRefreshViewPortOnce(uint64_t& outViewPort, uint32_t& outIndex) {
    auto toLower = [](std::string s) {
        for (auto& c : s) c = (char)tolower((unsigned char)c);
        return s;
        };

    uint32_t numElements = GetGObjectsCount(ctx->image_base);
    uint64_t chunksPtr = GetGObjectsChunksPtr(ctx->image_base);
    if (!chunksPtr || numElements == 0 || numElements > 900000)
        return false;

    const auto& vp = Offsets::Get().UViewPort;

    auto tryIndex = [&](uint32_t i) -> bool {
        uint64_t obj = GetObjectFromGObjects(ctx->image_base, chunksPtr, static_cast<int32_t>(i));
        if (!obj || !is_valid_pointer_optimized(obj)) return false;

        std::wstring nameW = NameResolver::GetObjectName(ctx->image_base, obj);
        if (nameW.empty()) return false;

        std::string name;
        name.reserve(nameW.length());
        for (wchar_t wc : nameW) {
            if (wc > 0 && wc < 128)
                name.push_back(static_cast<char>(wc));
        }
        if (name.empty()) return false;
        if (toLower(name) != X("embarkcommongameviewportclient")) return false;

        uint64_t world = ctx->read<uint64_t>(obj + vp.Uworld);
        if (!world || !is_valid_pointer_optimized(world) || (world & 0xF) != 0)
            return false;

        outViewPort = obj;
        outIndex = i;
        return true;
        };

    // Fast path: the viewport index is stable for the lifetime of a session
    // once resolved once, so try the cached index before the full scan.
    if (g_HasCachedViewportIndex.load(std::memory_order_relaxed)) {
        uint32_t cached = g_CachedViewportIndex.load(std::memory_order_relaxed);
        if (cached < numElements && tryIndex(cached))
            return true;
    }

    for (uint32_t i = 0; i < numElements; ++i) {
        if (tryIndex(i)) {
            g_CachedViewportIndex.store(i, std::memory_order_relaxed);
            g_HasCachedViewportIndex.store(true, std::memory_order_relaxed);
            return true;
        }
    }
    return false;
}

inline bool InitializeViewPort(uint64_t& outViewPort, uint32_t& outIndex) {
    auto toLower = [](std::string s) {
        for (auto& c : s) c = (char)tolower((unsigned char)c);
        return s;
        };

    for (int attempt = 0; attempt < 30; ++attempt) {
        uint32_t numElements = GetGObjectsCount(ctx->image_base);
        uint64_t chunksPtr = GetGObjectsChunksPtr(ctx->image_base);

        if (!chunksPtr || numElements == 0 || numElements > 900000) {
            // A null chunks pointer is a broken decryption, not a load-sequence race, and
            // no amount of waiting repairs it - the retry budget exists for the case where
            // objects simply have not been allocated yet, which presents as a valid chunks
            // pointer with a small count. Sitting here for 30 x 2s while GWorld has already
            // resolved UWorld just delays startup by a minute for nothing.
            if (!chunksPtr) {
                DBG_LOG("[InitializeViewPort] GObjects decryption is dead (chunks=0x0); "
                    "skipping viewport scan - UWorld comes from GWorld instead\n");
                return false;
            }
            DBG_LOG("[InitializeViewPort] attempt " << (attempt + 1)
                << ": bad chunks/count (chunks=0x" << std::hex << chunksPtr
                << " count=" << std::dec << numElements << "), sleep 2s\n");
            Sleep(2000);
            continue;
        }

        // No fixed band - EmbarkCommonGameViewportClient's index drifts release
        // to release as GObjects grows, so the whole live table is scanned.
        const uint32_t bandEnd = numElements - 1u;
        DBG_LOG("[InitializeViewPort] attempt " << (attempt + 1)
            << ": GObjects=" << numElements << " scanning indices [0 .. " << bandEnd << "]\n");

        // Diagnostic pass: dump raw class names for the first 20 valid objects
        // before the viewport-specific scan, so we can tell whether name
        // resolution itself works before trusting a 0-match ViewportScan result.
        DumpFirstNObjectNames(numElements, chunksPtr);

        // Diagnostic pass: EmbarkCommonGameViewportClient can't be found anymore,
        // so log every class name that looks viewport-related to identify the
        // renamed class. Runs once per session, ahead of the normal search below.
        if (!g_ViewportDiagnosticDone.load(std::memory_order_relaxed)) {
            DBG_LOG("[ViewportScan] starting diagnostic scan of " << numElements << " objects\n");
            uint32_t viewportMatches = 0;

            for (uint32_t i = 0; i <= bandEnd; ++i) {
                uint64_t diagObj = GetObjectFromGObjects(ctx->image_base, chunksPtr, static_cast<int32_t>(i));
                if (!diagObj || !is_valid_pointer_optimized(diagObj)) continue;

                std::wstring diagNameW = NameResolver::GetObjectName(ctx->image_base, diagObj);
                if (diagNameW.empty()) continue;

                std::string diagName;
                diagName.reserve(diagNameW.length());
                for (wchar_t wc : diagNameW) {
                    if (wc > 0 && wc < 128)
                        diagName.push_back(static_cast<char>(wc));
                }
                if (diagName.empty()) continue;

                std::string diagNameLower = toLower(diagName);
                if (diagNameLower.find("viewport") == std::string::npos &&
                    diagNameLower.find("gameviewport") == std::string::npos)
                    continue;

                DBG_LOG("[ViewportScan] index=" << i << " name=\"" << diagName
                    << "\" ptr=0x" << std::hex << diagObj << std::dec << "\n");
                ++viewportMatches;
            }

            DBG_LOG("[ViewportScan] complete: found " << viewportMatches << " viewport-related classes\n");
            g_ViewportDiagnosticDone.store(true, std::memory_order_relaxed);
        }

        struct Candidate { uint64_t obj; uint32_t index; };
        std::vector<Candidate> candidates;
        candidates.reserve(4);

        // Fast path: try the cached index (resolved by an earlier call in this
        // or another thread) before walking the full table.
        if (g_HasCachedViewportIndex.load(std::memory_order_relaxed)) {
            uint32_t cached = g_CachedViewportIndex.load(std::memory_order_relaxed);
            if (cached <= bandEnd) {
                uint64_t obj = GetObjectFromGObjects(ctx->image_base, chunksPtr, static_cast<int32_t>(cached));
                if (obj && is_valid_pointer_optimized(obj)) {
                    std::wstring nameW = NameResolver::GetObjectName(ctx->image_base, obj);
                    std::string name;
                    name.reserve(nameW.length());
                    for (wchar_t wc : nameW) {
                        if (wc > 0 && wc < 128)
                            name.push_back(static_cast<char>(wc));
                    }
                    if (!name.empty() && toLower(name) == X("embarkcommongameviewportclient")) {
                        DBG_LOG("[InitializeViewPort] cached index " << cached << " still valid, skipping full scan\n");
                        candidates.push_back({ obj, cached });
                    }
                }
            }
        }

        if (candidates.empty()) {
            for (uint32_t i = 0; i <= bandEnd; ++i) {
                uint64_t obj = GetObjectFromGObjects(ctx->image_base, chunksPtr, static_cast<int32_t>(i));
                if (!obj || !is_valid_pointer_optimized(obj)) continue;

                std::wstring nameW = NameResolver::GetObjectName(ctx->image_base, obj);
                if (nameW.empty()) continue;

                std::string name;
                name.reserve(nameW.length());
                for (wchar_t wc : nameW) {
                    if (wc > 0 && wc < 128)
                        name.push_back(static_cast<char>(wc));
                }
                if (name.empty()) continue;

                if (toLower(name) != X("embarkcommongameviewportclient")) continue;

                DBG_LOG("[InitializeViewPort] name hit at index " << i << " obj=0x" << std::hex << obj << std::dec << "\n");
                candidates.push_back({ obj, i });
            }
        }

        const auto& vp = Offsets::Get().UViewPort;
        for (const auto& c : candidates) {
            uint64_t world = ctx->read<uint64_t>(c.obj + vp.Uworld);
            bool w_ok = world && is_valid_pointer_optimized(world) && (world & 0xF) == 0;
            if (!w_ok) {
                DBG_LOG("[InitializeViewPort] candidate idx=" << c.index
                    << " obj=0x" << std::hex << c.obj << " UWorld invalid (0x" << world << std::dec << ")\n");
                continue;
            }
            outViewPort = c.obj;
            outIndex = c.index;
            g_CachedViewportIndex.store(c.index, std::memory_order_relaxed);
            g_HasCachedViewportIndex.store(true, std::memory_order_relaxed);
            DBG_LOG("[InitializeViewPort] OK idx=" << outIndex << " ViewPort=0x" << std::hex << outViewPort
                << " UWorld=0x" << world << std::dec << "\n");
            return true;
        }

        if (!candidates.empty())
            DBG_LOG("[InitializeViewPort] " << candidates.size()
                << " name match(es) in table but none had valid UWorld; sleep 3s\n");
        else
            DBG_LOG("[InitializeViewPort] no EmbarkCommonGameViewportClient found; sleep 3s\n");

        Sleep(3000);
    }

    DBG_LOG("[InitializeViewPort] failed after 30 attempts\n");
    return false;
}
