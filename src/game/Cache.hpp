#pragma once
#include <dwmapi.h>
#include <Windows.h>
#include <algorithm>
#include <winternl.h>
#include <map>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include <iostream>
#include <vector>
#include <string>
#include <cmath>
#include <shared_mutex>
#include "sdk/SDK.hpp"
#include "draws/Textures.h"
#include "settings/Types.h"
#include "settings/Settings.hpp"
#include "WorldCache.hpp"
#include "dgb/Spoof.h"


namespace SpatialLimits {
    constexpr float  CACHE_RADIUS    = 25000.0f;
    constexpr float  CACHE_RADIUS_SQ = CACHE_RADIUS * CACHE_RADIUS;
    constexpr float  EVICT_RADIUS    = 30000.0f;
    constexpr float  EVICT_RADIUS_SQ = EVICT_RADIUS * EVICT_RADIUS;
    constexpr int    ACTOR_BATCH     = 512;
    constexpr int    MAX_ACTORS_PER_LEVEL = 20000;
    constexpr int    MAX_COMP_STACK  = 128;
    constexpr int    MAX_CHILDREN    = 128;
    constexpr int    MAX_LEVELS      = 1024;
    constexpr size_t MAX_AGGGEOM_CNT = 256;
    constexpr DWORD  SLEEP_NORMAL_MS = 3000;
    constexpr DWORD  SLEEP_IDLE_MS   = 500;
}

inline void ClearAllGeometryCache() {
    SPOOF_FUNC;
    RayVis::Clear();
}
