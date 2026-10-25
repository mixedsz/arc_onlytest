#pragma once

#include <windows.h>

#include <cstdint>
#include <shared_mutex>
#include <unordered_set>
#include <vector>

// Item actors currently held by a live player.
//
// Threat classification is name-based and cannot tell a grenade in someone's
// hand from the same grenade sitting in a dead player's backpack: both are a
// BP_ItemActor_SmokeGrenade_C standing in the level, and the world scan sees
// them identically. SlowCache already resolves the held item for every pawn in
// the live-player snapshot, so this republishes those pointers for the world
// scan to check against. Corpses never enter that snapshot - they are pawns
// with no valid PlayerState - which is what makes their inventory drop out.
namespace HeldItems {

    inline std::shared_mutex g_mtx;
    inline std::unordered_set<uint64_t> g_held;
    inline uint64_t g_lastPublishMs = 0;

    // A publish older than this is treated as no information at all.
    inline constexpr uint64_t kFreshWindowMs = 5000;

    // Timestamps are taken here rather than passed in. The world scan captures
    // its "now" once and then walks up to 20000 actors, so by the time it asks
    // about a threat its clock can trail this one by seconds - and since the
    // publisher runs every 250ms, that made the freshness subtraction underflow
    // and report a perfectly current registry as stale.
    inline void Publish(const std::vector<uint64_t>& actors)
    {
        std::unique_lock<std::shared_mutex> lk(g_mtx);
        g_held.clear();
        g_held.insert(actors.begin(), actors.end());
        g_lastPublishMs = GetTickCount64();
    }

    inline void Clear()
    {
        std::unique_lock<std::shared_mutex> lk(g_mtx);
        g_held.clear();
        g_lastPublishMs = 0;
    }

    // Callers must fail open on false. If the slow cache has not reported
    // recently the registry knows nothing, and "nothing is held" would hide
    // every live grenade in the raid - the opposite of what this is for.
    inline bool Fresh()
    {
        std::shared_lock<std::shared_mutex> lk(g_mtx);
        if (g_lastPublishMs == 0)
            return false;
        const uint64_t now = GetTickCount64();
        return now >= g_lastPublishMs && (now - g_lastPublishMs) <= kFreshWindowMs;
    }

    inline bool IsHeld(uint64_t actor)
    {
        if (!actor)
            return false;
        std::shared_lock<std::shared_mutex> lk(g_mtx);
        return g_held.find(actor) != g_held.end();
    }

}
