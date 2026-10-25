#pragma once
#include <d3d9.h>
#include <vector>
#include <string>
#include <iostream>
#include <emmintrin.h>
#include <unordered_map>
#include <unordered_set>
#include <mutex>
#include "Offsets.hpp"
#include "dgb/Spoof.h"
#include "memory/interface/ctx.hpp"
#include "settings/Settings.hpp"
#include <TlHelp32.h>
#include <winternl.h>
#include <filesystem>
#include <ctime>
#include <iomanip>
#include <sstream>
#include "Unicorn.hpp"
#include "Platform.hpp"
#include "Steam.hpp"
#include "Xbox.hpp"

#ifndef DEG2RAD
#define DEG2RAD(x) ((x) * (static_cast<float>(M_PI) / 180.0f))
#endif

#ifndef RAD2DEG
#define RAD2DEG(x) ((x) * (180.0f / static_cast<float>(M_PI)))
#endif

#define M_PI 3.14159265358979323846264338327950288419716939937510


#ifndef HIWORD
#define HIWORD(x)   ((uint16_t)(((uint32_t)(x) >> 16) & 0xFFFF))
#endif

#ifndef BYTE2
#define BYTE2(x)    ((uint8_t)(((uint32_t)(x) >> 16) & 0xFF))
#endif

#ifndef BYTE1
#define BYTE1(x)    ((uint8_t)(((uint32_t)(x) >> 8) & 0xFF))
#endif



template <typename T>
__forceinline T __ROL__(T value, int count) {
	const unsigned int nbits = sizeof(T) * 8;
	count &= nbits - 1;
	return (value << count) | (value >> (nbits - count));
}

template <typename T>
__forceinline T __ROR__(T value, int count) {
	const unsigned int nbits = sizeof(T) * 8;
	count &= nbits - 1;
	return (value >> count) | (value << (nbits - count));
}



inline uint32_t ROL4(uint32_t val, int n) {
	return _rotl(val, n);
}

inline uint64_t ROL8(uint64_t val, int n) {
	return _rotl64(val, n);
}

#ifndef STEAM_ROL_DEFINED
#define STEAM_ROL_DEFINED
inline uint32_t __ROL4__(uint32_t value, unsigned int count) {
	return ROL4(value, count);
}

inline uint64_t __ROL8__(uint64_t value, unsigned int count) {
	return ROL8(value, count);
}
#endif

#define __ROL1__(v, c)  static_cast<uint8_t>(__ROL__(static_cast<uint8_t>(v), c))

static inline uint64_t ByteswapUint64(uint64_t value) {
	return ((value & 0xFF00000000000000ULL) >> 56) |
		((value & 0x00FF000000000000ULL) >> 40) |
		((value & 0x0000FF0000000000ULL) >> 24) |
		((value & 0x000000FF00000000ULL) >> 8) |
		((value & 0x00000000FF000000ULL) << 8) |
		((value & 0x0000000000FF0000ULL) << 24) |
		((value & 0x000000000000FF00ULL) << 40) |
		((value & 0x00000000000000FFULL) << 56);
}

#define ROL4 __ROL4__
#define ROL8 __ROL8__
#define _byteswap_uint64 ByteswapUint64

static std::mutex g_emulator_mutex;
static std::unordered_map<uint32_t, std::wstring> g_fname_cache;

inline bool IsValidFNameUtf8(const char* name) {
	if (!name || !*name) return false;
	for (const unsigned char* p = (const unsigned char*)name; *p; ++p) {
		if (*p == '?') return false;
		if (*p < 0x20 || *p == 0x7F) return false;
	}
	return true;
}

inline uintptr_t RequestGNames(uint64_t BaseAddress, uint32_t name_index) {
	if (!ctx || !BaseAddress) return 0;
	if (g_PlatformVersion == PlatformVersion::Xbox) {
		return Platform::Xbox::RequestGNames(BaseAddress, name_index);
	}
	return Platform::Steam::RequestGNames(BaseAddress, name_index);

}

inline bool GetNameFromIndex(uint64_t BaseAddress, uint32_t name_index, char* out_name, size_t max_len) {
	if (!ctx || !BaseAddress || !out_name || max_len == 0) return false;
	if (g_PlatformVersion == PlatformVersion::Xbox) {
		return Platform::Xbox::GetNameFromIndex(BaseAddress, name_index, out_name, max_len);
	}
	return Platform::Steam::GetNameFromIndex(BaseAddress, name_index, out_name, max_len);
}

inline std::wstring GetNameFromIndexW(uint64_t BaseAddress, uint32_t name_index) {
	char name_buffer[1024] = { 0 };
	if (!GetNameFromIndex(BaseAddress, name_index, name_buffer, sizeof(name_buffer)))
		return L"";

	std::wstring result;
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, name_buffer, -1, NULL, 0);
	if (size_needed > 0 && size_needed <= 1024) {
		result.resize(static_cast<size_t>(size_needed - 1));
		MultiByteToWideChar(CP_UTF8, 0, name_buffer, -1, &result[0], size_needed);
	}
	return result;
}

// Direct GWorld read. Returns 0 on Xbox, where no RVA has been derived for this build -
// that platform still goes through the viewport.
inline uint64_t GetGWorld(uint64_t GameBase) {
	if (!ctx || !GameBase) return 0;
	if (g_PlatformVersion == PlatformVersion::Xbox)
		return 0;
	return Platform::Steam::GetGWorld(GameBase);
}

inline int ProbeGWorldCandidates(uint64_t GameBase,
	Platform::Steam::GWorldCandidate* out, int maxOut) {
	if (!ctx || !GameBase) return 0;
	if (g_PlatformVersion == PlatformVersion::Xbox) return 0;
	return Platform::Steam::ProbeGWorldCandidates(GameBase, out, maxOut);
}

inline uint64_t ScanForGWorld(uint64_t GameBase, uint64_t& outRva) {
	outRva = 0;
	if (!ctx || !GameBase) return 0;
	if (g_PlatformVersion == PlatformVersion::Xbox)
		return 0;
	return Platform::Steam::ScanForGWorld(GameBase, outRva);
}

inline uint32_t GetGObjectsCount(uint64_t GameBase) {
	if (!ctx || !GameBase) {
		return 0;
	}
	if (g_PlatformVersion == PlatformVersion::Xbox) {
		return Platform::Xbox::GetGObjectsCount(GameBase);
	}
	return Platform::Steam::GetGObjectsCount(GameBase);
}

inline uint64_t GetGObjectsChunksPtr(uint64_t GameBase) {
	if (!ctx || !GameBase) return 0;
	if (g_PlatformVersion == PlatformVersion::Xbox) {
		return Platform::Xbox::GetGObjectsChunksPtr(GameBase);
	}
	return Platform::Steam::GetGObjectsChunksPtr(GameBase);
}

// Exposed for the health check only. Count and chunks both fail as a unit when
// the base pointer decrypt is wrong, so the diagnostic needs the intermediate
// to say which of the five stages actually broke.
inline uint64_t GetGObjectsBasePtr(uint64_t GameBase) {
	if (!ctx || !GameBase) return 0;
	if (g_PlatformVersion == PlatformVersion::Xbox) {
		return Platform::Xbox::DecryptGObjectsBasePtr(GameBase);
	}
	return Platform::Steam::DecryptGObjectsBasePtr(GameBase);
}

inline uint64_t GetObjectFromGObjects(uint64_t GameBase, uint64_t chunksPtr, int32_t objectIndex) {
	if (!chunksPtr || objectIndex < 0) {
		return 0;
	}
	if (g_PlatformVersion == PlatformVersion::Xbox) {
		return Platform::Xbox::GetObjectFromGObjects(GameBase, chunksPtr, objectIndex);
	}
	return Platform::Steam::GetObjectFromGObjects(GameBase, chunksPtr, objectIndex);
}

inline std::wstring DecryptFNameEntry(uint64_t GameBase, uint32_t comparison_index) {
	if (comparison_index == 0 || comparison_index == 0xFFFFFFFF) {
		return L"";
	}

	{
		std::lock_guard<std::mutex> lock(g_emulator_mutex);
		auto it = g_fname_cache.find(comparison_index);
		if (it != g_fname_cache.end()) {
			return it->second;
		}
	}

	char name_buffer[1024] = { 0 };
	bool valid_name = GetNameFromIndex(GameBase, comparison_index, name_buffer, sizeof(name_buffer))
		&& IsValidFNameUtf8(name_buffer);

	if (!valid_name) {
		return L"";
	}

	std::wstring result;
	int size_needed = MultiByteToWideChar(CP_UTF8, 0, name_buffer, -1, NULL, 0);
	if (size_needed > 0 && size_needed <= 1024) {
		result.resize(size_needed - 1);
		MultiByteToWideChar(CP_UTF8, 0, name_buffer, -1, &result[0], size_needed);
	}

	if (!result.empty()) {
		std::lock_guard<std::mutex> lock(g_emulator_mutex);
		g_fname_cache[comparison_index] = result;
	}

	return result;
}


namespace NameResolver
{

	inline void ClearFNameCaches() {
	}

	inline void InitializeFNameCache(uint64_t GameBase, uint32_t maxIndex = 0, uint32_t maxIndexLimit = 500000)
	{
	}

	inline void DecodeInPlace(uint16_t* s, int32_t maxLenChars) {
		if (g_PlatformVersion == PlatformVersion::Xbox) {
			Platform::Xbox::DecodeInPlace(s, maxLenChars);
			return;
		}
		Platform::Steam::DecodeInPlace(s, maxLenChars);
	}

	inline uint64_t ClassPrivate(uintptr_t GameBase, uint64_t object) {
		if (g_PlatformVersion == PlatformVersion::Xbox) {
			return Platform::Xbox::ClassPrivate(GameBase, object);
		}
		return Platform::Steam::ClassPrivate(GameBase, object);
	}


	// FField/FProperty reflection. Both platforms already implement these; they
	// simply had no dispatcher because nothing walked a property list yet.
	inline std::wstring GetFFieldName(uintptr_t GameBase, uint64_t ffield) {
		if (!ctx || !GameBase || !ffield) return L"";
		if (g_PlatformVersion == PlatformVersion::Xbox) {
			return Platform::Xbox::GetFFieldName(GameBase, ffield);
		}
		return Platform::Steam::GetFFieldName(GameBase, ffield);
	}

	inline int32_t GetFFieldOffset(uintptr_t ffield) {
		if (!ctx || !ffield) return 0;
		if (g_PlatformVersion == PlatformVersion::Xbox) {
			return Platform::Xbox::GetFFieldOffset(ffield);
		}
		return Platform::Steam::GetFFieldOffset(ffield);
	}

	inline uint64_t StructSuper(uint64_t structPtr) {
		if (!ctx || !structPtr) return 0;
		const uintptr_t off = (g_PlatformVersion == PlatformVersion::Xbox)
			? (uintptr_t)XBOX_USTRUCT_SUPERSTRUCT : (uintptr_t)STEAM_USTRUCT_SUPERSTRUCT;
		return ctx->read<uint64_t>(structPtr + off);
	}

	inline uint64_t StructChildProperties(uint64_t structPtr) {
		if (!ctx || !structPtr) return 0;
		const uintptr_t off = (g_PlatformVersion == PlatformVersion::Xbox)
			? (uintptr_t)XBOX_USTRUCT_CHILD_PROPERTIES : (uintptr_t)STEAM_USTRUCT_CHILD_PROPERTIES;
		return ctx->read<uint64_t>(structPtr + off);
	}

	inline uint64_t FieldNext(uint64_t ffield) {
		if (!ctx || !ffield) return 0;
		const uintptr_t off = (g_PlatformVersion == PlatformVersion::Xbox)
			? (uintptr_t)XBOX_FFIELD_NEXT : (uintptr_t)STEAM_FFIELD_NEXT;
		return ctx->read<uint64_t>(ffield + off);
	}

	inline uint32_t GetNamePrivate(uintptr_t GameBase, uint64_t object) {
		if (!ctx || !object || !GameBase) return 0;
		if (g_PlatformVersion == PlatformVersion::Xbox) {
			return Platform::Xbox::GetNamePrivate(GameBase, object);
		}
		return Platform::Steam::GetNamePrivate(GameBase, object);
	}

	inline std::wstring GetObjectName(uintptr_t GameBase, uint64_t object);

	// ClassPrivate yields a UClass*, not a name index. This used to cast its low
	// 32 bits to a comparison index, which cannot work: the class name belongs to
	// the class object, so it takes a second lookup on the pointer.
	inline std::wstring GetClassName(uintptr_t GameBase, uint64_t Object)
	{
		// Same bounds as is_valid_pointer_optimized, spelled out because SDK.hpp
		// includes this header rather than the other way round.
		const uint64_t classPtr = ClassPrivate(GameBase, Object);
		if (!classPtr || (classPtr & 0x7ULL) ||
			classPtr < 0x10000ULL || classPtr > 0x00007FFFFFFFFFFFULL)
			return L"";
		return GetObjectName(GameBase, classPtr);
	}
	inline std::wstring GetObjectName(uintptr_t GameBase, uint64_t object)
	{
		uint64_t name_private = GetNamePrivate(GameBase, object);
		if (!name_private) return L"";
		uint32_t name_index = static_cast<uint32_t>(name_private & 0xFFFFFFFF);

		std::wstring result = DecryptFNameEntry(GameBase, name_index);

		if (!result.empty() && result.length() < 1024) {
			for (wchar_t c : result) {
				if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || c == L'_')
					return result;
			}
		}
		return L"";
	}


	inline std::string GetObjectNameA(uintptr_t GameBase, uint64_t object)
	{
		if (!ctx || !(object)) return "";

		std::wstring wname = GetObjectName(GameBase, object);
		if (wname.empty()) return "";

		int size_needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), NULL, 0, NULL, NULL);
		if (size_needed <= 0) return "";

		std::string result(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), &result[0], size_needed, NULL, NULL);
		return result;
	}

	inline std::string SafeGetNameA(uintptr_t GameBase, uint64_t ptr, int max_retries = 3) {
		if (!ctx || !(ptr)) return "";

		for (int i = 0; i < max_retries; i++) {
			std::string name = GetObjectNameA(GameBase, ptr);

			if (!name.empty() && name.length() < 1024) {
				bool has_valid_char = false;
				for (char c : name) {
					if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') || c == '_') {
						has_valid_char = true;
						break;
					}
				}
				if (has_valid_char) {
					return name;
				}
			}
		}

		return "";
	}

	inline std::wstring SafeGetName(uintptr_t GameBase, uint64_t ptr, int max_retries = 3) {
		if (!ctx || !(ptr)) return L"";

		for (int i = 0; i < max_retries; i++) {
			std::wstring name = GetObjectName(GameBase, ptr);

			if (!name.empty() && name.length() < 1024) {
				bool has_valid_char = false;
				for (wchar_t c : name) {
					if ((c >= L'A' && c <= L'Z') || (c >= L'a' && c <= L'z') || c == L'_') {
						has_valid_char = true;
						break;
					}
				}
				if (has_valid_char) {
					return name;
				}
			}
		}

		return L"";
	}

	inline std::string GetClassPrivateNameA(uintptr_t GameBase, uint64_t object)
	{
		if (!ctx || !(object)) return "";

		std::wstring wname = GetClassName(GameBase, object);
		if (wname.empty()) return "";

		int size_needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), NULL, 0, NULL, NULL);
		if (size_needed <= 0) return "";

		std::string result(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), &result[0], size_needed, NULL, NULL);
		return result;
	}


	inline uint64_t GetBoneArrayPtr(uint64_t Mesh) {
		if (g_PlatformVersion == PlatformVersion::Xbox) {
			return Platform::Xbox::GetBoneArrayPtr(Mesh);
		}
		return Platform::Steam::GetBoneArrayPtr(Mesh);
	}

	// Both double-buffer slots, for callers that would rather score the two candidates
	// than trust the selector bits.
	inline bool GetBoneArrayPtrSlots(uint64_t Mesh,
		uint64_t& outPrimary, uint64_t& outAlt,
		uint32_t& outPrimaryBones, uint32_t& outAltBones)
	{
		if (g_PlatformVersion == PlatformVersion::Xbox) {
			return Platform::Xbox::GetBoneArrayPtrSlots(Mesh, outPrimary, outAlt, outPrimaryBones, outAltBones);
		}
		return Platform::Steam::GetBoneArrayPtrSlots(Mesh, outPrimary, outAlt, outPrimaryBones, outAltBones);
	}

	// Every LOD slot, for callers scoring across more than the selected pair.
	inline bool GetBoneArrayPtrSlotsAll(uint64_t Mesh,
		uint64_t* slotsOut, uint32_t* bonesOut, int maxSlots, int& outSelectedIndex)
	{
		if (g_PlatformVersion == PlatformVersion::Xbox) {
			return Platform::Xbox::GetBoneArrayPtrSlotsAll(Mesh, slotsOut, bonesOut, maxSlots, outSelectedIndex);
		}
		return Platform::Steam::GetBoneArrayPtrSlotsAll(Mesh, slotsOut, bonesOut, maxSlots, outSelectedIndex);
	}

	// Diagnostic-only; see BoneArrayDebugInfo (Platform.hpp) for what this does and does not verify.
	inline BoneArrayDebugInfo GetBoneArrayDebugInfo(uint64_t Mesh) {
		if (g_PlatformVersion == PlatformVersion::Xbox) {
			return Platform::Xbox::GetBoneArrayDebugInfo(Mesh);
		}
		return Platform::Steam::GetBoneArrayDebugInfo(Mesh);
	}
}

class FName
{
private:
	uint64_t m_object = 0;
	mutable std::wstring m_cached_name;
	mutable bool m_has_cached = false;

public:
	FName() = default;
	explicit FName(uint64_t obj) : m_object(obj) {}

	uint64_t GetObject() const { return m_object; }

	std::wstring GetName() const
	{
		if (!m_object) return L"";
		if (m_has_cached) return m_cached_name;

		m_cached_name = NameResolver::GetObjectName(ctx->image_base, m_object);
		m_has_cached = true;
		return m_cached_name;
	}

	std::string GetNameString() const
	{
		if (!m_object) return "";

		std::wstring wname = GetName();
		if (wname.empty()) return "";

		int size_needed = WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), NULL, 0, NULL, NULL);
		if (size_needed <= 0) return "";

		std::string result(size_needed, 0);
		WideCharToMultiByte(CP_UTF8, 0, wname.c_str(), (int)wname.size(), &result[0], size_needed, NULL, NULL);
		return result;
	}

	static std::wstring GetObjectName(uint64_t obj)
	{
		return NameResolver::GetObjectName(ctx->image_base, obj);
	}

	static std::string GetObjectNameA(uint64_t obj)
	{
		return NameResolver::GetObjectNameA(ctx->image_base, obj);
	}

	void ClearCache()
	{
		m_has_cached = false;
		m_cached_name.clear();
	}

	operator bool() const { return m_object != 0; }
	bool operator==(const FName& other) const { return m_object == other.m_object; }
	bool operator!=(const FName& other) const { return m_object != other.m_object; }
};
