#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace OffsetHealth {

enum class OffsetGroup {
    Image,
    GObjects,
    UWorld,
    UViewPort,
    UGameInstance,
    ULocalPlayer,
    UPlayerController,
    USceneComponent,
    UObject,
    UPawn,
    UPlayerState,
    UInventoryComponent,
    Arc,
    Generic
};

enum class Severity {
    Info,
    Warning,
    Critical
};

struct Issue {
    OffsetGroup group;
    Severity severity;
    std::string field;
    std::string message;
};

struct Result {
    bool healthy = true;
    std::vector<Issue> issues;
};

// Run a full health check against the attached process. imageBase should be ctx->image_base.
Result RunHealthCheck(uint64_t imageBase);

inline bool IsHealthy(const Result& r) { return r.healthy; }

// Signature-based scans that survive minor UE5 patches when hardcoded offsets drift.
// These are GObject / name scans, not raw byte-pattern scans.
bool ScanForViewportClient(uint64_t& outViewport, uint32_t& outIndex);
bool ScanForLocalPlayerController(uint64_t& outPlayerController);

} // namespace OffsetHealth
