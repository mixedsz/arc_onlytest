#pragma once
#include <string>
#include <algorithm>
#include "menu/string_encryption.hpp"

enum class PlatformVersion {
	Steam,
	Xbox
};

// Diagnostic-only snapshot of the USkinnedMeshComponent double-buffered bone
// transform selection (ComponentSpaceTransformsArray[2] + selector). Populated
// by NameResolver::GetBoneArrayDebugInfo() using the *same* decrypt/selector
// math as the production GetBoneArrayPtr() path for each platform - it does
// not add a new offset, it just also reads the slot GetBoneArrayPtr() did NOT
// pick, for one-shot verification logging.
struct BoneArrayDebugInfo {
	uint32_t selectorField = 0; // raw field GetBoneArrayPtr() derives its slot index from
	uint32_t selectedSlot = 0;  // slot offset actually used by GetBoneArrayPtr() (0 or 0x10)
	uint64_t readArrayPtr = 0;  // pointer at the selected slot (what GetBoneArrayPtr() returns)
	uint64_t editArrayPtr = 0;  // pointer at the other slot, for comparison
	uint32_t numBones = 0;
	bool valid = false;
};

inline PlatformVersion g_PlatformVersion = PlatformVersion::Steam;

inline PlatformVersion DetectPlatformFromExePath(const std::wstring& exePath) {
	if (exePath.empty()) {
		return PlatformVersion::Steam;
	}

	std::wstring lower = exePath;
	std::transform(lower.begin(), lower.end(), lower.begin(),
		[](wchar_t c) { return (wchar_t)towlower(c); });

	if (lower.find(X(L"wingdk")) != std::wstring::npos) {
		return PlatformVersion::Xbox;
	}
	if (lower.find(X(L"win64")) != std::wstring::npos) {
		return PlatformVersion::Steam;
	}
	if (lower.find(X(L"xbox")) != std::wstring::npos) {
		return PlatformVersion::Xbox;
	}
	if (lower.find(X(L"steam")) != std::wstring::npos) {
		return PlatformVersion::Steam;
	}

	return PlatformVersion::Steam;
}
