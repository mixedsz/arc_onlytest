#pragma once
#include <vector>
#include <string>
#include <mutex>
#include <cstddef>
#include "Types.h"
#include <fstream>
#include <filesystem>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <shlobj.h>
#include "imgui.h"
#include "dgb/lazy.hpp"
#include "config/build_config.hpp"


extern int Width1;
extern int Height1;
extern int ScreenCenterX;
extern int ScreenCenterY;
inline bool Random = false;
inline int fontIndex = 0;


inline bool combatModeActive = false;
inline int combatModeKey = 0;
inline int combatModeKeyMode = 2; // toggle by default - matches the original hardcoded behaviour

// Controller. Gamepad controls live in the same keybind table as the keyboard
// (see gamepad.hpp), so these only govern polling, labelling, and the virtual
// cursor - not which features a pad can drive.
inline bool  padEnabled          = true;
inline int   padLayout           = 0;      // 0 auto-detect, 1 Xbox, 2 PlayStation
inline bool  padCursorEnabled    = true;
inline float padCursorSpeed      = 1400.0f; // px/sec at full stick deflection
inline float padScrollSpeed      = 12.0f;   // wheel notches/sec at full right stick
inline float padDeadzone         = 0.20f;
inline float padTriggerThreshold = 0.30f;   // analog trigger -> digital press
inline int   padMenuCombo        = 0;      // 0 L3+R3, 1 Back+Start, 2 none
inline bool  padTabCycle         = true;   // bumpers cycle menu tabs

inline bool debugOverlayEnabled = false;
// stdout skeleton/ESP diagnostics. Off by default outside a debug build: release
// compiles DBG_LOG down to nothing, so leaving this on there costs the snprintf and
// string work at every call site - much of it inside the per-entity bone loop - to
// produce text that is then discarded. The menu checkbox still turns it on.
#ifdef _DEBUG
inline bool debugConsoleEnabled = true;
// Diagnostic W2S FOV source. 0 Current, 1 POV, 2 Locked Read-Back.
// Not saved to config; resets to Current every launch. Does not write game memory.
inline int camProjectionTest = 0;
#else
inline bool debugConsoleEnabled = false;
#endif
inline bool benchmarkEnabled = false;
inline bool debugDrawGeometry = false;
inline bool AutoSaveLogs = true;
inline bool LogErrorsOnly = false;
inline bool LogPlayerActions = false;
inline bool LogTransportErrors = false;

inline volatile LONG localPlayerScoped = 0;

namespace DisableWhenScoped {
    inline bool username = false;
    inline bool distance = false;
    inline bool heldItem = false;
    inline bool playerFlags = false;
    inline bool worldESP = false;
    inline bool groundItems = false;
    inline bool arcs = false;
    inline bool corpses = false;
    inline bool raiderCache = false;
    inline bool arcProbes = false;
    inline bool grenades = false;
    inline bool mines = false;
    inline bool traps = false;
}

namespace DebugSystem {
    inline std::vector<std::string> messages;
    inline std::vector<std::string> allMessages;
    inline std::mutex debugMutex;
    inline int maxMessages = 15;
    inline constexpr size_t kMaxAllMessages = 4000;
    inline constexpr size_t kAllMessagesKeep = 2000;

    inline void TrimAllMessagesLocked() {
        if (allMessages.size() > kMaxAllMessages)
            allMessages.erase(allMessages.begin(),
                allMessages.begin() + (ptrdiff_t)(allMessages.size() - kAllMessagesKeep));
    }

    inline void Clear() {
        std::lock_guard<std::mutex> lock(debugMutex);
        messages.clear();
    }

    inline void ClearAll() {
        std::lock_guard<std::mutex> lock(debugMutex);
        messages.clear();
        allMessages.clear();
    }

    inline void AddMessage(const std::string& msg) {
        if (!debugOverlayEnabled) return;
        std::lock_guard<std::mutex> lock(debugMutex);
        if (messages.size() >= maxMessages) {
            messages.erase(messages.begin());
        }
        messages.push_back(msg);
        allMessages.push_back(msg);
        TrimAllMessagesLocked();
    }

    inline std::vector<std::string> GetMessages() {
        std::lock_guard<std::mutex> lock(debugMutex);
        return messages;
    }

    inline std::string GetAllMessagesAsString() {
        std::lock_guard<std::mutex> lock(debugMutex);
        std::string result;
        for (const auto& msg : allMessages) {
            result += msg + "\n";
        }
        return result;
    }

    inline void LogConsole(const std::string& msg) {
        if (!debugConsoleEnabled)
            return;
        DBG_LOG(msg);
        // Also keep a copy for overlay/clipboard even if overlay draw is off.
        std::lock_guard<std::mutex> lock(debugMutex);
        allMessages.push_back(msg);
        TrimAllMessagesLocked();
        if (messages.size() >= maxMessages)
            messages.erase(messages.begin());
        messages.push_back(msg);
    }

    inline void CopyToClipboard() {
        std::string allLogs = GetAllMessagesAsString();
        if (allLogs.empty()) return;

        if (OpenClipboard(nullptr)) {
            EmptyClipboard();
            HGLOBAL hGlob = GlobalAlloc(GMEM_MOVEABLE, allLogs.size() + 1);
            if (hGlob) {
                char* pGlob = (char*)GlobalLock(hGlob);
                if (pGlob) {
                    memcpy(pGlob, allLogs.c_str(), allLogs.size() + 1);
                    GlobalUnlock(hGlob);
                    SetClipboardData(CF_TEXT, hGlob);
                }
            }
            CloseClipboard();
        }
    }
}
static const char* HitBoxes[] =
{
    "Head",
    "Neck",
    "Chest",
    "Pelvis",
};

// Legacy single-bone index (kept for old configs). Prefer HitBoxMask.
inline int HitBox = 0;
// Bitmask for multi-bone aim: bit0=Head, bit1=Neck, bit2=Chest, bit3=Pelvis.
inline int HitBoxMask = 1;

inline ImVec4 g_Accent = ImVec4(207 / 255.f, 21 / 255.f, 21 / 255.f, 1.0f);
inline ImVec4 g_AccentAct = ImVec4(187 / 255.f, 11 / 255.f, 11 / 255.f, 1.0f);

namespace Cfg::Scene
{
    inline bool WorldESP = true;

    inline bool SalvageESP = true;
    inline float SalvageColor[4] = { 0.60f, 0.75f, 0.90f, 1.0f };
    inline float MaxDistanceSalvage = 200.f;
    inline bool showSalvageIcon = true;
    inline bool SalvageTypeFilterMulti[SceneTypes::Salvage::COUNT] = { true, true, true, true, true, true, true, true, true };

    inline bool ContainersESP = true;
    inline float ContainersColor[4] = { 0.85f, 0.70f, 0.55f, 1.0f };
    inline float MaxDistanceContainers = 300.f;
    inline bool showContainerIcon = true;
    inline bool ContainersShowOpened = false;
    inline bool ContainerTypeFilterMulti[SceneTypes::Container::COUNT] = {
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true, true, true,
        true, true
    };

    inline bool ItemsESP = true;
    inline float ItemsColor[4] = { 0.85f, 0.80f, 0.95f, 1.0f };
    inline float MaxDistanceItems = 150.f;
    inline bool FovStacking = false;
    inline float FovStackAngle = 5.0f;
    inline float FovStackMinDistance = 50.0f;
    inline bool WorldShowCircle = true;
    inline bool WorldNameWithDistance = false;
    inline bool ItemsCategoryFilterMulti[22] = {
        true, true, true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true, true, true,
        true, true
    };

    inline bool CorpsesESP = true;
    inline float CorpsesColor[4] = { 0.75f, 0.55f, 0.55f, 0.90f };
    inline float MaxDistanceCorpses = 300.f;
    inline bool showCorpseIcon = true;

    inline bool RaiderCacheESP = true;
    inline float RaiderCacheColor[4] = { 0.80f, 0.65f, 0.50f, 1.0f };
    inline float MaxDistanceRaiderCache = 300.f;

    inline bool ArcProbesESP = true;
    inline float ArcProbesColor[4] = { 0.65f, 0.50f, 0.85f, 0.90f };
    inline float MaxDistanceArcProbes = 300.f;

    // Per-subtype threat filters. One bit per entry, ordered to match
    // ThreatFilterNames() in Types.h. -1 sets every bit, so a fresh install and
    // any config written before these existed both behave exactly as before:
    // everything shown. The top bit of each is the Other/Unknown catch-all, so
    // a threat the name chain does not recognise stays visible.
    inline int GrenadeTypeMask = -1;
    inline int MineTypeMask = -1;
    inline int TrapTypeMask = -1;

    inline bool GrenadeESP = true;
    inline float GrenadeColor[4] = { 0.90f, 0.45f, 0.45f, 1.0f };

    inline bool MinesESP = true;
    inline float MinesColor[4] = { 0.90f, 0.50f, 0.40f, 1.0f };

    inline bool TrapsESP = true;
    inline float TrapsColor[4] = { 0.85f, 0.55f, 0.40f, 1.0f };

    inline bool ExtractionsESP = true;
    inline float ExtractionsColor[4] = { 0.45f, 0.80f, 0.70f, 1.0f };
    inline bool ExtractionTimeLeft = true;
    inline bool showExtractionIcon = true;
    inline bool ExtractionTypeFilterMulti[9] = { true, true, true, true, true, true, true, true, true };

    inline bool ARCsESP = true;
    inline float ARCsColor[4] = { 0.85f, 0.45f, 0.50f, 1.0f };
    inline float MaxDistanceArcs = 300.f;
    inline bool ArcTypeFilterMulti[21] = {
        true, true, true, true, true, true, true, true,
        true, true, true, true, true, true, true, true,
        true, true, true, true, true
    };
    inline bool ARCsShowDestroyed = false;
    inline bool ARCsShowHealthBar = true;

    inline float MaxDistanceGrenade = 100.f;
    inline float MaxDistanceTraps= 100.f;
    inline float MaxDistanceMines = 100.f;

    inline bool ShowName = true;
    inline bool Icon = false;
    inline bool BoundsESP = false;
    inline float BoundsColor[4] = { 0.55f, 0.40f, 0.80f, 0.45f };
    inline bool Amount = true;
    inline bool Price = false;
    inline bool Weight = false;
    inline bool Filterbyprice = false;
    inline int PricePolicy = 0;
    inline int MinPrice = 0;

    inline bool flashlightColorChangerEnabled = false;
    inline float flashlightColor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

    inline bool showPlayerFlags = false;
    inline float playerFlagsColor[4] = { 0.65f, 0.55f, 0.90f, 0.85f };
    inline bool playerFlagsFilterMulti[8] = { true, true, true, true, true, true, true, true };

    inline void InitFilters()
    {
        std::fill_n(SalvageTypeFilterMulti, SceneTypes::Salvage::COUNT, true);
        std::fill_n(ContainerTypeFilterMulti, SceneTypes::Container::COUNT, true);
        std::fill_n(ItemsCategoryFilterMulti, 22, true);
        std::fill_n(ArcTypeFilterMulti, 21, true);
        std::fill_n(playerFlagsFilterMulti, 8, true);
    }
}

inline bool visualsb = true;
inline bool visuals1 = true;
inline bool helditem = true;
inline bool weaponlevel = true;
inline bool visuals2 = true;
inline bool visuals3 = false;
inline bool visuals4 = false;
inline bool visuals5 = false;
inline bool headdot = false;
inline bool squadname = true;
inline bool showPlatform = true;
inline float platformIconScale = 1.f;
inline bool streamproof = true;

inline bool showFilledBox = false;
inline bool showOutlineBox = false;

// Top of the Max Distance slider means "no range cull at all" rather than a very
// large number, so the render loop can skip the comparison outright. Enemies past
// bone-resolution range still get a box from the position-only path, just no pose.
inline constexpr float kEspDistanceUnlimited = 1500.f;
inline float distance_vis = kEspDistanceUnlimited;

inline std::vector<std::string> snaplineType = { X("CENTER"), X("TOP"), X("BOTTOM") };
inline int indexSnapline = 0;

inline std::vector<std::string> boxType = { X("FULL"), X("CORNERS"), X("DASHED"), X("ROUNDED") };
inline int indexBox = 0;

inline std::vector<std::string> namePosType = { X("ABOVE"), X("BELOW") };
inline std::vector<std::string> distPosType = { X("ABOVE"), X("BELOW") };
inline std::vector<std::string> squadPosType = { X("ABOVE"), X("BELOW") };
inline std::vector<std::string> heldPosType = { X("ABOVE"), X("BELOW") };
inline int indexNamePos = 0;
inline int indexDistPos = 1;
inline int indexSquadPos = 1;
inline int indexHeldPos = 1;
inline bool distanceWithName = false;

inline std::vector<std::string> barPosType = { X("LEFT"), X("RIGHT"), X("TOP"), X("BOTTOM") };
inline int healthBarPos = 1;
inline int shieldBarPos = 0;
inline bool showHealthBar = true;
inline bool showShieldBar = true;
inline float healthBarWidth = 4.0f;
inline float shieldBarWidth = 3.0f;
inline float healthBarOffset = 2.0f;
inline float shieldBarOffset = 2.0f;
inline bool healthBarBackground = true;
inline bool shieldBarBackground = true;
inline bool healthColorByValue = true;
inline float healthHighColor[4] = { 0.40f, 0.85f, 0.50f, 1.0f };
inline float healthMediumColor[4] = { 0.90f, 0.80f, 0.35f, 1.0f };
inline float healthLowColor[4] = { 0.90f, 0.35f, 0.35f, 1.0f };
inline float healthBackColor[4] = { 40.0f / 255.0f, 40.0f / 255.0f, 40.0f / 255.0f, 200.0f / 255.0f };
inline float shieldColor[4] = { 0.45f, 0.40f, 0.90f, 1.0f };
inline float shieldBackColor[4] = { 20.0f / 255.0f, 20.0f / 255.0f, 60.0f / 255.0f, 200.0f / 255.0f };

inline float box_rounding = 6.0f;
inline float box_dash_length = 8.0f;
inline float box_dash_gap = 4.0f;

inline bool showRadar = false;
inline bool radarShowName = true;
inline bool radarShowDistance = true;
inline float radarSize = 100.f;
inline float radarPosX = 120.f;
inline float radarPosY = 300.f;

inline bool offarrows = false;
inline bool drawfov = false;
inline float col_fov[4] = { 0.55f, 0.40f, 0.90f, 0.40f };

// Overlay HUD. None of this touches game memory - it is drawn from the render loop in
// main.cpp, above ESP and below the menu. Every numeric here is clamped on config load
// and again at draw time; see HudRender.cpp.
inline int   crosshairStyle = 0;      // 0 Disabled .. 32 Minimal Center Plus. 0-4 are Batch 1.
inline float crosshairSize = 10.0f;
inline float crosshairGap = 4.0f;
inline float crosshairThickness = 1.5f;
inline bool  crosshairOutline = true;
inline float col_crosshair[4] = { 1.0f, 1.0f, 1.0f, 0.90f };

inline float crosshairRotation = 0.0f;
inline float crosshairHorizontalScale = 1.0f;
inline float crosshairVerticalScale = 1.0f;
inline bool  crosshairCenterDot = false;
inline float crosshairDotSize = 2.0f;
inline float crosshairOutlineThickness = 1.0f;
inline float col_crosshair_outline[4] = { 0.0f, 0.0f, 0.0f, 200.0f / 255.0f };
inline bool  crosshairRounded = false;
inline float crosshairRingScale = 1.0f;
inline float crosshairBracketLength = 8.0f;
inline float crosshairChevronAngle = 45.0f;
inline int   crosshairSegmentCount = 4;
inline float crosshairSegmentGap = 12.0f;
inline bool  crosshairPulse = false;
inline float crosshairPulseSpeed = 1.5f;
inline float crosshairPulseAmount = 2.0f;
inline bool  crosshairUseAccent = false;
inline float col_crosshair_accent[4] = { 0.35f, 0.85f, 1.0f, 0.90f };
// Preview-only center mark. Not saved; never drawn on the live HUD.
inline bool  crosshairPreviewCenterMark = false;

inline void ResetCrosshairDefaults()
{
    crosshairStyle = 0;
    crosshairSize = 10.0f;
    crosshairGap = 4.0f;
    crosshairThickness = 1.5f;
    crosshairOutline = true;
    col_crosshair[0] = 1.0f; col_crosshair[1] = 1.0f; col_crosshair[2] = 1.0f; col_crosshair[3] = 0.90f;
    crosshairRotation = 0.0f;
    crosshairHorizontalScale = 1.0f;
    crosshairVerticalScale = 1.0f;
    crosshairCenterDot = false;
    crosshairDotSize = 2.0f;
    crosshairOutlineThickness = 1.0f;
    col_crosshair_outline[0] = 0.0f; col_crosshair_outline[1] = 0.0f;
    col_crosshair_outline[2] = 0.0f; col_crosshair_outline[3] = 200.0f / 255.0f;
    crosshairRounded = false;
    crosshairRingScale = 1.0f;
    crosshairBracketLength = 8.0f;
    crosshairChevronAngle = 45.0f;
    crosshairSegmentCount = 4;
    crosshairSegmentGap = 12.0f;
    crosshairPulse = false;
    crosshairPulseSpeed = 1.5f;
    crosshairPulseAmount = 2.0f;
    crosshairUseAccent = false;
    col_crosshair_accent[0] = 0.35f; col_crosshair_accent[1] = 0.85f;
    col_crosshair_accent[2] = 1.0f; col_crosshair_accent[3] = 0.90f;
    crosshairPreviewCenterMark = false;
}

inline bool  hudFpsCounter = true;
inline bool  hudCpuUsage = true;
inline bool  hudGpuUsage = true;
inline bool  hudResolution = false;
inline bool  hudClock = false;
inline bool  hudClock24h = true;
// 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right. Defaults to the right because
// the ESP debug block and the benchmark panel both live in the top-left.
inline int   hudCorner = 1;
inline float hudScale = 1.0f;
inline bool  hudBackground = true;
inline float col_hud[4] = { 0.78f, 0.82f, 0.86f, 1.0f };

// Cached-data warnings. Master default is off so a deploy does not change a live
// raid until the tab is enabled. Per-warning declared defaults match Balanced.
inline bool  warningsEnabled = false;
// Legacy/reserved: still loaded and saved as WarnDur so old configs round-trip.
// Batch 2.1 does not use this as a post-clear linger. Do not expose it in the menu.
inline float warningDisplayDuration = 3.0f;
inline float warningFadeDuration = 0.40f;
inline bool  warningProgressBar = true;
inline float warningInterfaceScale = 1.0f;
inline int   warningMaxVisible = 3;
inline bool  warningDelayJitter = true;
inline float warningDelayMin = 0.25f;
inline float warningDelayMax = 0.60f;
inline bool  nearbyEnemyEnabled = false;
inline float nearbyEnemyDistance = 50.0f;
inline bool  nearbyEnemyShowDistance = true;
inline bool  multipleEnemiesEnabled = true;
inline int   multipleEnemyCount = 2;
inline float multipleEnemyDistance = 80.0f;
inline bool  enemyBehindEnabled = true;
inline float enemyBehindDistance = 80.0f;
inline float enemyBehindAngle = 120.0f;
inline bool  directionIndicatorEnabled = true;
inline float directionIndicatorScale = 1.0f;
inline bool  directionIndicatorPulse = true;
inline float col_warning_text[4] = { 0.93f, 0.94f, 0.96f, 1.0f };
inline float col_warning_bg[4] = { 0.07f, 0.08f, 0.10f, 0.88f };
inline float col_warning_accent[4] = { 0.92f, 0.55f, 0.18f, 1.0f };

// 0 Easy, 1 Advanced
inline int   warningConfigMode = 0;
// 0 Balanced, 1 Close Range, 2 Quiet, 3 Full Awareness, 4 Custom
inline int   warningPreset = 0;
// 0 4-way, 1 8-way
inline int   warningDirectionDetail = 1;
inline bool  warningShowDirectionText = true;
// 1 Nearby, 2 Approaching, 3 ClosingFast, 4 PossibleEngagement
inline int   warningMinConfidence = 2;
inline bool  approachingAlertsEnabled = true;
inline float approachingMinimumSpeed = 1.0f;
inline float approachingFastSpeed = 3.0f;
inline float approachingMinimumDuration = 0.30f;
inline float approachingMaxDistance = 80.0f;
inline float approachingClearSpeed = 0.40f;
inline float approachingClearDebounce = 0.40f;
inline bool  possibleEngagementEnabled = true;
inline bool  possibleEngagementUseZooming = true;
inline bool  possibleEngagementUseFiring = true;
inline int   multipleMinConfidence = 2;

inline void WarningSanitizeApproachSpeeds()
{
    if (!std::isfinite(approachingClearSpeed)) approachingClearSpeed = 0.40f;
    if (!std::isfinite(approachingMinimumSpeed)) approachingMinimumSpeed = 1.0f;
    if (!std::isfinite(approachingFastSpeed)) approachingFastSpeed = 3.0f;
    approachingClearSpeed = std::clamp(approachingClearSpeed, 0.0f, 4.0f);
    approachingMinimumSpeed = std::clamp(approachingMinimumSpeed, 0.2f, 8.0f);
    approachingFastSpeed = std::clamp(approachingFastSpeed, 0.5f, 12.0f);
    if (approachingClearSpeed >= approachingMinimumSpeed)
        approachingClearSpeed = (std::max)(0.0f, approachingMinimumSpeed * 0.4f);
    if (approachingMinimumSpeed >= approachingFastSpeed)
        approachingFastSpeed = approachingMinimumSpeed + 0.5f;
}

inline bool WarningNearEq(float a, float b)
{
    return std::fabs(a - b) <= 0.051f;
}

inline bool WarningPresetMatches(int preset)
{
    if (preset < 0 || preset > 3)
        return false;
    if (!approachingAlertsEnabled || !possibleEngagementEnabled)
        return false;
    if (!possibleEngagementUseZooming || !possibleEngagementUseFiring)
        return false;
    if (!warningShowDirectionText)
        return false;
    if (!warningDelayJitter)
        return false;
    if (!WarningNearEq(warningFadeDuration, 0.40f))
        return false;
    if (!WarningNearEq(approachingClearSpeed, 0.40f) || !WarningNearEq(approachingClearDebounce, 0.40f))
        return false;
    if (!WarningNearEq(approachingMinimumSpeed, 1.0f))
        return false;

    switch (preset)
    {
    case 0:
        return !nearbyEnemyEnabled && warningMinConfidence == 2
            && WarningNearEq(approachingMaxDistance, 80.f)
            && WarningNearEq(approachingMinimumDuration, 0.30f)
            && WarningNearEq(approachingFastSpeed, 3.0f)
            && multipleMinConfidence == 2 && warningDirectionDetail == 1
            && warningMaxVisible == 3
            && WarningNearEq(warningDelayMin, 0.25f) && WarningNearEq(warningDelayMax, 0.60f)
            && WarningNearEq(multipleEnemyDistance, 80.f) && WarningNearEq(enemyBehindDistance, 80.f)
            && WarningNearEq(nearbyEnemyDistance, 50.f);
    case 1:
        return !nearbyEnemyEnabled && warningMinConfidence == 2
            && WarningNearEq(approachingMaxDistance, 40.f)
            && WarningNearEq(approachingMinimumDuration, 0.20f)
            && WarningNearEq(approachingFastSpeed, 2.5f)
            && multipleMinConfidence == 2 && warningDirectionDetail == 1
            && warningMaxVisible == 3
            && WarningNearEq(warningDelayMin, 0.10f) && WarningNearEq(warningDelayMax, 0.30f)
            && WarningNearEq(multipleEnemyDistance, 40.f) && WarningNearEq(enemyBehindDistance, 40.f)
            && WarningNearEq(nearbyEnemyDistance, 40.f);
    case 2:
        return !nearbyEnemyEnabled && warningMinConfidence == 3
            && WarningNearEq(approachingMaxDistance, 60.f)
            && WarningNearEq(approachingMinimumDuration, 0.50f)
            && WarningNearEq(approachingFastSpeed, 3.5f)
            && multipleMinConfidence == 3 && warningDirectionDetail == 0
            && warningMaxVisible == 2
            && WarningNearEq(warningDelayMin, 0.25f) && WarningNearEq(warningDelayMax, 0.60f)
            && WarningNearEq(multipleEnemyDistance, 60.f) && WarningNearEq(enemyBehindDistance, 60.f)
            && WarningNearEq(nearbyEnemyDistance, 50.f);
    case 3:
        return nearbyEnemyEnabled && warningMinConfidence == 1
            && WarningNearEq(approachingMaxDistance, 120.f)
            && WarningNearEq(approachingMinimumDuration, 0.20f)
            && WarningNearEq(approachingFastSpeed, 2.5f)
            && multipleMinConfidence == 1 && warningDirectionDetail == 1
            && warningMaxVisible == 3
            && WarningNearEq(warningDelayMin, 0.25f) && WarningNearEq(warningDelayMax, 0.60f)
            && WarningNearEq(multipleEnemyDistance, 120.f) && WarningNearEq(enemyBehindDistance, 120.f)
            && WarningNearEq(nearbyEnemyDistance, 120.f);
    default:
        return false;
    }
}

inline void ApplyWarningPreset(int preset)
{
    if (preset < 0 || preset > 3)
        return;

    warningShowDirectionText = true;
    approachingAlertsEnabled = true;
    possibleEngagementEnabled = true;
    possibleEngagementUseZooming = true;
    possibleEngagementUseFiring = true;
    approachingClearSpeed = 0.40f;
    approachingClearDebounce = 0.40f;
    approachingMinimumSpeed = 1.0f;
    warningFadeDuration = 0.40f;
    warningDelayJitter = true;

    switch (preset)
    {
    case 0:
        nearbyEnemyEnabled = false;
        warningMinConfidence = 2;
        approachingMaxDistance = 80.f;
        approachingMinimumDuration = 0.30f;
        approachingFastSpeed = 3.0f;
        multipleMinConfidence = 2;
        warningDirectionDetail = 1;
        warningMaxVisible = 3;
        warningDelayMin = 0.25f;
        warningDelayMax = 0.60f;
        nearbyEnemyDistance = 50.f;
        multipleEnemyDistance = 80.f;
        enemyBehindDistance = 80.f;
        break;
    case 1:
        nearbyEnemyEnabled = false;
        warningMinConfidence = 2;
        approachingMaxDistance = 40.f;
        approachingMinimumDuration = 0.20f;
        approachingFastSpeed = 2.5f;
        multipleMinConfidence = 2;
        warningDirectionDetail = 1;
        warningMaxVisible = 3;
        warningDelayMin = 0.10f;
        warningDelayMax = 0.30f;
        nearbyEnemyDistance = 40.f;
        multipleEnemyDistance = 40.f;
        enemyBehindDistance = 40.f;
        break;
    case 2:
        nearbyEnemyEnabled = false;
        warningMinConfidence = 3;
        approachingMaxDistance = 60.f;
        approachingMinimumDuration = 0.50f;
        approachingFastSpeed = 3.5f;
        multipleMinConfidence = 3;
        warningDirectionDetail = 0;
        warningMaxVisible = 2;
        warningDelayMin = 0.25f;
        warningDelayMax = 0.60f;
        nearbyEnemyDistance = 50.f;
        multipleEnemyDistance = 60.f;
        enemyBehindDistance = 60.f;
        break;
    case 3:
        nearbyEnemyEnabled = true;
        warningMinConfidence = 1;
        approachingMaxDistance = 120.f;
        approachingMinimumDuration = 0.20f;
        approachingFastSpeed = 2.5f;
        multipleMinConfidence = 1;
        warningDirectionDetail = 1;
        warningMaxVisible = 3;
        warningDelayMin = 0.25f;
        warningDelayMax = 0.60f;
        nearbyEnemyDistance = 120.f;
        multipleEnemyDistance = 120.f;
        enemyBehindDistance = 120.f;
        break;
    default:
        break;
    }
    WarningSanitizeApproachSpeeds();
}

inline bool zoomChangerEnabled = false;
inline float zoomChangerFOV = 80.0f;

inline bool zoomChangerKeybindEnabled = false;
inline int zoomChangerKey = 0;
inline int zoomChangerKeyMode = 1;
// True optical magnification for the hold-to-zoom key, applied against the
// camera's DefaultFOV rather than a fixed reference so it stays accurate if the
// in-game FOV slider is changed.
inline float zoomChangerMagnification = 4.0f;

inline bool fullBrightEnabled = false;
inline int  viewMode = 0;

inline bool lootThroughWallsEnabled = false;

inline float colpick2_generic[4] = { 0.15f, 1.00f, 0.20f, 0.95f };
inline float col_filledbox[4] = { 0.45f, 0.30f, 0.80f, 0.15f };
inline float col_spanline[4] = { 0.55f, 0.40f, 0.90f, 0.80f };

inline float col_skeleton[4] = { 0.15f, 1.00f, 0.20f, 0.95f };
// Occluded skeleton. Box occluded is col_invisible (ESP → Players → Box Occluded).
inline float col_skeleton_invisible[4] = { 1.0f, 0.15f, 0.15f, 0.90f };
inline float col_headdot[4] = { 0.70f, 0.50f, 1.0f, 0.95f };

inline float col_held[4] = { 0.75f, 0.65f, 0.95f, 1.0f };

inline float col_username[4] = { 0.90f, 0.85f, 1.0f, 1.0f };
inline float col_distance[4] = { 0.70f, 0.65f, 0.85f, 0.90f };
inline float col_squad[4] = { 0.60f, 0.45f, 0.95f, 1.0f };
inline float skeleton_thickness = 1.5f;

inline bool  drawChams         = false;
inline int   chamsMode         = 0;
inline float chamsGlowWidth    = 20.0f;
inline int   chamsGlowLayers   = 5;
inline float chamsFillOpacity  = 0.50f;
inline float col_chams[4]      = { 0.50f, 0.35f, 0.85f, 0.65f };
inline float col_chams2[4]     = { 0.75f, 0.55f, 1.0f, 0.55f };
inline float box_thickness = 1.0f;
inline float box_width_ratio = 0.85f;

inline float col_invisible[4] = { 1.0f, 0.15f, 0.15f, 0.90f };
inline float col_knocked[4] = { 1.0f, 0.78f, 0.15f, 0.95f };
inline float col_line[4] = { 1.f, 1.f, 1.f, 1.f };

inline bool ColorLooksRed(const float c[4])
{
    return c[0] > 0.70f && c[1] < 0.35f && c[2] < 0.35f;
}

inline bool ColorLooksGreen(const float c[4])
{
    return c[1] > 0.70f && c[0] < 0.45f;
}

// CPU ray vischeck (ESP color + aimbot visible-only). Players and ARCs have separate toggles.
inline bool visibilityCheck = true;
inline bool arcVisibilityCheck = true;
inline bool hideInvisibleEsp = false;

inline void SanitizeVischeckColors()
{
    if (ColorLooksRed(col_skeleton) && ColorLooksGreen(col_skeleton_invisible)) {
        float tmp[4];
        for (int i = 0; i < 4; ++i) {
            tmp[i] = col_skeleton[i];
            col_skeleton[i] = col_skeleton_invisible[i];
            col_skeleton_invisible[i] = tmp[i];
        }
    }
    if (ColorLooksRed(colpick2_generic)) {
        colpick2_generic[0] = 0.15f;
        colpick2_generic[1] = 1.00f;
        colpick2_generic[2] = 0.20f;
    }
    // Hide-occluded is not part of vischeck; occluded players stay drawn in Occluded.
    hideInvisibleEsp = false;
}



enum AimbotMode : int {
    AIMBOT_STANDARD = 0,
    AIMBOT_MEMORY = 1,
};

struct AimCfg_t {
    bool        enable = false;
    bool        lines = false;
    bool        targetting = false;
    bool        prediction = true;
    int         aimKey = 0;
    int         aimKeyMode = 1; // 0=disabled, 1=hold, 2=toggle, 3=always
    float       maxHz = 240.f; // aim corrections per second (UI max 360)
    float       fov = 90.f;
    float       baseSmooth = 6.f;
    float       deadzone = 1.f;
    bool        curvature = true;
    float       curveStrength = 0.20f;
    bool        sway = true;
    float       swaySpeed = 0.7f;
    float       swayAmount = 0.25f;
    bool        humanizer = true;
    float       jitterPitch = 0.3f;
    float       jitterYaw = 0.3f;
    bool        sensSafe = true;
    float       maxDegPerUpd = 2.5f;
    bool        randomBone = false;
    bool        visibleonly = false;
    float       maxDistance = 500.f;

    AimbotMode  mode = AIMBOT_STANDARD;
};

inline AimCfg_t AimCfg;

// 0 Sticky, 1 Balanced, 2 Natural, 3 Custom. Named presets write the feel knobs;
// Custom leaves them alone so a saved config is not overwritten.
inline int AimFeelPreset = 1;

inline void ApplyAimFeelPreset(int preset)
{
    switch (preset) {
    case 0: // Sticky — hard lock, almost no humanizer
        AimCfg.baseSmooth = 2.5f;
        AimCfg.maxHz = 240.f;
        AimCfg.deadzone = 0.15f;
        AimCfg.curvature = false;
        AimCfg.curveStrength = 0.f;
        AimCfg.sway = false;
        AimCfg.swaySpeed = 0.7f;
        AimCfg.swayAmount = 0.f;
        AimCfg.humanizer = false;
        AimCfg.jitterPitch = 0.f;
        AimCfg.jitterYaw = 0.f;
        AimCfg.sensSafe = true;
        AimCfg.maxDegPerUpd = 8.f;
        break;
    case 1: // Balanced — factory assist
        AimCfg.baseSmooth = 6.f;
        AimCfg.maxHz = 240.f;
        AimCfg.deadzone = 1.f;
        AimCfg.curvature = true;
        AimCfg.curveStrength = 0.20f;
        AimCfg.sway = true;
        AimCfg.swaySpeed = 0.7f;
        AimCfg.swayAmount = 0.25f;
        AimCfg.humanizer = true;
        AimCfg.jitterPitch = 0.30f;
        AimCfg.jitterYaw = 0.30f;
        AimCfg.sensSafe = true;
        AimCfg.maxDegPerUpd = 2.5f;
        break;
    case 2: // Natural — slower acquire, more float
        AimCfg.baseSmooth = 14.f;
        AimCfg.maxHz = 120.f;
        AimCfg.deadzone = 2.2f;
        AimCfg.curvature = true;
        AimCfg.curveStrength = 0.55f;
        AimCfg.sway = true;
        AimCfg.swaySpeed = 0.9f;
        AimCfg.swayAmount = 0.55f;
        AimCfg.humanizer = true;
        AimCfg.jitterPitch = 0.85f;
        AimCfg.jitterYaw = 0.85f;
        AimCfg.sensSafe = true;
        AimCfg.maxDegPerUpd = 1.0f;
        break;
    default:
        break;
    }
}

struct ArcAimbotConfig {
    bool enable = false;

    int activationKey = 0;
    int activationKeyMode = 1; // 0=disabled, 1=hold, 2=toggle, 3=always
    int weakspotKey = 0;
    int weakspotKeyMode = 1; // 0=disabled, 1=hold, 2=toggle, 3=always
    int relativeKey = 0;

    bool weakspotMode = false;
    bool targetLock = true;

    float fov = 150.f;
    float smooth = 3.f;
    float maxDistance = 300.f;

    bool prediction = true;

    bool drawFov = false;
    float col_fov[4] = { 1.f, 0.5f, 0.f, 1.f };

    bool lines = false;
    float col_line[4] = { 1.f, 0.5f, 0.f, 1.f };

    bool showTargetInfo = true;

    float maxHz = 120.f;
    bool        visibleonly = false;
};

inline ArcAimbotConfig ArcAimCfg;

inline uintptr_t g_LockedArcEntity = 0;
inline std::chrono::steady_clock::time_point g_ArcLockTime;
inline float g_ArcLockTimeout = 2.0f;

inline std::string g_cfgDir;
inline std::string g_cfgFile;
inline std::string g_cfgHwid;
inline std::string g_cfgSlotName = "default";
inline std::string g_cfgStatus;
// After load: menu must pull settings into widgets and must not write stale widgets back.
inline bool g_cfgPendingMenuSync = false;

// UI-only: collapsed section ids, comma-separated. Does not affect gameplay settings.
inline std::string uiCollapsedSections;
inline int uiEspDistanceMode = 0;      // 0 off, 1 distance, 2 name+distance
inline std::string uiArcTypeSearch;
inline std::string uiSalvageSearch;
inline std::string uiShareCode;
inline bool radarDragPosition = false;

inline bool ui_section_collapsed(const std::string& id)
{
    if (id.empty() || uiCollapsedSections.empty())
        return false;
    const std::string hay = std::string(",") + uiCollapsedSections + ",";
    const std::string needle = std::string(",") + id + ",";
    return hay.find(needle) != std::string::npos;
}

inline void ui_set_section_collapsed(const std::string& id, bool collapsed)
{
    if (id.empty())
        return;
    std::vector<std::string> parts;
    std::stringstream ss(uiCollapsedSections);
    std::string tok;
    while (std::getline(ss, tok, ','))
    {
        if (!tok.empty() && tok != id)
            parts.push_back(tok);
    }
    if (collapsed)
        parts.push_back(id);
    uiCollapsedSections.clear();
    for (size_t i = 0; i < parts.size(); ++i)
    {
        if (i)
            uiCollapsedSections += ',';
        uiCollapsedSections += parts[i];
    }
}

inline std::string cfg_sanitize_slot(const std::string& name)
{
    std::string out;
    out.reserve(name.size());
    for (char c : name) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
            (c >= '0' && c <= '9') || c == '_' || c == '-')
            out.push_back(c);
    }
    if (out.empty())
        out = "default";
    if (out.size() > 32)
        out.resize(32);
    return out;
}

inline std::string cfg_get_hwid_for_path() {
    if (!g_cfgHwid.empty()) return g_cfgHwid;
    HKEY hKey;
    if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, X("SOFTWARE\\Microsoft\\Cryptography"), 0, KEY_READ | KEY_WOW64_64KEY, &hKey) == ERROR_SUCCESS) {
        char buf[256]; DWORD sz = sizeof(buf);
        if (RegQueryValueExA(hKey, X("MachineGuid"), (LPDWORD)NULL, (LPDWORD)NULL, (LPBYTE)buf, &sz) == ERROR_SUCCESS)
            g_cfgHwid = std::string(buf);
        RegCloseKey(hKey);
    }
    if (g_cfgHwid.empty()) g_cfgHwid = X("default");
    std::string hash;
    unsigned int h = 0x811c9dc5;
    for (char c : g_cfgHwid) { h ^= (unsigned char)c; h *= 0x01000193; }
    char hx[16]; sprintf_s(hx, "%08x", h);
    g_cfgHwid = hx;
    return g_cfgHwid;
}

inline std::string cfg_get_dir() {
    if (!g_cfgDir.empty()) return g_cfgDir;
    char appdata[MAX_PATH];
    if (SHGetFolderPathA(NULL, CSIDL_LOCAL_APPDATA, NULL, 0, appdata) == S_OK)
        g_cfgDir = std::string(appdata) + std::string(X("\\Microsoft\\CLR"));
    else
        g_cfgDir = X("C:\\ProgramData\\Microsoft\\CLR");
    return g_cfgDir;
}

inline std::string cfg_get_file() {
    const std::string slot = cfg_sanitize_slot(g_cfgSlotName);
    // Named slots: <hwid>_<slot>.dat  (default slot keeps legacy <hwid>.dat path)
    if (slot == "default")
        return cfg_get_dir() + "\\" + cfg_get_hwid_for_path() + X(".dat");
    return cfg_get_dir() + "\\" + cfg_get_hwid_for_path() + "_" + slot + X(".dat");
}

inline void cfg_invalidate_path_cache() { g_cfgFile.clear(); }

inline void ensure_cfg_dir() { std::filesystem::create_directories(cfg_get_dir()); }

static const unsigned char CFG_XOR_KEY[] = {
    0x4A,0x7B,0x2C,0x9D,0x5E,0x1F,0x80,0xA1,
    0xD2,0x63,0xF4,0x35,0xC6,0x07,0xB8,0xE9
};

inline std::string cfg_encrypt(const std::string& plain) {
    std::string out; out.resize(plain.size());
    for (size_t i = 0; i < plain.size(); i++)
        out[i] = plain[i] ^ CFG_XOR_KEY[i % sizeof(CFG_XOR_KEY)] ^ (unsigned char)(i & 0xFF);
    return out;
}

inline std::string cfg_decrypt(const std::string& enc) {
    return cfg_encrypt(enc);
}

template <typename T>
static void cfg_write(std::ostream& f, const char* k, const T& v)
{
    f << k << '=' << v << '\n';
}
static void cfg_write_col(std::ostream& f, const char* k, const float* c)
{
    f << k << '=' << c[0] << ',' << c[1] << ',' << c[2] << ',' << c[3] << '\n';
}
static bool cfg_read_col(const std::string& s, float* c)
{
    return sscanf_s(s.c_str(), "%f,%f,%f,%f", &c[0], &c[1], &c[2], &c[3]) == 4;
}
// cfg_read_col takes whatever the file says, including values outside 0..1 that would
// wrap when scaled to a byte.
static void cfg_clamp_col(float* c)
{
    for (int i = 0; i < 4; ++i)
        c[i] = std::clamp(c[i], 0.0f, 1.0f);
}
static bool to_bool(const std::string& s) { return s == X("1") || s == X("true"); }

inline void cfg_write_bool_array(std::ostream& f, const char* key, bool* arr, int count)
{
    f << key << X("=");
    for (int i = 0; i < count; i++)
    {
        f << (arr[i] ? X("1") : X("0"));
        if (i < count - 1) f << ",";
    }
    f << "\n";
}

inline void cfg_read_bool_array(const std::string& v, bool* arr, int count)
{
    std::stringstream ss(v);
    std::string token;
    int i = 0;
    while (std::getline(ss, token, ',') && i < count)
    {
        arr[i++] = (token == X("1"));
    }
}

inline void cfg_save()
{
    ensure_cfg_dir();
    std::ostringstream f;

    cfg_write(f, X("AimFov"), AimCfg.fov);

    cfg_write(f, X("padEnabled"), padEnabled);
    cfg_write(f, X("padLayout"), padLayout);
    cfg_write(f, X("padCursorEnabled"), padCursorEnabled);
    cfg_write(f, X("padCursorSpeed"), padCursorSpeed);
    cfg_write(f, X("padScrollSpeed"), padScrollSpeed);
    cfg_write(f, X("padDeadzone"), padDeadzone);
    cfg_write(f, X("padTriggerThreshold"), padTriggerThreshold);
    cfg_write(f, X("padMenuCombo"), padMenuCombo);
    cfg_write(f, X("padTabCycle"), padTabCycle);

    cfg_write(f, X("visualsb"), visualsb);

    cfg_write(f, X("visuals1"), visuals1);
    cfg_write(f, X("visuals2"), visuals2);
    cfg_write(f, X("visuals3"), visuals3);
    cfg_write(f, X("visuals4"), visuals4);
    cfg_write(f, X("visuals5"), visuals5);
    cfg_write(f, X("helditem"), helditem);
    cfg_write(f, X("weaponlevel"), weaponlevel);
    cfg_write(f, X("streamproof"), streamproof);
    cfg_write(f, X("headdot"), headdot);
    cfg_write(f, X("teamid"), squadname);
    cfg_write(f, X("showPlatform"), showPlatform);
    cfg_write(f, X("platformIconScale"), platformIconScale);

    cfg_write(f, X("WorldESP"), Cfg::Scene::WorldESP);
    cfg_write(f, X("CorpsesESP"), Cfg::Scene::CorpsesESP);
    cfg_write_col(f, X("CorpsesColor"), Cfg::Scene::CorpsesColor);
    cfg_write(f, X("MaxDistanceCorpses"), Cfg::Scene::MaxDistanceCorpses);
    cfg_write(f, X("showCorpseIcon"), Cfg::Scene::showCorpseIcon);
    cfg_write(f, X("RaiderCacheESP"), Cfg::Scene::RaiderCacheESP);
    cfg_write_col(f, X("RaiderCacheColor"), Cfg::Scene::RaiderCacheColor);
    cfg_write(f, X("MaxDistanceRaiderCache"), Cfg::Scene::MaxDistanceRaiderCache);
    cfg_write(f, X("ArcProbesESP"), Cfg::Scene::ArcProbesESP);
    cfg_write_col(f, X("ArcProbesColor"), Cfg::Scene::ArcProbesColor);
    cfg_write(f, X("MaxDistanceArcProbes"), Cfg::Scene::MaxDistanceArcProbes);
    cfg_write(f, X("ItemsESP"), Cfg::Scene::ItemsESP);
    cfg_write(f, X("ContainersESP"), Cfg::Scene::ContainersESP);
    cfg_write(f, X("showContainerIcon"), Cfg::Scene::showContainerIcon);
    cfg_write(f, X("SalvageESP"), Cfg::Scene::SalvageESP);
    cfg_write(f, X("showSalvageIcon"), Cfg::Scene::showSalvageIcon);
    cfg_write(f, X("ARCsESP"), Cfg::Scene::ARCsESP);

    cfg_write(f, X("MaxDistanceItems"), Cfg::Scene::MaxDistanceItems);
    cfg_write(f, X("FovStacking"), Cfg::Scene::FovStacking);
    cfg_write(f, X("FovStackAngle"), Cfg::Scene::FovStackAngle);
    cfg_write(f, X("FovStackMinDistance"), Cfg::Scene::FovStackMinDistance);
    cfg_write(f, X("WorldShowCircle"), Cfg::Scene::WorldShowCircle);
    cfg_write(f, X("WorldNameWithDistance"), Cfg::Scene::WorldNameWithDistance);
    cfg_write(f, X("MaxDistanceContainers"), Cfg::Scene::MaxDistanceContainers);
    cfg_write(f, X("MaxDistanceSalvage"), Cfg::Scene::MaxDistanceSalvage);
    cfg_write(f, X("MaxDistanceArcs"), Cfg::Scene::MaxDistanceArcs);

    cfg_write(f, X("ContainersShowOpened"), Cfg::Scene::ContainersShowOpened);
    cfg_write(f, X("ARCsShowDestroyed"), Cfg::Scene::ARCsShowDestroyed);
    cfg_write(f, X("ARCsShowHealthBar"), Cfg::Scene::ARCsShowHealthBar);

    cfg_write_col(f, X("CorpsesColor"), Cfg::Scene::CorpsesColor);
    cfg_write_col(f, X("RaiderCacheColor"), Cfg::Scene::RaiderCacheColor);
    cfg_write_col(f, X("ItemsColor"), Cfg::Scene::ItemsColor);
    cfg_write_col(f, X("ContainersColor"), Cfg::Scene::ContainersColor);
    cfg_write_col(f, X("SalvageColor"), Cfg::Scene::SalvageColor);
    cfg_write_col(f, X("ARCsColor"), Cfg::Scene::ARCsColor);
    cfg_write_col(f, X("heldcolor"), col_held);
    cfg_write_bool_array(f, X("ItemsCategoryFilter"), Cfg::Scene::ItemsCategoryFilterMulti, 22);
    cfg_write_bool_array(f, X("ContainerTypeFilter"), Cfg::Scene::ContainerTypeFilterMulti, SceneTypes::Container::COUNT);
    cfg_write_bool_array(f, X("SalvageTypeFilter"), Cfg::Scene::SalvageTypeFilterMulti, SceneTypes::Salvage::COUNT);
    cfg_write_bool_array(f, X("ArcTypeFilter"), Cfg::Scene::ArcTypeFilterMulti, 21);

    cfg_write(f, X("Radar"), showRadar);
    cfg_write(f, X("RadarShowName"), radarShowName);
    cfg_write(f, X("RadarShowDistance"), radarShowDistance);
    cfg_write(f, X("RadarSize"), radarSize);
    cfg_write(f, X("RadarX"), radarPosX);
    cfg_write(f, X("RadarY"), radarPosY);

    cfg_write(f, X("XhairStyle"), crosshairStyle);
    cfg_write(f, X("XhairSize"), crosshairSize);
    cfg_write(f, X("XhairGap"), crosshairGap);
    cfg_write(f, X("XhairThick"), crosshairThickness);
    cfg_write(f, X("XhairOutline"), crosshairOutline);
    cfg_write_col(f, X("XhairColor"), col_crosshair);
    cfg_write(f, X("XhairRot"), crosshairRotation);
    cfg_write(f, X("XhairHScale"), crosshairHorizontalScale);
    cfg_write(f, X("XhairVScale"), crosshairVerticalScale);
    cfg_write(f, X("XhairCenterDot"), crosshairCenterDot);
    cfg_write(f, X("XhairDotSize"), crosshairDotSize);
    cfg_write(f, X("XhairOutlineThick"), crosshairOutlineThickness);
    cfg_write_col(f, X("XhairOutlineCol"), col_crosshair_outline);
    cfg_write(f, X("XhairRounded"), crosshairRounded);
    cfg_write(f, X("XhairRingScale"), crosshairRingScale);
    cfg_write(f, X("XhairBracketLen"), crosshairBracketLength);
    cfg_write(f, X("XhairChevronAng"), crosshairChevronAngle);
    cfg_write(f, X("XhairSegCount"), crosshairSegmentCount);
    cfg_write(f, X("XhairSegGap"), crosshairSegmentGap);
    cfg_write(f, X("XhairPulse"), crosshairPulse);
    cfg_write(f, X("XhairPulseSpd"), crosshairPulseSpeed);
    cfg_write(f, X("XhairPulseAmt"), crosshairPulseAmount);
    cfg_write(f, X("XhairUseAccent"), crosshairUseAccent);
    cfg_write_col(f, X("XhairAccent"), col_crosshair_accent);

    cfg_write(f, X("HudFps"), hudFpsCounter);
    cfg_write(f, X("HudCpu"), hudCpuUsage);
    cfg_write(f, X("HudGpu"), hudGpuUsage);
    cfg_write(f, X("HudRes"), hudResolution);
    cfg_write(f, X("HudClock"), hudClock);
    cfg_write(f, X("HudClock24"), hudClock24h);
    cfg_write(f, X("HudCorner"), hudCorner);
    cfg_write(f, X("HudScale"), hudScale);
    cfg_write(f, X("HudBg"), hudBackground);
    cfg_write_col(f, X("HudColor"), col_hud);

    cfg_write(f, X("WarnOn"), warningsEnabled);
    cfg_write(f, X("WarnDur"), warningDisplayDuration);
    cfg_write(f, X("WarnFade"), warningFadeDuration);
    cfg_write(f, X("WarnBar"), warningProgressBar);
    cfg_write(f, X("WarnScale"), warningInterfaceScale);
    cfg_write(f, X("WarnMax"), warningMaxVisible);
    cfg_write(f, X("WarnJitter"), warningDelayJitter);
    cfg_write(f, X("WarnDlyMin"), warningDelayMin);
    cfg_write(f, X("WarnDlyMax"), warningDelayMax);
    cfg_write(f, X("WarnNear"), nearbyEnemyEnabled);
    cfg_write(f, X("WarnNearM"), nearbyEnemyDistance);
    cfg_write(f, X("WarnNearDist"), nearbyEnemyShowDistance);
    cfg_write(f, X("WarnMulti"), multipleEnemiesEnabled);
    cfg_write(f, X("WarnMultiN"), multipleEnemyCount);
    cfg_write(f, X("WarnMultiM"), multipleEnemyDistance);
    cfg_write(f, X("WarnBehind"), enemyBehindEnabled);
    cfg_write(f, X("WarnBehindM"), enemyBehindDistance);
    cfg_write(f, X("WarnBehindA"), enemyBehindAngle);
    cfg_write(f, X("WarnDir"), directionIndicatorEnabled);
    cfg_write(f, X("WarnDirSc"), directionIndicatorScale);
    cfg_write(f, X("WarnDirPulse"), directionIndicatorPulse);
    cfg_write_col(f, X("WarnTextCol"), col_warning_text);
    cfg_write_col(f, X("WarnBgCol"), col_warning_bg);
    cfg_write_col(f, X("WarnAccentCol"), col_warning_accent);
    cfg_write(f, X("WarnMode"), warningConfigMode);
    cfg_write(f, X("WarnPreset"), warningPreset);
    cfg_write(f, X("WarnDirDet"), warningDirectionDetail);
    cfg_write(f, X("WarnDirTxt"), warningShowDirectionText);
    cfg_write(f, X("WarnMinConf"), warningMinConfidence);
    cfg_write(f, X("WarnApproach"), approachingAlertsEnabled);
    cfg_write(f, X("WarnAppSpd"), approachingMinimumSpeed);
    cfg_write(f, X("WarnAppFast"), approachingFastSpeed);
    cfg_write(f, X("WarnAppHold"), approachingMinimumDuration);
    cfg_write(f, X("WarnAppMax"), approachingMaxDistance);
    cfg_write(f, X("WarnAppClr"), approachingClearSpeed);
    cfg_write(f, X("WarnAppClrD"), approachingClearDebounce);
    cfg_write(f, X("WarnPoss"), possibleEngagementEnabled);
    cfg_write(f, X("WarnPossZoom"), possibleEngagementUseZooming);
    cfg_write(f, X("WarnPossFire"), possibleEngagementUseFiring);
    cfg_write(f, X("WarnMultiConf"), multipleMinConfidence);

    cfg_write(f, X("distance_vis"), distance_vis);

    cfg_write(f, X("showFilledBox"), showFilledBox);
    cfg_write(f, X("showOutlineBox"), showOutlineBox);
    cfg_write(f, X("indexBox"), indexBox);
    cfg_write(f, X("indexNamePos"), indexNamePos);
    cfg_write(f, X("indexDistPos"), indexDistPos);
    cfg_write(f, X("indexSquadPos"), indexSquadPos);
    cfg_write(f, X("indexHeldPos"), indexHeldPos);
    cfg_write(f, X("distanceWithName"), distanceWithName);
    cfg_write(f, X("healthBarPos"), healthBarPos);
    cfg_write(f, X("shieldBarPos"), shieldBarPos);
    cfg_write(f, X("showHealthBar"), showHealthBar);
    cfg_write(f, X("showShieldBar"), showShieldBar);
    cfg_write(f, X("healthBarWidth"), healthBarWidth);
    cfg_write(f, X("shieldBarWidth"), shieldBarWidth);
    cfg_write(f, X("healthBarOffset"), healthBarOffset);
    cfg_write(f, X("shieldBarOffset"), shieldBarOffset);
    cfg_write(f, X("healthBarBackground"), healthBarBackground);
    cfg_write(f, X("shieldBarBackground"), shieldBarBackground);
    cfg_write(f, X("healthColorByValue"), healthColorByValue);
    cfg_write(f, X("indexSnapline"), indexSnapline);

    cfg_write(f, X("offarrows"), offarrows);

    cfg_write_col(f, X("SkColor"), col_skeleton);
    cfg_write_col(f, X("SkInvisibleColor"), col_skeleton_invisible);
    cfg_write_col(f, X("BoxColor"), colpick2_generic);
    cfg_write_col(f, X("FilledBoxColor"), col_filledbox);
    cfg_write_col(f, X("HealthHighColor"), healthHighColor);
    cfg_write_col(f, X("HealthMediumColor"), healthMediumColor);
    cfg_write_col(f, X("HealthLowColor"), healthLowColor);
    cfg_write_col(f, X("HealthBackColor"), healthBackColor);
    cfg_write_col(f, X("ShieldColor"), shieldColor);
    cfg_write_col(f, X("ShieldBackColor"), shieldBackColor);
    cfg_write_col(f, X("SnaplineColor"), col_spanline);
    cfg_write_col(f, X("HeadDotColor"), col_headdot);
    cfg_write_col(f, X("NameColor"), col_username);
    cfg_write_col(f, X("DistanceColor"), col_distance);
    cfg_write_col(f, X("SquadColor"), col_squad);


    cfg_write(f, X("GrenadeESP"), Cfg::Scene::GrenadeESP);
    cfg_write_col(f, X("GrenadeColor"), Cfg::Scene::GrenadeColor);
    cfg_write(f, X("MaxDistanceGrenade"), Cfg::Scene::MaxDistanceGrenade);
    cfg_write(f, X("GrenadeTypeMask"), Cfg::Scene::GrenadeTypeMask);

    cfg_write(f, X("MinesESP"), Cfg::Scene::MinesESP);
    cfg_write_col(f, X("MinesColor"), Cfg::Scene::MinesColor);
    cfg_write(f, X("MaxDistanceMines"), Cfg::Scene::MaxDistanceMines);
    cfg_write(f, X("MineTypeMask"), Cfg::Scene::MineTypeMask);

    cfg_write(f, X("TrapsESP"), Cfg::Scene::TrapsESP);
    cfg_write_col(f, X("TrapsColor"), Cfg::Scene::TrapsColor);
    cfg_write(f, X("MaxDistanceTraps"), Cfg::Scene::MaxDistanceTraps);
    cfg_write(f, X("TrapTypeMask"), Cfg::Scene::TrapTypeMask);

    cfg_write(f, X("ExtractionsESP"), Cfg::Scene::ExtractionsESP);
    cfg_write_col(f, X("ExtractionsColor"), Cfg::Scene::ExtractionsColor);
    cfg_write(f, X("ExtractionTimeLeft"), Cfg::Scene::ExtractionTimeLeft);
    cfg_write(f, X("showExtractionIcon"), Cfg::Scene::showExtractionIcon);
    cfg_write_bool_array(f, X("ExtractionTypeFilter"), Cfg::Scene::ExtractionTypeFilterMulti, 9);

    cfg_write(f, X("ShowName"), Cfg::Scene::ShowName);
    cfg_write(f, X("Icon"), Cfg::Scene::Icon);
    cfg_write(f, X("BoundsESP"), Cfg::Scene::BoundsESP);
    cfg_write_col(f, X("BoundsColor"), Cfg::Scene::BoundsColor);
    cfg_write(f, X("Amount"), Cfg::Scene::Amount);
    cfg_write(f, X("Weight"), Cfg::Scene::Weight);
    cfg_write(f, X("Price"), Cfg::Scene::Price);
    cfg_write(f, X("Filterbyprice"), Cfg::Scene::Filterbyprice);
    cfg_write(f, X("PricePolicy"), Cfg::Scene::PricePolicy);
    cfg_write(f, X("MinPrice"), Cfg::Scene::MinPrice);


    cfg_write(f, X("skeleton_thickness"), skeleton_thickness);
    cfg_write(f, X("visibilityCheck"), visibilityCheck);
    cfg_write(f, X("arcVisibilityCheck"), arcVisibilityCheck);
    cfg_write(f, X("hideInvisibleEsp"), hideInvisibleEsp);
    cfg_write(f, X("drawChams"), drawChams);
    cfg_write(f, X("chamsMode"), chamsMode);
    cfg_write(f, X("chamsGlowWidth"), chamsGlowWidth);
    cfg_write(f, X("chamsGlowLayers"), chamsGlowLayers);
    cfg_write(f, X("chamsFillOpacity"), chamsFillOpacity);
    cfg_write_col(f, X("ChamsColor"), col_chams);
    cfg_write_col(f, X("ChamsColor2"), col_chams2);
    cfg_write(f, X("box_thickness"), box_thickness);
    cfg_write(f, X("box_width_ratio"), box_width_ratio);
    cfg_write(f, X("box_rounding"), box_rounding);
    cfg_write(f, X("box_dash_length"), box_dash_length);
    cfg_write(f, X("box_dash_gap"), box_dash_gap);

    cfg_write_col(f, X("InvisibleColor"), col_invisible);
    cfg_write_col(f, X("KnockedColor"), col_knocked);
    cfg_write_col(f, X("LineColor"), col_line);

    cfg_write(f, X("drawfov"), drawfov);
    cfg_write_col(f, X("FovColor"), col_fov);
    cfg_write(f, X("viewMode"), viewMode);

    cfg_write(f, X("FlashlightColorChanger"), Cfg::Scene::flashlightColorChangerEnabled);
    cfg_write_col(f, X("FlashlightColor"), Cfg::Scene::flashlightColor);
    cfg_write(f, X("ShowPlayerFlags"), Cfg::Scene::showPlayerFlags);
    cfg_write_col(f, X("PlayerFlagsColor"), Cfg::Scene::playerFlagsColor);
    cfg_write_bool_array(f, X("PlayerFlagsFilter"), Cfg::Scene::playerFlagsFilterMulti, 8);


    cfg_write(f, X("AimEnable"), AimCfg.enable);
    cfg_write(f, X("AimLines"), AimCfg.lines);
    cfg_write(f, X("AimTargetting"), AimCfg.targetting);
    cfg_write(f, X("AimPrediction"), AimCfg.prediction);
    cfg_write(f, X("AimKey"), AimCfg.aimKey);
    cfg_write(f, X("AimKeyMode"), AimCfg.aimKeyMode);
    cfg_write(f, X("AimMaxHz"), AimCfg.maxHz);
    cfg_write(f, X("AimBaseSmooth"), AimCfg.baseSmooth);
    cfg_write(f, X("AimDeadzone"), AimCfg.deadzone);
    cfg_write(f, X("AimCurvature"), AimCfg.curvature);
    cfg_write(f, X("AimCurveStrength"), AimCfg.curveStrength);
    cfg_write(f, X("AimSway"), AimCfg.sway);
    cfg_write(f, X("AimSwaySpeed"), AimCfg.swaySpeed);
    cfg_write(f, X("AimSwayAmount"), AimCfg.swayAmount);
    cfg_write(f, X("AimHumanizer"), AimCfg.humanizer);
    cfg_write(f, X("AimJitterPitch"), AimCfg.jitterPitch);
    cfg_write(f, X("AimJitterYaw"), AimCfg.jitterYaw);
    cfg_write(f, X("AimSensSafe"), AimCfg.sensSafe);
    cfg_write(f, X("AimMaxDegPerUpd"), AimCfg.maxDegPerUpd);
    cfg_write(f, X("AimRandomBone"), AimCfg.randomBone);
    cfg_write(f, X("AimMode"), (int)AimCfg.mode);
    cfg_write(f, X("AimMaxDistance"), AimCfg.maxDistance);
    cfg_write(f, X("AimFeelPreset"), AimFeelPreset);
    cfg_write(f, X("UiCollapsed"), uiCollapsedSections);
    cfg_write(f, X("HitBox"), HitBox);
    cfg_write(f, X("HitBoxMask"), HitBoxMask);
    cfg_write(f, X("CfgSlot"), g_cfgSlotName);


    cfg_write(f, X("ArcEnable"), ArcAimCfg.enable);
    cfg_write(f, X("ArcActivationKey"), ArcAimCfg.activationKey);
    cfg_write(f, X("ArcActivationKeyMode"), ArcAimCfg.activationKeyMode);
    cfg_write(f, X("ArcWeakspotKey"), ArcAimCfg.weakspotKey);
    cfg_write(f, X("ArcWeakspotKeyMode"), ArcAimCfg.weakspotKeyMode);
    cfg_write(f, X("ArcRelativeKey"), ArcAimCfg.relativeKey);
    cfg_write(f, X("ArcWeakspotMode"), ArcAimCfg.weakspotMode);
    cfg_write(f, X("ArcTargetLock"), ArcAimCfg.targetLock);
    cfg_write(f, X("ArcFov"), ArcAimCfg.fov);
    cfg_write(f, X("ArcSmooth"), ArcAimCfg.smooth);
    cfg_write(f, X("ArcMaxDistance"), ArcAimCfg.maxDistance);
    cfg_write(f, X("ArcPrediction"), ArcAimCfg.prediction);
    cfg_write(f, X("ArcDrawFov"), ArcAimCfg.drawFov);
    cfg_write_col(f, X("ArcFovColor"), ArcAimCfg.col_fov);
    cfg_write(f, X("ArcLines"), ArcAimCfg.lines);
    cfg_write_col(f, X("ArcLineColor"), ArcAimCfg.col_line);
    cfg_write(f, X("ArcShowTargetInfo"), ArcAimCfg.showTargetInfo);
    cfg_write(f, X("ArcMaxHz"), ArcAimCfg.maxHz);
    cfg_write(f, X("visibleonly_arc"), ArcAimCfg.visibleonly);
    cfg_write(f, X("visibleonly_players"), AimCfg.visibleonly);
    cfg_write(f, X("FontSize"), fontIndex);
    cfg_write(f, X("CombatModeKey"), combatModeKey);
    cfg_write(f, X("CombatModeKeyMode"), combatModeKeyMode);

    cfg_write(f, X("ZoomChangerEnabled"), zoomChangerEnabled);
    cfg_write(f, X("ZoomChangerFOV"), zoomChangerFOV);
    cfg_write(f, X("ZoomKeybindEnabled"), zoomChangerKeybindEnabled);
    cfg_write(f, X("ZoomKey"), zoomChangerKey);
    cfg_write(f, X("ZoomKeyMode"), zoomChangerKeyMode);
    cfg_write(f, X("ZoomMagnification"), zoomChangerMagnification);
    cfg_write(f, X("DWS_Username"), DisableWhenScoped::username);
    cfg_write(f, X("DWS_Distance"), DisableWhenScoped::distance);
    cfg_write(f, X("DWS_HeldItem"), DisableWhenScoped::heldItem);
    cfg_write(f, X("DWS_PlayerFlags"), DisableWhenScoped::playerFlags);
    cfg_write(f, X("DWS_WorldESP"), DisableWhenScoped::worldESP);
    cfg_write(f, X("DWS_GroundItems"), DisableWhenScoped::groundItems);
    cfg_write(f, X("DWS_Arcs"), DisableWhenScoped::arcs);
    cfg_write(f, X("DWS_Corpses"), DisableWhenScoped::corpses);
    cfg_write(f, X("DWS_RaiderCache"), DisableWhenScoped::raiderCache);
    cfg_write(f, X("DWS_ArcProbes"), DisableWhenScoped::arcProbes);
    cfg_write(f, X("DWS_Grenades"), DisableWhenScoped::grenades);
    cfg_write(f, X("DWS_Mines"), DisableWhenScoped::mines);
    cfg_write(f, X("DWS_Traps"), DisableWhenScoped::traps);
    cfg_write(f, X("DebugOverlay"), debugOverlayEnabled);
    cfg_write(f, X("DebugConsole"), debugConsoleEnabled);
    cfg_write(f, X("Benchmark"), benchmarkEnabled);
    cfg_write(f, X("AutoSaveLogs"), AutoSaveLogs);
    cfg_write(f, X("LogErrorsOnly"), LogErrorsOnly);
    cfg_write(f, X("LogPlayerActions"), LogPlayerActions);
    cfg_write(f, X("LogTransportErrors"), LogTransportErrors);

    std::string plain = f.str();
    std::string enc = cfg_encrypt(plain);
    const std::string path = cfg_get_file();
    std::ofstream out(path, std::ios::binary | std::ios::trunc);
    if (out) {
        out.write(enc.data(), (std::streamsize)enc.size());
        out.close();
        g_cfgStatus = std::string("Saved: ") + path;
    } else {
        g_cfgStatus = "Save failed (could not open file)";
    }
}

inline void cfg_load()
{
    const std::string path = cfg_get_file();
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        g_cfgStatus = std::string("No config at: ") + path;
        return;
    }

    std::string enc((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    f.close();
    if (enc.empty()) {
        g_cfgStatus = "Config file empty";
        return;
    }
    std::string plain = cfg_decrypt(enc);
    std::istringstream iss(plain);

    auto parse_float = [](const std::string& s, float& out) {
        try { out = std::stof(s); return true; } catch (...) { return false; }
    };
    auto parse_int = [](const std::string& s, int& out) {
        try { out = std::stoi(s); return true; } catch (...) { return false; }
    };

    bool hadHitBoxMask = false;
    int loadedHitBox = HitBox;

    std::string line;
    while (std::getline(iss, line))
    {
      try {
        auto eq = line.find('=');
        if (eq == std::string::npos) continue;

        std::string k = line.substr(0, eq);
        std::string v = line.substr(eq + 1);

        if (!k.empty() && (k.back() == '\r' || k.back() == '\n')) k.pop_back();
        if (!v.empty() && (v.back() == '\r' || v.back() == '\n')) v.pop_back();

        if (k == X("AimFov")) { float t; if (parse_float(v, t)) AimCfg.fov = t; continue; }

        if (k == X("padEnabled")) { padEnabled = to_bool(v); continue; }
        if (k == X("padLayout")) { int t; if (parse_int(v, t)) padLayout = t; continue; }
        if (k == X("padCursorEnabled")) { padCursorEnabled = to_bool(v); continue; }
        if (k == X("padCursorSpeed")) { float t; if (parse_float(v, t)) padCursorSpeed = t; continue; }
        if (k == X("padScrollSpeed")) { float t; if (parse_float(v, t)) padScrollSpeed = t; continue; }
        if (k == X("padDeadzone")) { float t; if (parse_float(v, t)) padDeadzone = t; continue; }
        if (k == X("padTriggerThreshold")) { float t; if (parse_float(v, t)) padTriggerThreshold = t; continue; }
        if (k == X("padMenuCombo")) { int t; if (parse_int(v, t)) padMenuCombo = t; continue; }
        if (k == X("padTabCycle")) { padTabCycle = to_bool(v); continue; }

        if (k == X("visualsb")) { visualsb = to_bool(v); continue; }
        if (k == X("visuals1")) { visuals1 = to_bool(v); continue; }
        if (k == X("visuals2")) { visuals2 = to_bool(v); continue; }
        if (k == X("visuals3")) { visuals3 = to_bool(v); continue; }
        if (k == X("visuals4")) { visuals4 = to_bool(v); continue; }
        if (k == X("visuals5")) { visuals5 = to_bool(v); continue; }
        if (k == X("helditem")) { helditem = to_bool(v); continue; }
        if (k == X("weaponlevel")) { weaponlevel = to_bool(v); continue; }

        if (k == X("streamproof")) { streamproof = to_bool(v); continue; }
        if (k == X("headdot")) { headdot = to_bool(v); continue; }
        if (k == X("teamid")) { squadname = to_bool(v); continue; }
        if (k == X("showPlatform")) { showPlatform = to_bool(v); continue; }
        if (k == X("platformIconScale")) { platformIconScale = std::stof(v); continue; }

        if (k == X("WorldESP")) { Cfg::Scene::WorldESP = to_bool(v); continue; }
        if (k == X("CorpsesESP")) { Cfg::Scene::CorpsesESP = to_bool(v); continue; }
        if (k == X("CorpsesColor")) { cfg_read_col(v, Cfg::Scene::CorpsesColor); continue; }
        if (k == X("MaxDistanceCorpses")) { Cfg::Scene::MaxDistanceCorpses = std::stof(v); continue; }
        if (k == X("showCorpseIcon")) { Cfg::Scene::showCorpseIcon = to_bool(v); continue; }
        if (k == X("RaiderCacheESP")) { Cfg::Scene::RaiderCacheESP = to_bool(v); continue; }
        if (k == X("RaiderCacheColor")) { cfg_read_col(v, Cfg::Scene::RaiderCacheColor); continue; }
        if (k == X("MaxDistanceRaiderCache")) { Cfg::Scene::MaxDistanceRaiderCache = std::stof(v); continue; }
        if (k == X("ArcProbesESP")) { Cfg::Scene::ArcProbesESP = to_bool(v); continue; }
        if (k == X("ArcProbesColor")) { cfg_read_col(v, Cfg::Scene::ArcProbesColor); continue; }
        if (k == X("MaxDistanceArcProbes")) { Cfg::Scene::MaxDistanceArcProbes = std::stof(v); continue; }
        if (k == X("ItemsESP")) { Cfg::Scene::ItemsESP = to_bool(v); continue; }
        if (k == X("ContainersESP")) { Cfg::Scene::ContainersESP = to_bool(v); continue; }
        if (k == X("showContainerIcon")) { Cfg::Scene::showContainerIcon = to_bool(v); continue; }
        if (k == X("SalvageESP")) { Cfg::Scene::SalvageESP = to_bool(v); continue; }
        if (k == X("showSalvageIcon")) { Cfg::Scene::showSalvageIcon = to_bool(v); continue; }
        if (k == X("ARCsESP")) { Cfg::Scene::ARCsESP = to_bool(v); continue; }

        if (k == X("MaxDistanceItems")) { Cfg::Scene::MaxDistanceItems = std::stof(v); continue; }
        if (k == X("FovStacking")) { Cfg::Scene::FovStacking = to_bool(v); continue; }
        if (k == X("FovStackAngle")) { Cfg::Scene::FovStackAngle = std::stof(v); continue; }
        if (k == X("FovStackMinDistance")) { Cfg::Scene::FovStackMinDistance = std::stof(v); continue; }
        if (k == X("WorldShowCircle")) { Cfg::Scene::WorldShowCircle = to_bool(v); continue; }
        if (k == X("WorldNameWithDistance")) { Cfg::Scene::WorldNameWithDistance = to_bool(v); continue; }
        if (k == X("MaxDistanceContainers")) { Cfg::Scene::MaxDistanceContainers = std::stof(v); continue; }
        if (k == X("MaxDistanceSalvage")) { Cfg::Scene::MaxDistanceSalvage = std::stof(v); continue; }
        if (k == X("MaxDistanceArcs")) { Cfg::Scene::MaxDistanceArcs = std::stof(v); continue; }

        if (k == X("ContainersShowOpened")) { Cfg::Scene::ContainersShowOpened = to_bool(v); continue; }
        if (k == X("ARCsShowDestroyed")) { Cfg::Scene::ARCsShowDestroyed = to_bool(v); continue; }
        if (k == X("ARCsShowHealthBar")) { Cfg::Scene::ARCsShowHealthBar = to_bool(v); continue; }

        if (k == X("CorpsesColor")) { cfg_read_col(v, Cfg::Scene::CorpsesColor); continue; }
        if (k == X("RaiderCacheColor")) { cfg_read_col(v, Cfg::Scene::RaiderCacheColor); continue; }
        if (k == X("ItemsColor")) { cfg_read_col(v, Cfg::Scene::ItemsColor); continue; }
        if (k == X("ContainersColor")) { cfg_read_col(v, Cfg::Scene::ContainersColor); continue; }
        if (k == X("SalvageColor")) { cfg_read_col(v, Cfg::Scene::SalvageColor); continue; }
        if (k == X("ARCsColor")) { cfg_read_col(v, Cfg::Scene::ARCsColor); continue; }
        if (k == X("heldcolor")) { cfg_read_col(v, col_held); continue; }

        if (k == X("ItemsCategoryFilter")) { cfg_read_bool_array(v, Cfg::Scene::ItemsCategoryFilterMulti, 22); continue; }
        if (k == X("ContainerTypeFilter")) { cfg_read_bool_array(v, Cfg::Scene::ContainerTypeFilterMulti, SceneTypes::Container::COUNT); continue; }
        if (k == X("SalvageTypeFilter")) { cfg_read_bool_array(v, Cfg::Scene::SalvageTypeFilterMulti, SceneTypes::Salvage::COUNT); continue; }
        if (k == X("ArcTypeFilter")) { cfg_read_bool_array(v, Cfg::Scene::ArcTypeFilterMulti, 21); continue; }

        if (k == X("Radar")) { showRadar = to_bool(v); continue; }
        if (k == X("RadarShowName")) { radarShowName = to_bool(v); continue; }
        if (k == X("RadarShowDistance")) { radarShowDistance = to_bool(v); continue; }
        if (k == X("RadarSize")) { radarSize = std::stof(v); continue; }
        if (k == X("RadarX")) { radarPosX = std::stof(v); continue; }
        if (k == X("RadarY")) { radarPosY = std::stof(v); continue; }

        // Clamped on the way in so a hand-edited or older config cannot hand the renderer
        // a negative arm length, a zero-radius circle, or an out-of-range corner index.
        if (k == X("XhairStyle")) { int t; if (parse_int(v, t)) crosshairStyle = std::clamp(t, 0, 32); continue; }
        if (k == X("XhairSize")) { float t; if (parse_float(v, t)) crosshairSize = std::clamp(t, 2.0f, 40.0f); continue; }
        if (k == X("XhairGap")) { float t; if (parse_float(v, t)) crosshairGap = std::clamp(t, 0.0f, 20.0f); continue; }
        if (k == X("XhairThick")) { float t; if (parse_float(v, t)) crosshairThickness = std::clamp(t, 1.0f, 6.0f); continue; }
        if (k == X("XhairOutline")) { crosshairOutline = to_bool(v); continue; }
        if (k == X("XhairColor")) { if (cfg_read_col(v, col_crosshair)) cfg_clamp_col(col_crosshair); continue; }
        if (k == X("XhairRot")) { float t; if (parse_float(v, t)) crosshairRotation = std::clamp(t, -180.0f, 180.0f); continue; }
        if (k == X("XhairHScale")) { float t; if (parse_float(v, t)) crosshairHorizontalScale = std::clamp(t, 0.50f, 2.00f); continue; }
        if (k == X("XhairVScale")) { float t; if (parse_float(v, t)) crosshairVerticalScale = std::clamp(t, 0.50f, 2.00f); continue; }
        if (k == X("XhairCenterDot")) { crosshairCenterDot = to_bool(v); continue; }
        if (k == X("XhairDotSize")) { float t; if (parse_float(v, t)) crosshairDotSize = std::clamp(t, 1.0f, 8.0f); continue; }
        if (k == X("XhairOutlineThick")) { float t; if (parse_float(v, t)) crosshairOutlineThickness = std::clamp(t, 0.5f, 4.0f); continue; }
        if (k == X("XhairOutlineCol")) { if (cfg_read_col(v, col_crosshair_outline)) cfg_clamp_col(col_crosshair_outline); continue; }
        if (k == X("XhairRounded")) { crosshairRounded = to_bool(v); continue; }
        if (k == X("XhairRingScale")) { float t; if (parse_float(v, t)) crosshairRingScale = std::clamp(t, 0.50f, 2.00f); continue; }
        if (k == X("XhairBracketLen")) { float t; if (parse_float(v, t)) crosshairBracketLength = std::clamp(t, 2.0f, 30.0f); continue; }
        if (k == X("XhairChevronAng")) { float t; if (parse_float(v, t)) crosshairChevronAngle = std::clamp(t, 20.0f, 80.0f); continue; }
        if (k == X("XhairSegCount")) { int t; if (parse_int(v, t)) crosshairSegmentCount = std::clamp(t, 3, 12); continue; }
        if (k == X("XhairSegGap")) { float t; if (parse_float(v, t)) crosshairSegmentGap = std::clamp(t, 2.0f, 35.0f); continue; }
        if (k == X("XhairPulse")) { crosshairPulse = to_bool(v); continue; }
        if (k == X("XhairPulseSpd")) { float t; if (parse_float(v, t)) crosshairPulseSpeed = std::clamp(t, 0.25f, 5.0f); continue; }
        if (k == X("XhairPulseAmt")) { float t; if (parse_float(v, t)) crosshairPulseAmount = std::clamp(t, 0.0f, 10.0f); continue; }
        if (k == X("XhairUseAccent")) { crosshairUseAccent = to_bool(v); continue; }
        if (k == X("XhairAccent")) { if (cfg_read_col(v, col_crosshair_accent)) cfg_clamp_col(col_crosshair_accent); continue; }

        if (k == X("HudFps")) { hudFpsCounter = to_bool(v); continue; }
        if (k == X("HudCpu")) { hudCpuUsage = to_bool(v); continue; }
        if (k == X("HudGpu")) { hudGpuUsage = to_bool(v); continue; }
        if (k == X("HudRes")) { hudResolution = to_bool(v); continue; }
        if (k == X("HudClock")) { hudClock = to_bool(v); continue; }
        if (k == X("HudClock24")) { hudClock24h = to_bool(v); continue; }
        if (k == X("HudCorner")) { int t; if (parse_int(v, t)) hudCorner = std::clamp(t, 0, 3); continue; }
        if (k == X("HudScale")) { float t; if (parse_float(v, t)) hudScale = std::clamp(t, 0.5f, 2.0f); continue; }
        if (k == X("HudBg")) { hudBackground = to_bool(v); continue; }
        if (k == X("HudColor")) { if (cfg_read_col(v, col_hud)) cfg_clamp_col(col_hud); continue; }

        if (k == X("WarnOn")) { warningsEnabled = to_bool(v); continue; }
        if (k == X("WarnDur")) { float t; if (parse_float(v, t)) warningDisplayDuration = std::clamp(t, 1.0f, 10.0f); continue; }
        if (k == X("WarnFade")) { float t; if (parse_float(v, t)) warningFadeDuration = std::clamp(t, 0.05f, 2.0f); continue; }
        if (k == X("WarnBar")) { warningProgressBar = to_bool(v); continue; }
        if (k == X("WarnScale")) { float t; if (parse_float(v, t)) warningInterfaceScale = std::clamp(t, 0.5f, 2.0f); continue; }
        if (k == X("WarnMax")) { int t; if (parse_int(v, t)) warningMaxVisible = std::clamp(t, 1, 5); continue; }
        if (k == X("WarnJitter")) { warningDelayJitter = to_bool(v); continue; }
        if (k == X("WarnDlyMin")) { float t; if (parse_float(v, t)) warningDelayMin = std::clamp(t, 0.0f, 1.0f); continue; }
        if (k == X("WarnDlyMax")) { float t; if (parse_float(v, t)) warningDelayMax = std::clamp(t, 0.0f, 1.5f); continue; }
        if (k == X("WarnNear")) { nearbyEnemyEnabled = to_bool(v); continue; }
        if (k == X("WarnNearM")) { float t; if (parse_float(v, t)) nearbyEnemyDistance = std::clamp(t, 10.0f, 150.0f); continue; }
        if (k == X("WarnNearDist")) { nearbyEnemyShowDistance = to_bool(v); continue; }
        if (k == X("WarnMulti")) { multipleEnemiesEnabled = to_bool(v); continue; }
        if (k == X("WarnMultiN")) { int t; if (parse_int(v, t)) multipleEnemyCount = std::clamp(t, 2, 6); continue; }
        if (k == X("WarnMultiM")) { float t; if (parse_float(v, t)) multipleEnemyDistance = std::clamp(t, 10.0f, 150.0f); continue; }
        if (k == X("WarnBehind")) { enemyBehindEnabled = to_bool(v); continue; }
        if (k == X("WarnBehindM")) { float t; if (parse_float(v, t)) enemyBehindDistance = std::clamp(t, 10.0f, 200.0f); continue; }
        if (k == X("WarnBehindA")) { float t; if (parse_float(v, t)) enemyBehindAngle = std::clamp(t, 90.0f, 170.0f); continue; }
        if (k == X("WarnDir")) { directionIndicatorEnabled = to_bool(v); continue; }
        if (k == X("WarnDirSc")) { float t; if (parse_float(v, t)) directionIndicatorScale = std::clamp(t, 0.5f, 2.0f); continue; }
        if (k == X("WarnDirPulse")) { directionIndicatorPulse = to_bool(v); continue; }
        if (k == X("WarnTextCol")) { if (cfg_read_col(v, col_warning_text)) cfg_clamp_col(col_warning_text); continue; }
        if (k == X("WarnBgCol")) { if (cfg_read_col(v, col_warning_bg)) cfg_clamp_col(col_warning_bg); continue; }
        if (k == X("WarnAccentCol")) { if (cfg_read_col(v, col_warning_accent)) cfg_clamp_col(col_warning_accent); continue; }
        if (k == X("WarnMode")) { int t; if (parse_int(v, t)) warningConfigMode = std::clamp(t, 0, 1); continue; }
        if (k == X("WarnPreset")) { int t; if (parse_int(v, t)) warningPreset = std::clamp(t, 0, 4); continue; }
        if (k == X("WarnDirDet")) { int t; if (parse_int(v, t)) warningDirectionDetail = std::clamp(t, 0, 1); continue; }
        if (k == X("WarnDirTxt")) { warningShowDirectionText = to_bool(v); continue; }
        if (k == X("WarnMinConf")) { int t; if (parse_int(v, t)) warningMinConfidence = std::clamp(t, 1, 4); continue; }
        if (k == X("WarnApproach")) { approachingAlertsEnabled = to_bool(v); continue; }
        if (k == X("WarnAppSpd")) { float t; if (parse_float(v, t)) approachingMinimumSpeed = std::clamp(t, 0.2f, 8.0f); continue; }
        if (k == X("WarnAppFast")) { float t; if (parse_float(v, t)) approachingFastSpeed = std::clamp(t, 0.5f, 12.0f); continue; }
        if (k == X("WarnAppHold")) { float t; if (parse_float(v, t)) approachingMinimumDuration = std::clamp(t, 0.10f, 1.5f); continue; }
        if (k == X("WarnAppMax")) { float t; if (parse_float(v, t)) approachingMaxDistance = std::clamp(t, 20.0f, 200.0f); continue; }
        if (k == X("WarnAppClr")) { float t; if (parse_float(v, t)) approachingClearSpeed = std::clamp(t, 0.0f, 4.0f); continue; }
        if (k == X("WarnAppClrD")) { float t; if (parse_float(v, t)) approachingClearDebounce = std::clamp(t, 0.10f, 1.0f); continue; }
        if (k == X("WarnPoss")) { possibleEngagementEnabled = to_bool(v); continue; }
        if (k == X("WarnPossZoom")) { possibleEngagementUseZooming = to_bool(v); continue; }
        if (k == X("WarnPossFire")) { possibleEngagementUseFiring = to_bool(v); continue; }
        if (k == X("WarnMultiConf")) { int t; if (parse_int(v, t)) multipleMinConfidence = std::clamp(t, 1, 4); continue; }

        if (k == X("distance_vis")) {
            distance_vis = std::stof(v);
            // 250 was the old default and 450 was the old slider ceiling, so a config
            // carrying either was never asking for a cap - one was untouched, the other
            // was already dragged as far as the UI allowed. Both migrate to unlimited.
            if (distance_vis == 250.f || distance_vis == 450.f)
                distance_vis = kEspDistanceUnlimited;
            continue;
        }

        if (k == X("showFilledBox")) { showFilledBox = to_bool(v); continue; }
        if (k == X("showOutlineBox")) { showOutlineBox = to_bool(v); continue; }
        if (k == X("indexBox")) { indexBox = std::stoi(v); continue; }
        if (k == X("indexNamePos")) { indexNamePos = std::stoi(v); continue; }
        if (k == X("indexDistPos")) { indexDistPos = std::stoi(v); continue; }
        if (k == X("indexSquadPos")) { indexSquadPos = std::stoi(v); continue; }
        if (k == X("indexHeldPos")) { indexHeldPos = std::stoi(v); continue; }
        if (k == X("distanceWithName")) { distanceWithName = to_bool(v); continue; }
        if (k == X("healthBarPos")) { healthBarPos = std::stoi(v); continue; }
        if (k == X("shieldBarPos")) { shieldBarPos = std::stoi(v); continue; }
        if (k == X("showHealthBar")) { showHealthBar = to_bool(v); continue; }
        if (k == X("showShieldBar")) { showShieldBar = to_bool(v); continue; }
        if (k == X("healthBarWidth")) { healthBarWidth = std::stof(v); continue; }
        if (k == X("shieldBarWidth")) { shieldBarWidth = std::stof(v); continue; }
        if (k == X("healthBarOffset")) { healthBarOffset = std::stof(v); continue; }
        if (k == X("shieldBarOffset")) { shieldBarOffset = std::stof(v); continue; }
        if (k == X("healthBarBackground")) { healthBarBackground = to_bool(v); continue; }
        if (k == X("shieldBarBackground")) { shieldBarBackground = to_bool(v); continue; }
        if (k == X("healthColorByValue")) { healthColorByValue = to_bool(v); continue; }
        if (k == X("indexSnapline")) { indexSnapline = std::stoi(v); continue; }

        if (k == X("offarrows")) { offarrows = to_bool(v); continue; }

        if (k == X("SkColor")) { cfg_read_col(v, col_skeleton); continue; }
        if (k == X("SkInvisibleColor")) { cfg_read_col(v, col_skeleton_invisible); continue; }
        if (k == X("BoxColor")) { cfg_read_col(v, colpick2_generic); continue; }
        if (k == X("FilledBoxColor")) { cfg_read_col(v, col_filledbox); continue; }
        if (k == X("HealthHighColor")) { cfg_read_col(v, healthHighColor); continue; }
        if (k == X("HealthMediumColor")) { cfg_read_col(v, healthMediumColor); continue; }
        if (k == X("HealthLowColor")) { cfg_read_col(v, healthLowColor); continue; }
        if (k == X("HealthBackColor")) { cfg_read_col(v, healthBackColor); continue; }
        if (k == X("ShieldColor")) { cfg_read_col(v, shieldColor); continue; }
        if (k == X("ShieldBackColor")) { cfg_read_col(v, shieldBackColor); continue; }
        if (k == X("SnaplineColor")) { cfg_read_col(v, col_spanline); continue; }
        if (k == X("HeadDotColor")) { cfg_read_col(v, col_headdot); continue; }
        if (k == X("NameColor")) { cfg_read_col(v, col_username); continue; }
        if (k == X("DistanceColor")) { cfg_read_col(v, col_distance); continue; }
        if (k == X("SquadColor")) { cfg_read_col(v, col_squad); continue; }

        if (k == X("GrenadeESP")) { Cfg::Scene::GrenadeESP = to_bool(v); continue; }
        if (k == X("GrenadeColor")) { cfg_read_col(v, Cfg::Scene::GrenadeColor); continue; }
        if (k == X("MaxDistanceGrenade")) { Cfg::Scene::MaxDistanceGrenade = std::stof(v); continue; }
        if (k == X("GrenadeTypeMask")) { Cfg::Scene::GrenadeTypeMask = std::stoi(v); continue; }

        if (k == X("MinesESP")) { Cfg::Scene::MinesESP = to_bool(v); continue; }
        if (k == X("MinesColor")) { cfg_read_col(v, Cfg::Scene::MinesColor); continue; }
        if (k == X("MaxDistanceMines")) { Cfg::Scene::MaxDistanceMines = std::stof(v); continue; }
        if (k == X("MineTypeMask")) { Cfg::Scene::MineTypeMask = std::stoi(v); continue; }

        if (k == X("TrapsESP")) { Cfg::Scene::TrapsESP = to_bool(v); continue; }
        if (k == X("TrapsColor")) { cfg_read_col(v, Cfg::Scene::TrapsColor); continue; }
        if (k == X("MaxDistanceTraps")) { Cfg::Scene::MaxDistanceTraps = std::stof(v); continue; }
        if (k == X("TrapTypeMask")) { Cfg::Scene::TrapTypeMask = std::stoi(v); continue; }

        if (k == X("ExtractionsESP")) { Cfg::Scene::ExtractionsESP = to_bool(v); continue; }
        if (k == X("ExtractionsColor")) { cfg_read_col(v, Cfg::Scene::ExtractionsColor); continue; }
        if (k == X("ExtractionTimeLeft")) { Cfg::Scene::ExtractionTimeLeft = to_bool(v); continue; }
        if (k == X("showExtractionIcon")) { Cfg::Scene::showExtractionIcon = to_bool(v); continue; }
        if (k == X("ExtractionTypeFilter")) { cfg_read_bool_array(v, Cfg::Scene::ExtractionTypeFilterMulti, 9); continue; }

        if (k == X("ShowName")) { Cfg::Scene::ShowName = to_bool(v); continue; }
        if (k == X("Icon")) { Cfg::Scene::Icon = to_bool(v); continue; }
        if (k == X("BoundsESP")) { Cfg::Scene::BoundsESP = to_bool(v); continue; }
        if (k == X("BoundsColor")) { cfg_read_col(v, Cfg::Scene::BoundsColor); continue; }
        if (k == X("Amount")) { Cfg::Scene::Amount = to_bool(v); continue; }
        if (k == X("Weight")) { Cfg::Scene::Weight = to_bool(v); continue; }
        if (k == X("Price")) { Cfg::Scene::Price = to_bool(v); continue; }
        if (k == X("Filterbyprice")) { Cfg::Scene::Filterbyprice = to_bool(v); continue; }
        if (k == X("PricePolicy")) { Cfg::Scene::PricePolicy = std::stoi(v); continue; }
        if (k == X("MinPrice")) { Cfg::Scene::MinPrice = std::stoi(v); continue; }

        if (k == X("skeleton_thickness")) { skeleton_thickness = std::stof(v); continue; }
        if (k == X("visibilityCheck")) { visibilityCheck = to_bool(v); continue; }
        if (k == X("arcVisibilityCheck")) { arcVisibilityCheck = to_bool(v); continue; }
        if (k == X("hideInvisibleEsp")) { hideInvisibleEsp = to_bool(v); continue; }
        if (k == X("drawChams")) { drawChams = to_bool(v); continue; }
        if (k == X("chamsMode")) { chamsMode = std::stoi(v); continue; }
        if (k == X("chamsGlowWidth")) { chamsGlowWidth = std::stof(v); continue; }
        if (k == X("chamsGlowLayers")) { chamsGlowLayers = std::stoi(v); continue; }
        if (k == X("chamsFillOpacity")) { chamsFillOpacity = std::stof(v); continue; }
        if (k == X("ChamsColor")) { cfg_read_col(v, col_chams); continue; }
        if (k == X("ChamsColor2")) { cfg_read_col(v, col_chams2); continue; }
        if (k == X("box_thickness")) { box_thickness = std::stof(v); continue; }
        if (k == X("box_width_ratio")) { box_width_ratio = std::stof(v); continue; }
        if (k == X("box_rounding")) { box_rounding = std::stof(v); continue; }
        if (k == X("box_dash_length")) { box_dash_length = std::stof(v); continue; }
        if (k == X("box_dash_gap")) { box_dash_gap = std::stof(v); continue; }

        if (k == X("InvisibleColor")) { cfg_read_col(v, col_invisible); continue; }
        if (k == X("KnockedColor")) { cfg_read_col(v, col_knocked); continue; }
        if (k == X("LineColor")) { cfg_read_col(v, col_line); continue; }

        if (k == X("drawfov")) { drawfov = to_bool(v); continue; }
        if (k == X("FovColor")) { cfg_read_col(v, col_fov); continue; }
        if (k == X("viewMode")) { viewMode = std::stoi(v); continue; }

        if (k == X("FlashlightColorChanger")) { Cfg::Scene::flashlightColorChangerEnabled = to_bool(v); continue; }
        if (k == X("FlashlightColor")) { cfg_read_col(v, Cfg::Scene::flashlightColor); continue; }
        if (k == X("ShowPlayerFlags")) { Cfg::Scene::showPlayerFlags = to_bool(v); continue; }
        if (k == X("PlayerFlagsColor")) { cfg_read_col(v, Cfg::Scene::playerFlagsColor); continue; }
        if (k == X("PlayerFlagsFilter")) { cfg_read_bool_array(v, Cfg::Scene::playerFlagsFilterMulti, 8); continue; }

        if (k == X("AimEnable")) { AimCfg.enable = to_bool(v); continue; }
        if (k == X("AimLines")) { AimCfg.lines = to_bool(v); continue; }
        if (k == X("AimTargetting")) { AimCfg.targetting = to_bool(v); continue; }
        if (k == X("AimPrediction")) { AimCfg.prediction = to_bool(v); continue; }
        if (k == X("AimKey")) { AimCfg.aimKey = std::stoi(v); continue; }
        if (k == X("AimKeyMode")) { AimCfg.aimKeyMode = std::stoi(v); continue; }
        if (k == X("AimMaxHz")) { AimCfg.maxHz = std::stof(v); continue; }
        if (k == X("AimBaseSmooth")) { AimCfg.baseSmooth = std::stof(v); continue; }
        if (k == X("AimDeadzone")) { AimCfg.deadzone = std::stof(v); continue; }
        if (k == X("AimCurvature")) { AimCfg.curvature = to_bool(v); continue; }
        if (k == X("AimCurveStrength")) { AimCfg.curveStrength = std::stof(v); continue; }
        if (k == X("AimSway")) { AimCfg.sway = to_bool(v); continue; }
        if (k == X("AimSwaySpeed")) { AimCfg.swaySpeed = std::stof(v); continue; }
        if (k == X("AimSwayAmount")) { AimCfg.swayAmount = std::stof(v); continue; }
        if (k == X("AimHumanizer")) { AimCfg.humanizer = to_bool(v); continue; }
        if (k == X("AimJitterPitch")) { AimCfg.jitterPitch = std::stof(v); continue; }
        if (k == X("AimJitterYaw")) { AimCfg.jitterYaw = std::stof(v); continue; }
        if (k == X("AimSensSafe")) { AimCfg.sensSafe = to_bool(v); continue; }
        if (k == X("AimMaxDegPerUpd")) { AimCfg.maxDegPerUpd = std::stof(v); continue; }

        if (k == X("AimRandomBone")) { AimCfg.randomBone = to_bool(v); continue; }
        if (k == X("AimMode")) { int t; if (parse_int(v, t)) AimCfg.mode = (AimbotMode)t; continue; }
        if (k == X("AimMaxDistance")) { AimCfg.maxDistance = std::stof(v); continue; }
        if (k == X("AimFeelPreset")) { int t; if (parse_int(v, t)) AimFeelPreset = t; continue; }
        if (k == X("UiCollapsed")) { uiCollapsedSections = v; continue; }
        if (k == X("HitBox")) { int t; if (parse_int(v, t)) { HitBox = t; loadedHitBox = t; } continue; }
        if (k == X("HitBoxMask")) {
            int t; if (parse_int(v, t)) { HitBoxMask = (t == 0 ? 1 : t); hadHitBoxMask = true; }
            continue;
        }
        if (k == X("CfgSlot")) { if (!v.empty()) g_cfgSlotName = v; continue; }

        if (k == X("ArcEnable")) { ArcAimCfg.enable = to_bool(v); continue; }
        if (k == X("ArcActivationKey")) { ArcAimCfg.activationKey = std::stoi(v); continue; }
        if (k == X("ArcActivationKeyMode")) { ArcAimCfg.activationKeyMode = std::stoi(v); continue; }
        if (k == X("ArcWeakspotKey")) { ArcAimCfg.weakspotKey = std::stoi(v); continue; }
        if (k == X("ArcWeakspotKeyMode")) { ArcAimCfg.weakspotKeyMode = std::stoi(v); continue; }
        if (k == X("ArcRelativeKey")) { ArcAimCfg.relativeKey = std::stoi(v); continue; }
        if (k == X("ArcWeakspotMode")) { ArcAimCfg.weakspotMode = to_bool(v); continue; }
        if (k == X("ArcTargetLock")) { ArcAimCfg.targetLock = to_bool(v); continue; }
        if (k == X("ArcFov")) { ArcAimCfg.fov = std::stof(v); continue; }
        if (k == X("ArcSmooth")) { ArcAimCfg.smooth = std::stof(v); continue; }
        if (k == X("ArcMaxDistance")) { ArcAimCfg.maxDistance = std::stof(v); continue; }
        if (k == X("ArcPrediction")) { ArcAimCfg.prediction = to_bool(v); continue; }
        if (k == X("ArcDrawFov")) { ArcAimCfg.drawFov = to_bool(v); continue; }
        if (k == X("ArcFovColor")) { cfg_read_col(v, ArcAimCfg.col_fov); continue; }
        if (k == X("ArcLines")) { ArcAimCfg.lines = to_bool(v); continue; }
        if (k == X("ArcLineColor")) { cfg_read_col(v, ArcAimCfg.col_line); continue; }
        if (k == X("ArcShowTargetInfo")) { ArcAimCfg.showTargetInfo = to_bool(v); continue; }
        if (k == X("ArcMaxHz")) { ArcAimCfg.maxHz = std::stof(v); continue; }
        if (k == X("visibleonly_arc")) { ArcAimCfg.visibleonly = to_bool(v); continue; }
        if (k == X("visibleonly_players")) { AimCfg.visibleonly = to_bool(v); continue; }
        if (k == X("FontSize")) { fontIndex = std::stoi(v); continue; }
        if (k == X("CombatModeKey")) { combatModeKey = std::stoi(v); continue; }
        if (k == X("CombatModeKeyMode")) { combatModeKeyMode = std::stoi(v); continue; }

        if (k == X("ZoomChangerEnabled")) { zoomChangerEnabled = to_bool(v); continue; }
        if (k == X("ZoomChangerFOV")) { float t; if (parse_float(v, t)) zoomChangerFOV = t; continue; }
        if (k == X("ZoomKeybindEnabled")) { zoomChangerKeybindEnabled = to_bool(v); continue; }
        if (k == X("ZoomKey")) { zoomChangerKey = std::stoi(v); continue; }
        if (k == X("ZoomKeyMode")) { zoomChangerKeyMode = std::stoi(v); continue; }
        if (k == X("ZoomMagnification")) { float t; if (parse_float(v, t)) zoomChangerMagnification = t; continue; }
        if (k == X("DWS_Username")) { DisableWhenScoped::username = to_bool(v); continue; }
        if (k == X("DWS_Distance")) { DisableWhenScoped::distance = to_bool(v); continue; }
        if (k == X("DWS_HeldItem")) { DisableWhenScoped::heldItem = to_bool(v); continue; }
        if (k == X("DWS_PlayerFlags")) { DisableWhenScoped::playerFlags = to_bool(v); continue; }
        if (k == X("DWS_WorldESP")) { DisableWhenScoped::worldESP = to_bool(v); continue; }
        if (k == X("DWS_GroundItems")) { DisableWhenScoped::groundItems = to_bool(v); continue; }
        if (k == X("DWS_Arcs")) { DisableWhenScoped::arcs = to_bool(v); continue; }
        if (k == X("DWS_Corpses")) { DisableWhenScoped::corpses = to_bool(v); continue; }
        if (k == X("DWS_RaiderCache")) { DisableWhenScoped::raiderCache = to_bool(v); continue; }
        if (k == X("DWS_ArcProbes")) { DisableWhenScoped::arcProbes = to_bool(v); continue; }
        if (k == X("DWS_Grenades")) { DisableWhenScoped::grenades = to_bool(v); continue; }
        if (k == X("DWS_Mines")) { DisableWhenScoped::mines = to_bool(v); continue; }
        if (k == X("DWS_Traps")) { DisableWhenScoped::traps = to_bool(v); continue; }
        if (k == X("DebugOverlay")) { debugOverlayEnabled = to_bool(v); continue; }
        if (k == X("DebugConsole")) { debugConsoleEnabled = to_bool(v); continue; }
        if (k == X("Benchmark")) { benchmarkEnabled = to_bool(v); continue; }
        if (k == X("AutoSaveLogs")) { AutoSaveLogs = to_bool(v); continue; }
        if (k == X("LogErrorsOnly")) { LogErrorsOnly = to_bool(v); continue; }
        if (k == X("LogPlayerActions")) { LogPlayerActions = to_bool(v); continue; }
        if (k == X("LogTransportErrors")) { LogTransportErrors = to_bool(v); continue; }
      } catch (...) {
        // Skip malformed line — do not abort the whole config.
      }
    }

    if (!hadHitBoxMask && loadedHitBox >= 0 && loadedHitBox < 4)
        HitBoxMask = (1 << loadedHitBox);
    if (HitBoxMask == 0)
        HitBoxMask = 1;

    if (warningDelayMin > warningDelayMax)
    {
        const float t = warningDelayMin;
        warningDelayMin = warningDelayMax;
        warningDelayMax = t;
    }
    WarningSanitizeApproachSpeeds();
    SanitizeVischeckColors();
    warningConfigMode = std::clamp(warningConfigMode, 0, 1);
    warningPreset = std::clamp(warningPreset, 0, 4);
    warningDirectionDetail = std::clamp(warningDirectionDetail, 0, 1);
    warningMinConfidence = std::clamp(warningMinConfidence, 1, 4);
    multipleMinConfidence = std::clamp(multipleMinConfidence, 1, 4);
    approachingMinimumDuration = std::clamp(approachingMinimumDuration, 0.10f, 1.5f);
    approachingMaxDistance = std::clamp(approachingMaxDistance, 20.0f, 200.0f);
    approachingClearDebounce = std::clamp(approachingClearDebounce, 0.10f, 1.0f);
    if (warningPreset >= 0 && warningPreset <= 3 && !WarningPresetMatches(warningPreset))
        warningPreset = 4;

    g_cfgPendingMenuSync = true;
    g_cfgStatus = std::string("Loaded: ") + path;
}
