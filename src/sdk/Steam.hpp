#pragma once
#include "Platform.hpp"
#include "Decryptions.hpp"
#include "Unicorn.hpp"
#include <Windows.h>
#include <intrin.h>
#include <iostream>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <atomic>
#include <string>
#include <vector>
#include <mutex>
#include <cstdio>
#include <emmintrin.h>
#include <tmmintrin.h>
#include <smmintrin.h>
#include <wmmintrin.h>

#ifndef BYTE2
#define BYTE2(x)    ((uint8_t)(((uint32_t)(x) >> 16) & 0xFF))
#endif

#ifndef HIWORD
#define HIWORD(x)   ((uint16_t)(((uint32_t)(x) >> 16) & 0xFFFF))
#endif

#ifndef STEAM_ROL_DEFINED
#define STEAM_ROL_DEFINED

inline uint16_t __ROL2__(uint16_t value, unsigned int count) {
	const unsigned int mask = 15;
	count &= mask;
	return (value << count) | (value >> ((16 - count) & mask));
}

inline uint32_t __ROL4__(uint32_t value, unsigned int count) {
	const unsigned int mask = 31;
	count &= mask;
	return (value << count) | (value >> ((32 - count) & mask));
}

inline uint64_t __ROL8__(uint64_t value, unsigned int count) {
	const unsigned int mask = 63;
	count &= mask;
	return (value << count) | (value >> ((64 - count) & mask));
}
#endif

#define STEAM_USTRUCT_SUPERSTRUCT         0xB0
#define STEAM_USTRUCT_CHILD_PROPERTIES    0x100
#define STEAM_USTRUCT_PROPERTIES_SIZE     0x110
#define STEAM_USTRUCT_CHILDREN            0xD8
#define STEAM_ELEMENT_SIZE                0x80

#define STEAM_FFIELD_NEXT                 0x98
#define STEAM_FFIELD_CLASS                0x90
#define STEAM_FFIELD_NAME                 0x70
#define STEAM_UFIELD_NEXT                 0x20
#define STEAM_UENUM_NAMES                 0xB0

#define STEAM_FPROPERTY_OFFSET            0xC4
#define STEAM_FPROPERTY_FLAGS             0xB8
#define STEAM_FPROPERTY_ARRAY_DIM         0xF0
#define STEAM_FPROPERTY_ELEMENT_SIZE      0xF8
#define STEAM_FPROPERTY_STRUCT            0x128
#define STEAM_FPROPERTY_INNER             0x110
#define STEAM_FPROPERTY_VALUE             0x128

#define STEAM_FFIELD_NAME_XOR_KEY_RVA     0xB4E02B0
#define STEAM_FFIELD_NAME_SHUFLO_IMM      57
#define STEAM_FFIELD_NAME_ROT32           9
#define STEAM_FFIELD_NAME_XOR_IMM         0x890EF320D7E2DC4CuLL
#define STEAM_FPROPERTY_OFFSET_XOR_CONST  0xEE0CA1CBu

#define STEAM_UFUNCTION_FLAGS             0x158
#define STEAM_UFUNCTION_NUMPARMS          0x15C
#define STEAM_UFUNCTION_PARMSSIZE         0x15E
#define STEAM_UFUNCTION_RETURNOFFSET      0x160
#define STEAM_UFUNCTION_FUNC              0x150

#define STEAM_UOBJECT_SLOT_STRIDE            32
#define STEAM_UOBJECT_SLOT_BASE_OFFSET       32
#define STEAM_UOBJECT_FNV32_PRIME            16777619u
#define STEAM_UOBJECT_FNV32_K                0xD4C2DB3Au
#define STEAM_UOBJECT_SLOT_ROL_A             0x19
#define STEAM_UOBJECT_SLOT_ROL_B             0x0E
#define STEAM_UOBJECT_SLOT_ROL_C             0x19
#define STEAM_UOBJECT_SLOT_ROL_D             0x0E
// Slot index is (base + adj) & 3, NOT base ^ adj - see UObjectSlotFor_Steam.
// Adding 0 or 2 to a 2-bit value is identical to XOR-ing it, so the name and
// class constants read the same under either rule and never revealed which one
// applies. Outer is the only one that separates them, and only when base is 0
// or 2; a probe over bases 1 and 3 fits both.
#define STEAM_UOBJECT_SLOT_NAME_XOR          2u
#define STEAM_UOBJECT_SLOT_CLASS_ADJ         0u
#define STEAM_UOBJECT_SLOT_OUTER_ADJ         1u
#define STEAM_UOBJECT_SLOT_CLMUL_K1          0x0B6641A64F1B214DULL
#define STEAM_UOBJECT_SLOT_CLMUL_K2          0x8FA21A13D9179A47ULL

// Direct GWorld pointer. CL-1372005: double-deref [RVA]→intermediary→[+0]=UWorld.
#define STEAM_GWORLD_RVA                     0x10967B98  /* CL-1372005 live-verified; was 0xE782D78 */

#define STEAM_GOBJECTS_BASE_ENC_RVA          0xE616340
#define STEAM_GOBJECTS_BASE_XOR_KEY_RVA      0xB48A020
#define STEAM_GOBJECTS_BASE_ROT64_AMOUNT     0x26
#define STEAM_GOBJECTS_BASE_SHUFLO_IMM       57
#define STEAM_GOBJECTS_COUNT_OFFSET          0x70
#define STEAM_GOBJECTS_COUNT_XOR_KEY_RVA     0xB4DD440
#define STEAM_GOBJECTS_COUNT_ROT64_AMOUNT    0x0D
#define STEAM_GOBJECTS_COUNT_SHUFLO_IMM      0xE3
#define STEAM_GOBJECTS_VTABLE_OFFSET         0xA0
#define STEAM_GOBJECTS_CTX_OFFSET            0xD0
#define STEAM_GOBJECTS_VTABLE_FN_INDEX       0x18
#define STEAM_GOBJECTS_ITEM_SIZE             20
#define STEAM_GOBJECTS_ITEM_FLAGS_OFFSET     8
#define STEAM_GOBJECTS_ITEM_SERIAL_OFFSET    16
#define STEAM_GOBJECTS_INVALID_FLAGS_MASK    0x30200000u

// FName CL-1341255 / v20260818. Slot decrypt is PCLMULQDQ; pool/keystream RVAs moved.
#define STEAM_FNAME_POOL_RVA                 0xE35AB00
#define STEAM_FNAME_XOR_TABLE_RVA            0xE2997F4
#define STEAM_FNAME_KEYSTREAM_COUNT          144
#define STEAM_FNAME_KEYSTREAM_BASE           80
#define STEAM_FNAME_HASH_PRIME32             0x01000193u
#define STEAM_FNAME_SHARD_HASH_ADD           0x30091BB7u
#define STEAM_FNAME_SHARD_HASH_SEED_OFF      0x6FD0ULL
#define STEAM_FNAME_SHARD_BLOCK_BASE_OFF     0x6FE0ULL
#define STEAM_FNAME_SHARD_BLOCK_STRIDE       32ULL
#define STEAM_FNAME_SHARD_ROL_A              0x17
#define STEAM_FNAME_SHARD_ROL_B              0x15
#define STEAM_FNAME_SHARD_ROL_C              0x17
#define STEAM_FNAME_SHARD_SHR_D              0x0B
#define STEAM_FNAME_BLOCK_ROL64              4
#define STEAM_FNAME_BLOCK_ROL32              2
#define STEAM_FNAME_BLOCK_XOR                0xF31D220392B6800BULL
#define STEAM_FNAME_FNV64_PRIME              0x100000001B3ULL
#define STEAM_FNAME_FNV64_ADD                0x6463CD794F959557ULL
#define STEAM_FNAME_FNV64_ROL1               0x30
#define STEAM_FNAME_FNV64_ROL2               0x2E
#define STEAM_FNAME_WIDE_FLAG                0x8000u
#define STEAM_FNAME_LEN_MASK                 0x03FFu
#define STEAM_FNAME_STRING_BIAS              0xD917u
#define STEAM_FNAME_KEY_ADVANCE              1u

// Bone array CL-1341255: ROL64(50) + PSHUFB + ROL32(22), no XOR.
#define STEAM_BONEARRAY_ENC_OFFSET           0x7B0
#define STEAM_BONEARRAY_LOD_OFFSET           0x7F8
#define STEAM_BONEARRAY_LOD_SHIFT            27
#define STEAM_BONEARRAY_LOD_MASK             0xFFFFFFF0u
#define STEAM_BONEARRAY_DESCRIPTOR_OFFSET    0x48
#define STEAM_BONEARRAY_TOTALBONES_OFFSET    0xB00
#define STEAM_BONEARRAY_PTR_ROL64            50
#define STEAM_BONEARRAY_PTR_ROL32            22

#define STEAM_MESH_LAST_RENDER_TIME          0x4C0  /* CL-1372005 plaintext float */
#define STEAM_MESH_LAST_RENDER_TIME_ONSCREEN 0x4C4  /* CL-1372005 plaintext float */
#define STEAM_MESH_LAST_RENDER_TIME_KEY      0u  /* XOR retired CL-1372005 */
#define STEAM_MESH_LAST_RENDER_ONSCREEN_KEY  0u  /* XOR retired CL-1372005 */
#define STEAM_MESH_WORLD_PRIVATE             0x140  /* CL-1372005 */
#define STEAM_WORLD_TIME_SECONDS             0x240  /* CL-1372005 UWorld::TimeSeconds */
// Occlusion epsilon for LRTS vs worldTime. 0.5s was so loose that players
// behind cover stayed "visible" for half a second after last on-screen draw.
#define STEAM_VISIBLE_EPSILON                0.12f

#define STEAM_PLAYERNAME_FNV_PRIME           16777619u
#define STEAM_PLAYERNAME_FNV_SEED            0xD351FEECu
#define STEAM_PLAYERNAME_FNV_ROL             28

std::wstring GetNameFromIndexW(uint64_t BaseAddress, uint32_t name_index);
uint64_t EmulateVTableCallWithTLS(uint64_t GameBase, uint64_t funcPtr, uint64_t rcx_param, uint64_t rdx_param);

namespace Platform {
	namespace Steam {

		// CL-1341255 / v818: CI decode is identity. Only the 144-entry keystream is loaded
		// from the image; SIMD seed masks are gone.
		struct FNameMasks {
			uint16_t keyTable[256]{};
			bool ready = false;
		};
		inline FNameMasks g_fnameMasks;

		inline uint16_t FNameKeystreamSlot(uint32_t k) {
			const int idx = (int)((k & 0x3Fu) + STEAM_FNAME_KEYSTREAM_BASE);
			if (idx < 0 || idx >= 256)
				return 0;
			return g_fnameMasks.keyTable[idx];
		}

		inline bool FNameInit(uint64_t BaseAddress) {
			if (g_fnameMasks.ready)
				return true;
			if (!ctx || !BaseAddress)
				return false;

			uint16_t table[STEAM_FNAME_KEYSTREAM_COUNT]{};
			if (!ctx->read_array(BaseAddress + STEAM_FNAME_XOR_TABLE_RVA, table, sizeof(table)))
				return false;
			int nz = 0;
			for (uint16_t k : table) {
				if (k != 0) ++nz;
			}
			if (nz < 8)
				return false;

			FNameMasks next;
			memcpy(next.keyTable, table, sizeof(table));
			next.ready = true;
			g_fnameMasks = next;
			return true;
		}

		inline uint64_t FNameDecryptBlock(uint64_t raw) {
			uint64_t x = __ROL8__(raw, STEAM_FNAME_BLOCK_ROL64) ^ STEAM_FNAME_BLOCK_XOR;
			const uint32_t d0 = __ROL4__(static_cast<uint32_t>(x), STEAM_FNAME_BLOCK_ROL32);
			const uint32_t d1 = __ROL4__(static_cast<uint32_t>(x >> 32), STEAM_FNAME_BLOCK_ROL32);
			return static_cast<uint64_t>(d0) | (static_cast<uint64_t>(d1) << 32);
		}

		inline uint64_t FNameClmulLo(uint64_t a, uint64_t b) {
			const __m128i va = _mm_set_epi64x(0, static_cast<long long>(a));
			const __m128i vb = _mm_set_epi64x(0, static_cast<long long>(b));
			return static_cast<uint64_t>(_mm_cvtsi128_si64(_mm_clmulepi64_si128(va, vb, 0x00)));
		}

		// Plaintext slot value, before the halves are swapped.
		//
		// The trailing ROL64(32) is presentation, not decryption: it moves the
		// FName comparison index into the low half so name slots can take it
		// with a truncating cast. Pointer slots want the value as-is, and
		// applying the swap to one is what produced 0x91B8AF0000000000 out of
		// the perfectly good 0x91B8AF00.
		inline uint64_t FNameDecryptSlot16Raw(uint64_t lo, uint64_t hi) {
			const uint64_t t = hi ^ FNameClmulLo(STEAM_UOBJECT_SLOT_CLMUL_K1, lo);
			return FNameClmulLo(STEAM_UOBJECT_SLOT_CLMUL_K2, t) ^ lo;
		}

		inline uint64_t FNameDecryptSlot16(uint64_t lo, uint64_t hi) {
			return __ROL8__(FNameDecryptSlot16Raw(lo, hi), 32);
		}

		inline uintptr_t RequestGNames(uint64_t BaseAddress, uint32_t name_index) {
			if (!ctx || !BaseAddress || name_index == 0) return 0;
			if (!FNameInit(BaseAddress)) return 0;

			const uint32_t nameOffset = name_index & 0xFFFFu;
			const uint64_t chunk = BaseAddress + STEAM_FNAME_POOL_RVA +
				((static_cast<uint64_t>(name_index) >> 8) & 0xFFFF00ULL);

			const uint64_t hashAddress = chunk + STEAM_FNAME_SHARD_HASH_SEED_OFF;
			const uint32_t lo = static_cast<uint32_t>(hashAddress);
			const uint32_t hi = static_cast<uint32_t>(hashAddress >> 32);
			const uint32_t P = STEAM_FNAME_HASH_PRIME32;
			const uint32_t A = STEAM_FNAME_SHARD_HASH_ADD;

			uint32_t h = __ROL4__(lo, STEAM_FNAME_SHARD_ROL_A) * P + A;
			h = __ROL4__(h, STEAM_FNAME_SHARD_ROL_B) * P + hi + A;
			h = __ROL4__(h, STEAM_FNAME_SHARD_ROL_C) * P + A;
			h = (h >> STEAM_FNAME_SHARD_SHR_D) * P + A;
			const uint32_t mix = h ^ (h >> 16);

			const uint64_t blockBase = chunk + STEAM_FNAME_SHARD_BLOCK_BASE_OFF;
			const uint64_t raw1 = ctx->read<uint64_t>(
				blockBase + STEAM_FNAME_SHARD_BLOCK_STRIDE * (mix & 7u));
			const uint64_t raw2 = ctx->read<uint64_t>(
				blockBase + STEAM_FNAME_SHARD_BLOCK_STRIDE * ((mix + 1u) & 7u));
			if (!raw1 && !raw2)
				return 0;

			const uint64_t v13 = FNameDecryptBlock(raw1);
			const uint64_t v15 = FNameDecryptBlock(raw2);
			uint64_t fnv =
				STEAM_FNAME_FNV64_PRIME * __ROL8__(v13, STEAM_FNAME_FNV64_ROL1) + STEAM_FNAME_FNV64_ADD;
			fnv = STEAM_FNAME_FNV64_PRIME * __ROL8__(fnv, STEAM_FNAME_FNV64_ROL2) + STEAM_FNAME_FNV64_ADD;

			const uint64_t entry = v13 + (v15 ^ fnv) + 2ULL * static_cast<uint64_t>(nameOffset);
			if (!validpointer((uintptr_t)entry))
				return 0;
			return (uintptr_t)entry;
		}

		inline bool GetNameFromIndex(uint64_t BaseAddress, uint32_t name_index, char* out_name, size_t max_len) {
			if (!ctx || !BaseAddress || !out_name || max_len == 0) return false;

			uintptr_t name_entry_ptr = Platform::Steam::RequestGNames(BaseAddress, name_index);
			if (!name_entry_ptr || !validpointer(name_entry_ptr))
				return false;

			uint16_t header = ctx->read<uint16_t>(name_entry_ptr);
			if (header == 0)
				return false;

			const bool is_wide = (header & STEAM_FNAME_WIDE_FLAG) != 0;
			const int str_len = (int)(header & STEAM_FNAME_LEN_MASK);
			if (str_len <= 0 || str_len > 1023 || (size_t)str_len >= max_len)
				return false;

			uint32_t state = static_cast<uint32_t>(str_len) + STEAM_FNAME_STRING_BIAS;

			if (is_wide) {
				uint16_t wide_name[1024]{};
				if (!ctx->read_array(name_entry_ptr + 2, wide_name, 2u * (uint32_t)str_len))
					return false;

				for (int i = 0; i < str_len; ++i)
					wide_name[i] ^= FNameKeystreamSlot(state + (uint32_t)i * STEAM_FNAME_KEY_ADVANCE);

				wide_name[str_len] = 0;
				int converted = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)wide_name, str_len,
					out_name, (int)(max_len - 1), NULL, NULL);
				if (converted <= 0) return false;
				out_name[converted] = '\0';
			}
			else {
				uint8_t encrypted_data[1024]{};
				if (!ctx->read_array(name_entry_ptr + 2, encrypted_data, (uint32_t)str_len))
					return false;

				for (int i = 0; i < str_len; ++i) {
					encrypted_data[i] ^= (uint8_t)(FNameKeystreamSlot(
						state + (uint32_t)i * STEAM_FNAME_KEY_ADVANCE) >> 3);
				}

				memcpy(out_name, encrypted_data, (size_t)str_len);
				out_name[str_len] = '\0';
			}

			return out_name[0] != '\0';
		}

		inline uint32_t UObjectComputeHash_Steam(uintptr_t a1) {
			uint64_t addr = (uint64_t)(a1 + 16);
			uint32_t lo = (uint32_t)addr;
			uint32_t hi = (uint32_t)(addr >> 32);
			const uint32_t P = STEAM_FNAME_HASH_PRIME32;
			const uint32_t K = STEAM_UOBJECT_FNV32_K;

			uint32_t h = __ROL4__(lo, STEAM_UOBJECT_SLOT_ROL_A) * P + K;
			h = __ROL4__(h, STEAM_UOBJECT_SLOT_ROL_B) * P + hi + K;
			h = __ROL4__(h, STEAM_UOBJECT_SLOT_ROL_C) * P + K;
			h = __ROL4__(h, STEAM_UOBJECT_SLOT_ROL_D) * P + K;
			return h ^ (h >> 16);
		}

		inline uint32_t UObjectSlotIndex_Steam(uintptr_t a1) {
			return UObjectComputeHash_Steam(a1) & 3u;
		}

		// Slots rotate around the base index rather than XOR-ing against it.
		// Confirmed over six live probes covering bases 0, 1 and 3: class at
		// +0, name at +2, outer at +1, and +3 always reads sixteen zero bytes.
		inline uint32_t UObjectSlotFor_Steam(uint64_t object, uint32_t adj) {
			return (UObjectComputeHash_Steam((uintptr_t)object) + adj) & 3u;
		}

		inline bool UObjectReadSlot16_Steam(uint64_t object, uint32_t slot,
			uint64_t& lo, uint64_t& hi) {
			alignas(16) uint8_t raw[16]{};
			if (!ctx->read_array(
					object + STEAM_UOBJECT_SLOT_BASE_OFFSET +
						(uint64_t)STEAM_UOBJECT_SLOT_STRIDE * slot,
					raw, sizeof(raw)))
				return false;
			memcpy(&lo, raw, 8);
			memcpy(&hi, raw + 8, 8);
			return lo || hi;
		}

		// Name slots: comparison index lands in the low 32 bits.
		inline uint64_t UObjectDecryptSlot16_Steam(uint64_t object, uint32_t slot) {
			uint64_t lo = 0, hi = 0;
			if (!UObjectReadSlot16_Steam(object, slot, lo, hi))
				return 0;
			return FNameDecryptSlot16(lo, hi);
		}

		// Class and outer slots: the plaintext is already the pointer.
		inline uint64_t UObjectDecryptSlotPtr_Steam(uint64_t object, uint32_t slot) {
			uint64_t lo = 0, hi = 0;
			if (!UObjectReadSlot16_Steam(object, slot, lo, hi))
				return 0;
			return FNameDecryptSlot16Raw(lo, hi);
		}

		inline uintptr_t ClassPrivate(uintptr_t GameBase, uint64_t object) {
			(void)GameBase;
			if (!ctx || !object) return 0;
			return (uintptr_t)UObjectDecryptSlotPtr_Steam(
				object, UObjectSlotFor_Steam(object, STEAM_UOBJECT_SLOT_CLASS_ADJ));
		}

		inline uint64_t OuterPrivate(uintptr_t GameBase, uint64_t object) {
			(void)GameBase;
			if (!ctx || !object) return 0;
			return UObjectDecryptSlotPtr_Steam(
				object, UObjectSlotFor_Steam(object, STEAM_UOBJECT_SLOT_OUTER_ADJ));
		}

		inline uint32_t GetNamePrivate(uintptr_t GameBase, uint64_t object) {
			if (!ctx || !GameBase || !object) return 0;
			if (!FNameInit(GameBase)) return 0;
			return (uint32_t)UObjectDecryptSlot16_Steam(
				object, UObjectSlotFor_Steam(object, STEAM_UOBJECT_SLOT_NAME_XOR));
		}

		inline int32_t GetFFieldOffset(uintptr_t ffield_ptr) {
			if (!ctx || !ffield_ptr) return 0;
			uint32_t encrypted = ctx->read<uint32_t>(ffield_ptr + STEAM_FPROPERTY_OFFSET);
			if (encrypted == 0) return 0;
			return static_cast<int32_t>(_byteswap_ulong(encrypted ^ STEAM_FPROPERTY_OFFSET_XOR_CONST));
		}

		inline std::wstring GetFFieldName(uintptr_t GameBase, uint64_t ffield_ptr) {
			if (!ctx || !GameBase || !ffield_ptr) return L"";

			__m128i raw = ctx->read<__m128i>(ffield_ptr + STEAM_FFIELD_NAME);
			__m128i key = _mm_cvtsi64_si128(
				(long long)ctx->read<uint64_t>(GameBase + STEAM_FFIELD_NAME_XOR_KEY_RVA));
			__m128i x = _mm_xor_si128(
				_mm_shufflelo_epi16(raw, STEAM_FFIELD_NAME_SHUFLO_IMM), key);
			__m128i rot = _mm_or_si128(
				_mm_slli_epi32(x, STEAM_FFIELD_NAME_ROT32),
				_mm_srli_epi32(x, 32 - STEAM_FFIELD_NAME_ROT32));
			uint64_t dec = __ROL8__(rot.m128i_u64[0] ^ STEAM_FFIELD_NAME_XOR_IMM, 32);
			uint32_t name_index = (uint32_t)dec;

			if (name_index == 0 || name_index == 0xFFFFFFFF) return L"";

			return GetNameFromIndexW(GameBase, name_index);
		}

		// A bare range check is not enough to call something UWorld. The configured RVA
		// returned 0x4DF03F40, which passed that test and was wrong: PersistentLevel read
		// back as 0x0, LevelCollections as 0x202000000000098, and the address sat at ~1.3GB
		// while every other live object in this process is at 14-77GB. Everything
		// downstream then failed in ways that looked like bad field offsets rather than a
		// bad base, which is the expensive kind of wrong.
		//
		// A real UWorld has a non-null PersistentLevel and a Levels TArray with a plausible
		// count. Both are cheap and neither can be satisfied by an unrelated allocation.
		// Strict pointer test. validpointer() in ctx.hpp intentionally omits the alignment
		// check because FName chunk and string pointers are legitimately unaligned, and
		// is_valid_pointer_optimized() lives in SDK.hpp which is not visible from here. The
		// alignment term is the part that matters for this job: the bad candidate that got
		// through last time, 0x79006C006E, is UTF-16 text and is not 8-byte aligned.
		inline bool GWorldPtrSane(uint64_t p) {
			if (!p) return false;
			if (p & 0x7ULL) return false;
			return p >= 0x10000ULL && p <= 0x00007FFFFFFFFFFFULL;
		}

		// Module bounds, so candidates living inside the image can be rejected outright.
		// A UWorld is a heap allocation; anything at an address the loader mapped is data,
		// not an object. This matters because the back-reference test alone can be closed
		// by accident inside the image: the sweep matched 0x14C0E6240 whose PersistentLevel
		// was 0x14C0E6390, and 0x14C0E6390 + 0x128 is 0x14C0E64B8 - the very address the
		// sweep had just read the candidate from. A pointer table that references itself
		// forms a valid-looking cycle without being an object at all.
		inline bool GetModuleBounds(uint64_t GameBase, uint64_t& outEnd) {
			outEnd = 0;
			if (!ctx || !GameBase) return false;
			const uint32_t e_lfanew = ctx->read<uint32_t>(GameBase + 0x3C);
			if (e_lfanew < 0x40 || e_lfanew > 0x1000) return false;
			const uint32_t sizeOfImage = ctx->read<uint32_t>(GameBase + e_lfanew + 0x50);
			if (sizeOfImage < 0x1000 || sizeOfImage > 0x20000000u) return false;
			outEnd = GameBase + sizeOfImage;
			return true;
		}
		inline uint64_t g_moduleEnd = 0;

		inline bool GWorldOffImage(uint64_t p) {
			if (!GWorldPtrSane(p))
				return false;
			if (ctx->image_base && g_moduleEnd &&
				p >= ctx->image_base && p < g_moduleEnd)
				return false;
			return true;
		}

		inline bool GWorldHasModuleVtable(uint64_t obj) {
			if (!ctx || !obj)
				return false;
			if (!g_moduleEnd && ctx->image_base)
				GetModuleBounds(ctx->image_base, g_moduleEnd);
			const uint64_t vt = ctx->read<uint64_t>(obj);
			if (!GWorldPtrSane(vt) || !ctx->image_base || !g_moduleEnd)
				return false;
			return vt >= ctx->image_base && vt < g_moduleEnd;
		}

		// Set when GWorldLooksReal accepts a candidate via an OwningWorld back-reference.
		inline uint32_t g_gworldFoundPL = 0;
		inline uint32_t g_gworldFoundOW = 0;

		inline bool GWorldLooksReal(uint64_t candidate) {
			if (!GWorldOffImage(candidate) || (candidate & 0xF))
				return false;
			if (!GWorldHasModuleVtable(candidate))
				return false;

			const uint32_t plConfigured = (uint32_t)Offsets::Get().UWorld.PersistentLevel;
			const uint32_t owConfigured = (uint32_t)Offsets::Get().ULevel.OwningWorld;
			const uint32_t plList[] = { plConfigured, 0x110u, 0xF0u, 0x158u, 0x128u, 0x140u, 0xD8u };
			const uint32_t owList[] = { owConfigured, 0x130u, 0x128u, 0x120u, 0x118u };

			for (uint32_t pl : plList) {
				const uint64_t level = ctx->read<uint64_t>(candidate + pl);
				if (!GWorldOffImage(level) || (level & 0xF))
					continue;
				for (uint32_t ow : owList) {
					if (ctx->read<uint64_t>(level + ow) != candidate)
						continue;
					g_gworldFoundPL = pl;
					g_gworldFoundOW = ow;
					return true;
				}
			}
			return false;
		}

		inline uint64_t g_gworldRvaOverride = 0;
		inline uint64_t g_gworldIndirectOffset = (uint64_t)-1;

		inline uint64_t GWorldTryDecryptSlot(uint64_t addr) {
			alignas(16) uint8_t raw[16]{};
			if (!ctx->read_array(addr, raw, sizeof(raw)))
				return 0;
			uint64_t lo = 0, hi = 0;
			memcpy(&lo, raw, 8);
			memcpy(&hi, raw + 8, 8);
			if (!lo && !hi)
				return 0;
			return FNameDecryptSlot16(lo, hi);
		}

		// UWorld::OwningGameInstance is encrypted (raw read is 0). Dump: pshufb mask
		// 0xB09C350, xor keys 0xB06C380 / 0xB06C390. Also try the v818 PCLMUL slot
		// used for other UObject pointers, in case this field moved onto that pipeline.
		inline uint64_t DecryptOwningGameInstance(uint64_t uworld, uint32_t offset) {
			if (!ctx || !uworld || !offset)
				return 0;
			alignas(16) uint8_t raw[16]{};
			if (!ctx->read_array(uworld + offset, raw, sizeof(raw)))
				return 0;
			uint64_t lo = 0, hi = 0;
			memcpy(&lo, raw, 8);
			memcpy(&hi, raw + 8, 8);
			if (!lo && !hi)
				return 0;

			const uint64_t base = ctx->image_base;
			if (base) {
				const __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i*>(raw));
				const __m128i mask = ctx->read<__m128i>(base + 0xB09C350ull);
				const __m128i k1 = ctx->read<__m128i>(base + 0xB06C380ull);
				const __m128i k2 = ctx->read<__m128i>(base + 0xB06C390ull);
				__m128i d = _mm_xor_si128(_mm_shuffle_epi8(v, mask), k1);
				d = _mm_xor_si128(d, k2);
				const uint64_t shuffled = static_cast<uint64_t>(_mm_cvtsi128_si64(d));
				if (GWorldPtrSane(shuffled))
					return shuffled;
				d = _mm_xor_si128(_mm_xor_si128(v, k1), k2);
				const uint64_t xored = static_cast<uint64_t>(_mm_cvtsi128_si64(d));
				if (GWorldPtrSane(xored))
					return xored;
			}

			const uint64_t slot = FNameDecryptSlot16(lo, hi);
			if (GWorldPtrSane(slot))
				return slot;
			return 0;
		}

		inline uint64_t GWorldTryArrayElements(uint64_t data, int32_t num, int stride) {
			if (!GWorldPtrSane(data) || num <= 0 || num > 64 || stride < 8)
				return 0;
			for (int32_t i = 0; i < num; ++i) {
				const uint64_t elem = data + (uint64_t)i * (uint64_t)stride;
				for (uint64_t off = 0; off <= 16; off += 8) {
					const uint64_t p = ctx->read<uint64_t>(elem + off);
					if (GWorldLooksReal(p))
						return p;
				}
				const uint64_t dec = GWorldTryDecryptSlot(elem);
				if (GWorldLooksReal(dec))
					return dec;
				if (GWorldPtrSane(dec)) {
					const uint64_t inner = ctx->read<uint64_t>(dec);
					if (GWorldLooksReal(inner))
						return inner;
				}
			}
			return 0;
		}

		inline bool GWorldReadTArray(uint64_t addr, uint64_t& data, int32_t& num, int32_t& maxv) {
			struct { uint64_t data; int32_t num; int32_t max; } a{};
			if (!ctx->read_array(addr, &a, sizeof(a)))
				return false;
			if (!GWorldPtrSane(a.data))
				return false;
			if (a.num < 0 || a.num > 64 || a.max < a.num || a.max > 4096)
				return false;
			data = a.data;
			num = a.num;
			maxv = a.max;
			return true;
		}

		inline uint64_t GWorldResolveFromPointer(uint64_t p) {
			if (GWorldLooksReal(p))
				return p;
			if (!GWorldPtrSane(p))
				return 0;

			for (uint64_t probe = 0; probe <= 0x80; probe += 8) {
				const uint64_t inner = ctx->read<uint64_t>(p + probe);
				if (GWorldLooksReal(inner)) {
					g_gworldIndirectOffset = probe;
					return inner;
				}
			}

			uint64_t data = 0;
			int32_t num = 0, maxv = 0;
			if (GWorldReadTArray(p, data, num, maxv)) {
				const uint64_t hit16 = GWorldTryArrayElements(data, num, 16);
				if (hit16)
					return hit16;
				const uint64_t hit24 = GWorldTryArrayElements(data, num, 24);
				if (hit24)
					return hit24;
			}
			return 0;
		}

		inline uint64_t GetGWorld(uint64_t GameBase) {
			if (!ctx || !GameBase) return 0;
			if (!g_moduleEnd)
				GetModuleBounds(GameBase, g_moduleEnd);

			if (g_gworldRvaOverride) {
				const uint64_t cached = ctx->read<uint64_t>(GameBase + g_gworldRvaOverride);
				const uint64_t hit = GWorldResolveFromPointer(cached);
				if (hit)
					return hit;
				g_gworldRvaOverride = 0;
			}

			const uint64_t slot = GameBase + STEAM_GWORLD_RVA;
			const uint64_t raw = ctx->read<uint64_t>(slot);

			if (g_gworldIndirectOffset != (uint64_t)-1 && GWorldPtrSane(raw)) {
				const uint64_t known = ctx->read<uint64_t>(raw + g_gworldIndirectOffset);
				const uint64_t hit = GWorldResolveFromPointer(known);
				if (hit)
					return hit;
			}

			if (uint64_t hit = GWorldResolveFromPointer(raw))
				return hit;

			uint64_t data = 0;
			int32_t num = 0, maxv = 0;
			if (GWorldReadTArray(slot, data, num, maxv)) {
				if (uint64_t hit = GWorldTryArrayElements(data, num, 16))
					return hit;
				if (uint64_t hit = GWorldTryArrayElements(data, num, 24))
					return hit;
			}

			const uint64_t dec = GWorldTryDecryptSlot(slot);
			if (uint64_t hit = GWorldResolveFromPointer(dec))
				return hit;

			static const uint64_t kXor = 0xA738DD8241D227C2ULL;
			if (uint64_t hit = GWorldResolveFromPointer(raw ^ kXor))
				return hit;

			return 0;
		}

		inline int DumpGWorldRva(uint64_t GameBase, char* out, size_t outSz) {
			if (!out || outSz < 8) return 0;
			out[0] = 0;
			if (!ctx || !GameBase) return 0;
			if (!g_moduleEnd)
				GetModuleBounds(GameBase, g_moduleEnd);

			const uint64_t slot = GameBase + STEAM_GWORLD_RVA;
			const uint64_t raw = ctx->read<uint64_t>(slot);
			const int inImg = (g_moduleEnd && raw >= GameBase && raw < g_moduleEnd) ? 1 : 0;
			uint64_t vt = 0;
			if (GWorldPtrSane(raw))
				vt = ctx->read<uint64_t>(raw);

			uint64_t data = 0;
			int32_t num = 0, maxv = 0;
			const int tarr = GWorldReadTArray(slot, data, num, maxv) ? 1 : 0;
			const uint64_t dec = GWorldTryDecryptSlot(slot);
			const uint64_t xorv = raw ^ 0xA738DD8241D227C2ULL;

			uint64_t p0 = GWorldPtrSane(raw) ? ctx->read<uint64_t>(raw) : 0;
			uint64_t p8 = GWorldPtrSane(raw) ? ctx->read<uint64_t>(raw + 8) : 0;

			return snprintf(out, outSz,
				"rva=0x%X raw=0x%llX sane=%d inImg=%d vt=0x%llX +0=0x%llX +8=0x%llX tarr=%d data=0x%llX n=%d max=%d slotDec=0x%llX xor=0x%llX realRaw=%d realDec=%d realXor=%d",
				(unsigned)STEAM_GWORLD_RVA,
				(unsigned long long)raw,
				GWorldPtrSane(raw) ? 1 : 0,
				inImg,
				(unsigned long long)vt,
				(unsigned long long)p0,
				(unsigned long long)p8,
				tarr,
				(unsigned long long)data,
				(int)num,
				(int)maxv,
				(unsigned long long)dec,
				(unsigned long long)xorv,
				GWorldLooksReal(raw) ? 1 : 0,
				GWorldLooksReal(dec) ? 1 : 0,
				GWorldLooksReal(xorv) ? 1 : 0);
		}

		struct GWorldCandidate {
			uint64_t rva;
			uint64_t object;
			uint64_t vtable;
			int offImagePointers;
			uint64_t at0x110, at0xF0, at0x128, at0x158;
		};

		inline int ProbeGWorldCandidates(uint64_t GameBase, GWorldCandidate* out, int maxOut) {
			if (!ctx || !GameBase || !out || maxOut <= 0) return 0;
			GetModuleBounds(GameBase, g_moduleEnd);
			if (!g_moduleEnd) return 0;

			auto offImage = [&](uint64_t p) {
				return GWorldPtrSane(p) && !(p >= GameBase && p < g_moduleEnd);
			};

			int found = 0;

			auto emit = [&](uint64_t rva, uint64_t cand) {
				if (found >= maxOut || !offImage(cand) || (cand & 0xF))
					return;
				const uint64_t vtable = ctx->read<uint64_t>(cand);
				if (!GWorldPtrSane(vtable) || vtable < GameBase || vtable >= g_moduleEnd)
					return;
				std::vector<uint8_t> obj(0x400);
				if (!ctx->read_array(cand, obj.data(), obj.size()))
					return;
				int ptrCount = 0;
				for (size_t k = 8; k + 8 <= obj.size(); k += 8) {
					uint64_t f = 0;
					memcpy(&f, obj.data() + k, sizeof(f));
					if (offImage(f)) ++ptrCount;
				}
				GWorldCandidate c{};
				c.rva = rva;
				c.object = cand;
				c.vtable = vtable;
				c.offImagePointers = ptrCount;
				memcpy(&c.at0x110, obj.data() + 0x110, 8);
				memcpy(&c.at0xF0,  obj.data() + 0xF0,  8);
				memcpy(&c.at0x128, obj.data() + 0x128, 8);
				memcpy(&c.at0x158, obj.data() + 0x158, 8);
				out[found++] = c;
			};

			const uint64_t raw = ctx->read<uint64_t>(GameBase + STEAM_GWORLD_RVA);
			emit(STEAM_GWORLD_RVA, raw);
			if (GWorldPtrSane(raw)) {
				emit(STEAM_GWORLD_RVA, ctx->read<uint64_t>(raw));
				emit(STEAM_GWORLD_RVA, ctx->read<uint64_t>(raw + 8));
			}

			const uint64_t kScanStart = 0x0E000000ULL;
			const uint64_t kScanEnd   = g_moduleEnd ? (g_moduleEnd - GameBase) : 0x11800000ULL;
			constexpr size_t kBlock = 0x10000;
			std::vector<uint8_t> buf(kBlock);
			int examined = 0;

			for (uint64_t rva = kScanStart; rva < kScanEnd && found < maxOut; rva += kBlock) {
				if (!ctx->read_array(GameBase + rva, buf.data(), kBlock))
					continue;
				for (size_t i = 0; i + 8 <= kBlock && found < maxOut; i += 8) {
					uint64_t cand = 0;
					memcpy(&cand, buf.data() + i, sizeof(cand));
					if (!offImage(cand) || (cand & 0xF))
						continue;
					const uint64_t vtable = ctx->read<uint64_t>(cand);
					if (!GWorldPtrSane(vtable) || vtable < GameBase || vtable >= g_moduleEnd)
						continue;
					if (++examined > 2500)
						return found;
					emit(rva + i, cand);
				}
			}
			return found;
		}

		inline uint64_t ScanForGWorld(uint64_t GameBase, uint64_t& outRva) {
			outRva = 0;
			if (!ctx || !GameBase) return 0;
			GetModuleBounds(GameBase, g_moduleEnd);

			const uint64_t kNear = 0x2000;
			const uint64_t kCenter = STEAM_GWORLD_RVA;
			const uint64_t kImageSize = g_moduleEnd ? (g_moduleEnd - GameBase) : 0x11800000ULL;
			const uint64_t nearBeg = (kCenter > kNear) ? (kCenter - kNear) : 0;
			const uint64_t nearEnd = (kCenter + kNear < kImageSize) ? (kCenter + kNear) : kImageSize;

			for (uint64_t rva = nearBeg & ~7ULL; rva < nearEnd; rva += 8) {
				const uint64_t cand = ctx->read<uint64_t>(GameBase + rva);
				const uint64_t hit = GWorldResolveFromPointer(cand);
				if (!hit)
					continue;
				outRva = rva;
				g_gworldRvaOverride = rva;
				return hit;
			}

			const uint64_t kScanStart = 0x00400000ULL;
			constexpr size_t kBlock = 0x10000;
			std::vector<uint8_t> buf(kBlock);
			for (uint64_t rva = kScanStart; rva < kImageSize; rva += kBlock) {
				if (!ctx->read_array(GameBase + rva, buf.data(), kBlock))
					continue;
				for (size_t i = 0; i + 8 <= kBlock; i += 8) {
					uint64_t cand = 0;
					memcpy(&cand, buf.data() + i, sizeof(cand));
					if (!GWorldPtrSane(cand) || (cand & 0xF))
						continue;
					if (!GWorldLooksReal(cand))
						continue;
					outRva = rva + i;
					g_gworldRvaOverride = outRva;
					return cand;
				}
			}
			return 0;
		}

		inline uint64_t DecryptGObjectsBasePtr(uint64_t GameBase) {
			if (!ctx || !GameBase) return 0;

			__m128i enc     = ctx->read<__m128i>(GameBase + STEAM_GOBJECTS_BASE_ENC_RVA);
			__m128i xor_key = ctx->read<__m128i>(GameBase + STEAM_GOBJECTS_BASE_XOR_KEY_RVA);

			__m128i xored = _mm_xor_si128(enc, xor_key);
			__m128i r64   = _mm_or_si128(
				_mm_slli_epi64(xored, STEAM_GOBJECTS_BASE_ROT64_AMOUNT),
				_mm_srli_epi64(xored, 64 - STEAM_GOBJECTS_BASE_ROT64_AMOUNT));
			__m128i shuf  = _mm_shufflelo_epi16(r64, STEAM_GOBJECTS_BASE_SHUFLO_IMM);
			return shuf.m128i_u64[0];
		}

		inline uint32_t GetGObjectsCount(uint64_t GameBase) {
			if (!ctx || !GameBase) return 0;

			uint64_t base_ptr = DecryptGObjectsBasePtr(GameBase);
			if (!base_ptr) return 0;

			__m128i enc     = ctx->read<__m128i>(base_ptr + STEAM_GOBJECTS_COUNT_OFFSET);
			__m128i xor_key = ctx->read<__m128i>(GameBase + STEAM_GOBJECTS_COUNT_XOR_KEY_RVA);

			__m128i r64   = _mm_or_si128(
				_mm_slli_epi64(enc, STEAM_GOBJECTS_COUNT_ROT64_AMOUNT),
				_mm_srli_epi64(enc, 64 - STEAM_GOBJECTS_COUNT_ROT64_AMOUNT));
			__m128i shuf  = _mm_shufflelo_epi16(r64, STEAM_GOBJECTS_COUNT_SHUFLO_IMM);
			__m128i xored = _mm_xor_si128(shuf, xor_key);
			int32_t count = _mm_cvtsi128_si32(xored);

			if (count <= 0 || count > 20000000) return 0;
			return (uint32_t)count;
		}

		inline uint64_t GetGObjectsChunksPtr(uint64_t GameBase) {
			if (!ctx || !GameBase) return 0;

			uint64_t base_ptr = DecryptGObjectsBasePtr(GameBase);
			if (!base_ptr) return 0;

			uint64_t vtable = ctx->read<uint64_t>(base_ptr + STEAM_GOBJECTS_VTABLE_OFFSET);
			if (!vtable) return 0;
			uint64_t funcPtr = ctx->read<uint64_t>(vtable + STEAM_GOBJECTS_VTABLE_FN_INDEX);
			if (!funcPtr) return 0;

			return EmulateVTableCallWithTLS(GameBase, funcPtr,
				base_ptr + STEAM_GOBJECTS_VTABLE_OFFSET,
				base_ptr + STEAM_GOBJECTS_CTX_OFFSET);
		}

		inline uint64_t GetObjectFromGObjects(uint64_t GameBase, uint64_t chunksPtr, int32_t objectIndex) {
			(void)GameBase;
			if (!chunksPtr || objectIndex < 0) return 0;

			uint32_t chunkIndex = ((uint32_t)objectIndex >> 16) & 0xFFFF;
			uint32_t elementIndex = (uint32_t)objectIndex & 0xFFFF;

			uint64_t chunkPtr = ctx->read<uint64_t>(chunksPtr + 8ULL * chunkIndex);
			if (!chunkPtr) return 0;

			uint64_t itemAddr = chunkPtr + (uint64_t)STEAM_GOBJECTS_ITEM_SIZE * elementIndex;

			uint32_t flags = ctx->read<uint32_t>(itemAddr + STEAM_GOBJECTS_ITEM_FLAGS_OFFSET);
			if ((flags & STEAM_GOBJECTS_INVALID_FLAGS_MASK) != 0) return 0;

			uint64_t objPtr = ctx->read<uint64_t>(itemAddr);
			if (!objPtr) return 0;

			return objPtr;
		}

		// Rotates a character within an inclusive range, leaving anything outside untouched.
		inline uint8_t RotateCharacter(int character, int low, int high, int amount) {
			return character >= low && character <= high
				? static_cast<uint8_t>((character - low + amount) % (high - low + 1) + low)
				: static_cast<uint8_t>(character);
		}

		// Player-name cipher. The keystream constants changed with the patch (seed
		// 0x20003155 -> 0xA7A3FF6B, rotate 29 -> 19) and the XOR mask is now taken from the
		// post-multiply state instead of -109 * the intermediate, which is why names came
		// back as printable garbage. The substitution stage is unchanged: ROT47 over the
		// printable range, ROT5 over digits, ROT13 over each letter case, then ROT47 again.
		inline void DecodeInPlace(uint16_t* s, int32_t maxLenChars) {
			if (!s || maxLenChars <= 0 || !s[0])
				return;

			static uint8_t substitution[256]{};
			static bool initialized = false;
			if (!initialized) {
				for (int character = 0; character < 256; ++character) {
					uint8_t value = RotateCharacter(character, 33, 126, 47);
					value = RotateCharacter(value, 48, 57, 5);
					value = RotateCharacter(value, 97, 122, 13);
					value = RotateCharacter(value, 65, 90, 13);
					substitution[character] = RotateCharacter(value, 33, 126, 47);
				}
				initialized = true;
			}

			uint32_t state = 0;
			int i = 0;
			for (; i < maxLenChars && s[i] != 0; ++i) {
				state = STEAM_PLAYERNAME_FNV_PRIME *
					(state + __ROL4__(
						STEAM_PLAYERNAME_FNV_PRIME * state + STEAM_PLAYERNAME_FNV_SEED,
						STEAM_PLAYERNAME_FNV_ROL));
				s[i] = substitution[(s[i] ^ static_cast<int>(state & 0x1Fu)) & 0xFF];
			}

			s[i] = 0;
		}

		inline int64_t extract_epi64(__m128i value, int index)
		{
			if (index == 0)
				return _mm_cvtsi128_si64(value);
			else
				return _mm_extract_epi64(value, 1);
		}

		inline float DecryptRenderFloat(uint32_t encrypted, uint32_t key) {
			if (key == 0) {
				float f;
				memcpy(&f, &encrypted, sizeof(f));
				return f;
			}
			uint32_t bits = _byteswap_ulong(encrypted ^ key);
			float f;
			memcpy(&f, &bits, sizeof(f));
			return f;
		}

		// World clock published by the cache once per pass. CL-1372005 pins
		// TimeSeconds at UWorld+0x240 (STEAM_WORLD_TIME_SECONDS); WorldPrivate is
		// mesh+0x140. The probe still learns the clock if that offset is empty.
		inline std::atomic<double> g_worldTimeSeconds{ 0.0 };
		inline std::atomic<uint32_t> g_worldClockOffset{ 0 };
		inline std::atomic<int> g_worldClockIsDouble{ 0 };

		// CL-1372005: LastRenderTime / OnScreen are plaintext floats at 0x4C0 / 0x4C4
		// (XOR scheme retired). Keys are 0 so DecryptRenderFloat is a bitcast.
		inline std::atomic<uint32_t> g_renderStampOffset{ STEAM_MESH_LAST_RENDER_TIME_ONSCREEN };
		inline std::atomic<uint32_t> g_renderStampKey{ STEAM_MESH_LAST_RENDER_ONSCREEN_KEY };

		// Set once ProbeRenderStamp has graded a field against the live clock.
		// The dump-pinned pair above reads 0x00000000 on every mesh of the
		// current build, so a learned offset takes priority over both.
		inline std::atomic<bool> g_stampResolved{ false };

		// "Occluded" and "I have no data" are different answers and must not share a
		// return value - see IsVisible.
		//
		// Tracked as an unbroken run of successful decodes rather than a flag set per
		// call, because IsVisible runs over every mesh type. ARC meshes read something
		// plausible at 0x4C4 while every player mesh on this build reads 0x00000000, so
		// a per-call flag sat true half the time and would have re-armed Hide Invisible
		// ESP against players whose data is still garbage. Requiring an unbroken run
		// means one failing mesh anywhere keeps the subsystem marked untrusted, which is
		// the safe direction for the one control that can remove a player from screen.
		inline std::atomic<uint32_t> g_stampOkStreak{ 0 };
		inline constexpr uint32_t kStampTrustStreak = 64;

		inline bool StampDataUsable() {
			return g_stampOkStreak.load(std::memory_order_relaxed) >= kStampTrustStreak;
		}

		inline void NoteStampDecoded() {
			uint32_t n = g_stampOkStreak.load(std::memory_order_relaxed);
			if (n < kStampTrustStreak)
				g_stampOkStreak.store(n + 1, std::memory_order_relaxed);
		}

		inline void NoteStampUndecodable() {
			g_stampOkStreak.store(0, std::memory_order_relaxed);
		}

		inline std::atomic<uint32_t> g_visFlagOff{ 0 };
		inline std::atomic<uint8_t> g_visA57saw0{ 0 };
		inline std::atomic<uint8_t> g_visA57saw1{ 0 };
		inline std::atomic<uint8_t> g_vis997saw0{ 0 };
		inline std::atomic<uint8_t> g_vis997saw1{ 0 };
		inline std::atomic<uint32_t> g_visFlagSaw0{ 0 };
		inline std::atomic<uint32_t> g_visFlagSaw1{ 0 };

		// Runtime LRTS (LastRenderTimeOnScreen) discovery. Pre-CL-1372005 XOR-encrypted the
		// float; key = raw ^ byteswap(worldTimeBits) when the mesh is in frustum.
		// bRecentlyRendered @ 0x997 bit5 is frustum-only — used to gate the scan,
		// never as the wall answer. No Sleep: bank hits, verify ~120ms later.
		inline constexpr uint32_t kBrrOffset = 0x997;
		inline constexpr uint8_t  kBrrMask = 0x20;
		inline constexpr int kLrtsMaxKeys = 6;
		inline constexpr uint32_t kLrtsScanLo = 0x400;
		inline constexpr uint32_t kLrtsScanHi = 0x600;

		struct LrtsState {
			uint32_t lrtsOff = 0;
			uint32_t lrtOff = 0;
			uint32_t lrtKey = 0;
			uint32_t keys[kLrtsMaxKeys]{};
			int keyN = 0;
			int scanAttempts = 0;
			uintptr_t lastWorld = 0;

			// Phase-1 bank → phase-2 verify (replaces Sleep(120))
			uintptr_t pendMesh = 0;
			uint64_t pendMs = 0;
			float pendWorldTime = 0.f;
			struct Hit { uint32_t off; uint32_t raw; uint32_t key; };
			Hit pendHits[64]{};
			int pendN = 0;

			// Async key re-learn when known keys stop decrypting
			uintptr_t keyPendMesh = 0;
			uint32_t keyPendKey = 0;
			float keyPendTime = 0.f;
		};
		inline LrtsState g_lrts{};

		inline void ResetLrtsScan() {
			g_lrts = LrtsState{};
		}

		inline void ResetVisibilityProbe() {
			g_renderStampOffset.store(STEAM_MESH_LAST_RENDER_TIME_ONSCREEN, std::memory_order_relaxed);
			g_renderStampKey.store(STEAM_MESH_LAST_RENDER_ONSCREEN_KEY, std::memory_order_relaxed);
			g_stampResolved.store(false, std::memory_order_relaxed);
			g_stampOkStreak.store(0, std::memory_order_relaxed);
			g_visFlagOff.store(0, std::memory_order_relaxed);
			g_visA57saw0.store(0, std::memory_order_relaxed);
			g_visA57saw1.store(0, std::memory_order_relaxed);
			g_vis997saw0.store(0, std::memory_order_relaxed);
			g_vis997saw1.store(0, std::memory_order_relaxed);
			g_visFlagSaw0.store(0, std::memory_order_relaxed);
			g_visFlagSaw1.store(0, std::memory_order_relaxed);
			ResetLrtsScan();
		}

		// Call once per frame per candidate mesh until g_lrts.lrtsOff locks.
		inline void ScanForLRTS(uintptr_t meshComp, float worldTime) {
			if (g_lrts.lrtsOff || !ctx || !meshComp || !validpointer(meshComp))
				return;
			if (worldTime < 10.f || g_lrts.scanAttempts >= 10)
				return;

			const uint64_t nowMs = GetTickCount64();

			// --- Phase 2: verify banked hits after ~120ms ---
			if (g_lrts.pendN > 0 && g_lrts.pendMesh) {
				if (nowMs - g_lrts.pendMs > 2000) {
					g_lrts.pendMesh = 0;
					g_lrts.pendN = 0;
				} else if (g_lrts.pendMesh != meshComp) {
					return; // wait for the same mesh
				} else if (nowMs - g_lrts.pendMs < 120) {
					return;
				} else {
				int best = -1;
				float bestD = 999.f;
				const float expect = g_lrts.pendWorldTime + 0.12f;
				for (int i = 0; i < g_lrts.pendN; ++i) {
					const uint32_t raw2 = ctx->read<uint32_t>(meshComp + g_lrts.pendHits[i].off);
					if (ctx->last_status() != TransportStatus::Ok)
						continue;
					if (raw2 == g_lrts.pendHits[i].raw)
						continue; // static — not a render time
					const float v2 = DecryptRenderFloat(raw2, g_lrts.pendHits[i].key);
					const float d2 = std::fabs(expect - v2);
					if (std::isfinite(v2) && v2 > 1.f && d2 < 1.0f) {
						if (d2 < bestD) {
							bestD = d2;
							best = i;
						}
					}
				}

				if (best >= 0) {
					const uint32_t off1 = g_lrts.pendHits[best].off;
					const uint32_t key1 = g_lrts.pendHits[best].key;
					bool paired = false;
					for (int j = 0; j < g_lrts.pendN; ++j) {
						if (j == best)
							continue;
						const uint32_t oj = g_lrts.pendHits[j].off;
						uint32_t gap = 0;
						if (oj == off1 - 4 || oj == off1 - 8)
							gap = off1 - oj;
						else if (oj == off1 + 4 || oj == off1 + 8)
							gap = oj - off1;
						if (!gap)
							continue;
						if (oj < off1) {
							g_lrts.lrtOff = oj;
							g_lrts.lrtKey = g_lrts.pendHits[j].key;
							g_lrts.lrtsOff = off1;
							g_lrts.keys[0] = key1;
							g_lrts.keyN = 1;
						} else {
							g_lrts.lrtOff = off1;
							g_lrts.lrtKey = key1;
							g_lrts.lrtsOff = oj;
							g_lrts.keys[0] = g_lrts.pendHits[j].key;
							g_lrts.keyN = 1;
						}
						paired = true;
						break;
					}
					if (!paired) {
						g_lrts.lrtsOff = off1;
						g_lrts.keys[0] = key1;
						g_lrts.keyN = 1;
					}
					g_renderStampOffset.store(g_lrts.lrtsOff, std::memory_order_relaxed);
					g_renderStampKey.store(g_lrts.keys[0], std::memory_order_relaxed);
					g_stampResolved.store(true, std::memory_order_relaxed);
				}

				g_lrts.pendMesh = 0;
				g_lrts.pendN = 0;
				return;
				}
			}

			// --- Phase 1: bank candidate dwords that decrypt to ~worldTime ---
			// Prefer frustum (BRR) so we key from a drawn mesh; after a few misses
			// scan anyway — BRR bit is often dead while LRTS still updates.
			const uint8_t brr = ctx->read<uint8_t>(meshComp + kBrrOffset);
			const bool brrOk = ctx->last_status() == TransportStatus::Ok && (brr & kBrrMask);
			if (!brrOk && g_lrts.scanAttempts < 3)
				return;

			++g_lrts.scanAttempts;

			uint32_t wtBits = 0;
			{
				float wf = worldTime;
				memcpy(&wtBits, &wf, 4);
			}
			const uint32_t wtSwapped = _byteswap_ulong(wtBits);

			g_lrts.pendN = 0;

			auto considerRaw = [&](uint32_t off, uint32_t raw, uint32_t key) {
				if (raw == 0 || g_lrts.pendN >= 64)
					return;
				const float v = DecryptRenderFloat(raw, key);
				if (!std::isfinite(v) || v < 1.f)
					return;
				if (std::fabs(worldTime - v) > 0.5f)
					return;
				g_lrts.pendHits[g_lrts.pendN++] = { off, raw, key };
			};

			// Seed dump LRTS / LRT offsets first (known layout on this CL).
			{
				const uint32_t rawOn = ctx->read<uint32_t>(
					meshComp + STEAM_MESH_LAST_RENDER_TIME_ONSCREEN);
				const uint32_t rawRt = ctx->read<uint32_t>(
					meshComp + STEAM_MESH_LAST_RENDER_TIME);
				if (rawOn) {
					considerRaw(STEAM_MESH_LAST_RENDER_TIME_ONSCREEN, rawOn,
						rawOn ^ wtSwapped);
					considerRaw(STEAM_MESH_LAST_RENDER_TIME_ONSCREEN, rawOn,
						STEAM_MESH_LAST_RENDER_ONSCREEN_KEY);
				}
				if (rawRt) {
					considerRaw(STEAM_MESH_LAST_RENDER_TIME, rawRt, rawRt ^ wtSwapped);
					considerRaw(STEAM_MESH_LAST_RENDER_TIME, rawRt,
						STEAM_MESH_LAST_RENDER_TIME_KEY);
				}
			}

			constexpr uint32_t kScanSize = kLrtsScanHi - kLrtsScanLo;
			uint8_t buf[kScanSize];
			if (!ctx->read_array(meshComp + kLrtsScanLo, buf, kScanSize)) {
				if (g_lrts.pendN == 0)
					return;
			} else {
				for (uint32_t rel = 0; rel + 4 <= kScanSize; rel += 4) {
					uint32_t raw = 0;
					memcpy(&raw, buf + rel, 4);
					if (raw == 0)
						continue;
					considerRaw(kLrtsScanLo + rel, raw, raw ^ wtSwapped);
				}
			}
			if (g_lrts.pendN == 0)
				return;

			g_lrts.pendMesh = meshComp;
			g_lrts.pendMs = nowMs;
			g_lrts.pendWorldTime = worldTime;
		}

		// Occlusion answer once ScanForLRTS has locked. Visible iff LRTS ~= worldTime.
		inline bool IsMeshVisibleLrts(uintptr_t meshComp, float worldTime, uintptr_t worldPtr = 0) {
			if (worldPtr && g_lrts.lastWorld != worldPtr) {
				g_lrts.lastWorld = worldPtr;
				ResetLrtsScan();
				g_stampResolved.store(false, std::memory_order_relaxed);
			}
			if (!ctx || !meshComp || !validpointer(meshComp))
				return false;
			if (!g_lrts.lrtsOff || g_lrts.keyN <= 0 || worldTime <= 1.f)
				return false;

			if (g_lrts.keyPendMesh && worldTime - g_lrts.keyPendTime > 2.0f) {
				const uint32_t pr = ctx->read<uint32_t>(g_lrts.keyPendMesh + g_lrts.lrtsOff);
				if (ctx->last_status() == TransportStatus::Ok) {
					const float pv = DecryptRenderFloat(pr, g_lrts.keyPendKey);
					if (std::isfinite(pv) && std::fabs(worldTime - pv) < 1.5f) {
						bool dup = false;
						for (int k = 0; k < g_lrts.keyN; ++k) {
							if (g_lrts.keys[k] == g_lrts.keyPendKey) {
								dup = true;
								break;
							}
						}
						if (!dup && g_lrts.keyN < kLrtsMaxKeys)
							g_lrts.keys[g_lrts.keyN++] = g_lrts.keyPendKey;
					}
				}
				g_lrts.keyPendMesh = 0;
			}

			const uint32_t raw = ctx->read<uint32_t>(meshComp + g_lrts.lrtsOff);
			if (ctx->last_status() != TransportStatus::Ok || raw == 0)
				return false;

			float bestVal = 0.f;
			bool found = false;
			for (int k = 0; k < g_lrts.keyN; ++k) {
				const float v = DecryptRenderFloat(raw, g_lrts.keys[k]);
				if (std::isfinite(v) && v > -2000.f && v < worldTime + 10.f) {
					bestVal = v;
					found = true;
					break;
				}
			}

			if (!found) {
				const uint8_t brr = ctx->read<uint8_t>(meshComp + kBrrOffset);
				if ((brr & kBrrMask) && !g_lrts.keyPendMesh && g_lrts.keyN < kLrtsMaxKeys) {
					uint32_t wtBits = 0;
					{
						float wf = worldTime;
						memcpy(&wtBits, &wf, 4);
					}
					g_lrts.keyPendKey = raw ^ _byteswap_ulong(wtBits);
					g_lrts.keyPendMesh = meshComp;
					g_lrts.keyPendTime = worldTime;
				}
				return false;
			}

			const float delta = worldTime - bestVal;
			return delta <= STEAM_VISIBLE_EPSILON && delta >= -0.25f;
		}

		// Several UWorld fields advance at clock rate - TimeSeconds, RealTimeSeconds,
		// AudioTimeSeconds - and they do not share an origin. The render stamp and its clock
		// must be in the same timebase, so the stamp probe is what confirms which one is right;
		// a candidate that yields no stamp is discarded and the next is tried. Without this a
		// clock in the wrong timebase differs by a constant and reads as permanently occluded,
		// which is indistinguishable from a genuinely hidden player.
		struct WorldClockCandidate {
			uint32_t offset = 0;
			bool isDouble = false;
		};
		inline WorldClockCandidate g_clockCandidates[8]{};
		inline int g_clockCandidateCount = 0;
		inline int g_clockCandidateIndex = 0;

		inline double ReadWorldClockAt(uintptr_t uworld, const WorldClockCandidate& c) {
			if (!c.offset)
				return 0.0;
			return c.isDouble
				? ctx->read<double>(uworld + c.offset)
				: (double)ctx->read<float>(uworld + c.offset);
		}

		// Two snapshots of the same window, taken a known interval apart. A clock is the field
		// whose value grew by roughly the elapsed wall time.
		inline bool ProbeWorldClock(uintptr_t uworld) {
			if (!ctx || !uworld || !validpointer(uworld))
				return false;

			constexpr uint32_t kBegin = 0x600;
			constexpr uint32_t kEnd = 0x1200;
			constexpr size_t kSpan = (size_t)(kEnd - kBegin);

			static std::vector<uint8_t> s_first;
			static uint64_t s_firstMs = 0;
			static uintptr_t s_firstWorld = 0;

			std::vector<uint8_t> now(kSpan);
			if (!ctx->read_array(uworld + kBegin, now.data(), kSpan))
				return false;
			const uint64_t nowMs = GetTickCount64();

			static std::mutex s_clockProbeMtx;
			std::lock_guard<std::mutex> lk(s_clockProbeMtx);

			// The first pass only banks a reference sample; a rate cannot be measured from one
			// reading. Re-banked if the world changed, since offsets carry no meaning across it.
			if (s_first.size() != kSpan || s_firstWorld != uworld || s_firstMs == 0) {
				s_first = std::move(now);
				s_firstMs = nowMs;
				s_firstWorld = uworld;
				return false;
			}

			const double dt = (double)(nowMs - s_firstMs) / 1000.0;
			if (dt < 0.15)
				return false;   // too short to tell a clock from noise

			g_clockCandidateCount = 0;
			g_clockCandidateIndex = 0;

			auto consider = [&](uint32_t off, bool isDouble, double a, double b) {
				if (g_clockCandidateCount >= 8)
					return;
				if (!std::isfinite(a) || !std::isfinite(b))
					return;
				if (a <= 1.0 || a > 1.0e7 || b <= 1.0 || b > 1.0e7)
					return;
				const double advanced = b - a;
				if (advanced < dt * 0.5 || advanced > dt * 1.6)
					return;
				g_clockCandidates[g_clockCandidateCount++] = { off, isDouble };
			};

			for (size_t i = 0; i + 8 <= kSpan; i += 8) {
				double a = 0.0, b = 0.0;
				memcpy(&a, s_first.data() + i, sizeof(a));
				memcpy(&b, now.data() + i, sizeof(b));
				consider(kBegin + (uint32_t)i, true, a, b);
			}
			for (size_t i = 0; i + 4 <= kSpan; i += 4) {
				float a = 0.0f, b = 0.0f;
				memcpy(&a, s_first.data() + i, sizeof(a));
				memcpy(&b, now.data() + i, sizeof(b));
				consider(kBegin + (uint32_t)i, false, (double)a, (double)b);
			}

			// Kept as the new reference so a later re-probe measures a fresh interval.
			s_first = std::move(now);
			s_firstMs = nowMs;

			if (g_clockCandidateCount == 0)
				return false;

			g_worldClockOffset.store(g_clockCandidates[0].offset, std::memory_order_relaxed);
			g_worldClockIsDouble.store(g_clockCandidates[0].isDouble ? 1 : 0, std::memory_order_relaxed);
			return true;
		}

		// Single-sample window used only as a last-ditch sanity check on a cached offset.
		// The probe itself no longer trusts this: a position float decrypts into this range
		// often enough that the first hit was garbage, which is what produced the flicker.
		inline bool RenderStampPlausible(float stamp, double worldNow) {
			if (!std::isfinite(stamp) || stamp <= 1.0f)
				return false;
			const double d = worldNow - (double)stamp;
			return d >= -1.0 && d <= 120.0;
		}

		enum class StampProbeResult {
			Pending,
			Failed,
			Found
		};

		struct StampCandidate {
			uint32_t offset = 0;
			uint32_t key = 0;
			float value = 0.0f;
			double delta = 0.0;     // worldNow - value
			double advanced = 0.0;  // how much it moved between the paired samples
			bool strict = false;    // passed the full clock-tracking test
		};

		// Reported in StampCandidate::key when the entry came from the plaintext
		// double pass rather than an XOR-decoded float.
		inline constexpr uint32_t kDoubleMarkerKey = 0xDDDDDDDDu;

		// Separates "the read failed", "the window is all zeros" and "there is real
		// data here but nothing tracks the clock". All three previously surfaced as
		// an identical FAILED/matches=0 line, which is why two sessions of empty
		// results said nothing about which one was actually happening.
		struct StampProbeStats {
			bool readOk = false;
			int nonZeroDwords = 0;
			int changedDwords = 0;
			int totalDwords = 0;
			double dt = 0.0;
		};

		// Two snapshots of the same mesh window, taken a known interval apart. A real stamp
		// is non-decreasing, advances at most at clock rate, and sits within 2s of worldNow
		// on at least one of the two samples - which a stable position float cannot do.
		// Pending means the first sample is banked and the clock must not be advanced.
		// Failed means this mesh is not being drawn; the next call is free to bank another.
		// outCands is optional. It receives every field that moved with the clock,
		// strict ones first, so a Failed result can still say what it was choosing
		// between instead of just reporting nothing.
		inline StampProbeResult ProbeRenderStamp(uintptr_t mesh, double worldNow,
			uint32_t& outOffset, uint32_t& outKey,
			StampCandidate* outCands = nullptr, int maxCands = 0, int* outCandCount = nullptr,
			StampProbeStats* outStats = nullptr)
		{
			if (outCandCount)
				*outCandCount = 0;
			if (outStats)
				*outStats = StampProbeStats{};
			if (!ctx || !mesh || !validpointer(mesh) || !std::isfinite(worldNow) || worldNow <= 1.0)
				return StampProbeResult::Failed;

			// Whole-object sweep. The successive narrower windows were all anchored
			// on the dump offsets, which read zero, so they were guesses built on a
			// broken reference.
			constexpr uint32_t kBegin = 0x000;
			constexpr uint32_t kEnd = 0x1800;
			constexpr size_t kSpan = (size_t)(kEnd - kBegin);

			static std::mutex s_stampProbeMtx;
			static std::vector<uint8_t> s_first;
			static uint64_t s_firstMs = 0;
			static uintptr_t s_firstMesh = 0;

			std::vector<uint8_t> now(kSpan);
			if (!ctx->read_array(mesh + kBegin, now.data(), kSpan))
				return StampProbeResult::Failed;
			const uint64_t nowMs = GetTickCount64();

			if (outStats) {
				outStats->readOk = true;
				outStats->totalDwords = (int)(kSpan / 4);
				for (size_t i = 0; i + 4 <= kSpan; i += 4) {
					uint32_t v = 0;
					memcpy(&v, now.data() + i, sizeof(v));
					if (v != 0)
						++outStats->nonZeroDwords;
				}
			}

			// Debug 29 through Debug 42: stampOff stays 0x0 the whole raid. Do not guess
			// another mesh offset from a log — that needs a live dump of
			// LastRenderTime / LastRenderTimeOnScreen / bRecentlyRendered on a drawn
			// SkeletalMeshComponent. Until the stamp is pinned, IsVisible is false.
			// Present, the 8ms player cache, and ARC vis all call IsVisible while the
			// stamp is still unknown. The bank below is a process-wide slot; concurrent
			// move/clear/data() is a heap AV.
			std::lock_guard<std::mutex> lk(s_stampProbeMtx);

			auto bank = [&]() {
				s_first = std::move(now);
				s_firstMs = nowMs;
				s_firstMesh = mesh;
			};

			if (s_first.size() != kSpan || s_firstMesh == 0 || s_firstMs == 0) {
				bank();
				return StampProbeResult::Pending;
			}

			const double dt = (double)(nowMs - s_firstMs) / 1000.0;
			if (s_firstMesh != mesh) {
				// Another mesh still owns the slot. Steal it only if that mesh never came
				// back - otherwise every player in the pass re-banks and no pair is ever
				// formed.
				if (dt > 1.0)
					bank();
				return StampProbeResult::Pending;
			}
			if (dt < 0.15)
				return StampProbeResult::Pending;

			// How much of the window is live at all. A mesh that is genuinely being
			// drawn should have plenty of churn; near-zero churn means this mesh is
			// dormant and grading it for a moving stamp is pointless.
			if (outStats) {
				outStats->dt = dt;
				for (size_t i = 0; i + 4 <= kSpan; i += 4) {
					uint32_t va = 0, vb = 0;
					memcpy(&va, s_first.data() + i, sizeof(va));
					memcpy(&vb, now.data() + i, sizeof(vb));
					if (va != vb)
						++outStats->changedDwords;
				}
			}

			const uint32_t keys[3] = {
				STEAM_MESH_LAST_RENDER_ONSCREEN_KEY, STEAM_MESH_LAST_RENDER_TIME_KEY, 0 };

			auto decode = [&](const std::vector<uint8_t>& buf, size_t i, uint32_t key, float& out, uint32_t* rawOut) -> bool {
				uint32_t raw = 0;
				memcpy(&raw, buf.data() + i, sizeof(raw));
				if (raw == 0 || raw == 0xFFFFFFFFu || raw == 0x0010FFFFu)
					return false;
				out = DecryptRenderFloat(raw, key);
				if (rawOut) *rawOut = raw;
				return true;
			};

			auto stampOk = [&](float s) {
				return std::isfinite(s) && s > 1.0f && (double)s <= worldNow + 1.0;
			};

			// A live on-screen stamp must move with the clock. The previous rule allowed
			// a hold (advance 0), which is how mesh+0x52C locked: raw 0x0010FFFF decrypted
			// to a constant 20.398 that happened to sit within 2s of worldNow, then never
			// moved again. Occluded meshes fail this on purpose so the probe waits for one
			// that is actually being drawn.
			auto timeValid = [&](float a, float b) {
				if (!stampOk(a) || !stampOk(b))
					return false;
				const double advanced = (double)b - (double)a;
				if (advanced < dt * 0.3 || advanced > dt * 1.5)
					return false;
				const double ageB = worldNow - (double)b;
				return ageB >= -0.25 && ageB <= 2.0;
			};

			uint32_t bestOff = 0, bestKey = 0;
			double bestAge = 1.0e9;
			bool bestPaired = false;

			for (size_t i = 0; i + 4 <= kSpan; i += 4) {
				for (uint32_t key : keys) {
					float a = 0.0f, b = 0.0f;
					uint32_t rawA = 0, rawB = 0;
					if (!decode(s_first, i, key, a, &rawA) || !decode(now, i, key, b, &rawB))
						continue;
					if (rawA == rawB)
						continue;
					const bool strict = timeValid(a, b);

					// Epoch-agnostic reporting: anything ticking at roughly clock
					// rate, whatever its absolute value.
					//
					// Every previous net also required the value to sit near
					// worldNow, which silently assumed the stamp shares an epoch
					// with UWorld+0x8B0. A stamp keyed to absolute app time or to
					// level-load time tracks the clock perfectly and still gets
					// thrown out by that assumption. Rate is the invariant; the
					// offset between epochs is a constant I can read off the value.
					if (outCands && outCandCount && *outCandCount < maxCands) {
						const double advanced = (double)b - (double)a;
						const bool rateMatch = std::isfinite(a) && std::isfinite(b) &&
							a > 0.0f && b > 0.0f &&
							advanced >= dt * 0.5 && advanced <= dt * 1.5;
						if (strict || rateMatch) {
							auto& c = outCands[(*outCandCount)++];
							c.offset = kBegin + (uint32_t)i;
							c.key = key;
							c.value = b;
							c.delta = worldNow - (double)b;
							c.advanced = advanced;
							c.strict = strict;
						}
					}

					if (!strict)
						continue;

					// The two render stamps sit 8 bytes apart with the on-screen one higher.
					// Pairing is the tiebreak, not the acceptance test - a lone stamp that
					// tracks the clock is still preferred over a paired position float.
					bool paired = false;
					if (i >= 8) {
						for (uint32_t k2 : keys) {
							float ba = 0.0f, bb = 0.0f;
							if (decode(s_first, i - 8, k2, ba, nullptr) && decode(now, i - 8, k2, bb, nullptr) &&
								timeValid(ba, bb)) {
								paired = true;
								break;
							}
						}
					}

					const double age = worldNow - (double)b;
					if ((paired && !bestPaired) || (paired == bestPaired && age < bestAge)) {
						bestOff = kBegin + (uint32_t)i;
						bestKey = key;
						bestAge = age;
						bestPaired = paired;
					}
				}
			}

			// Plaintext double pass. UE keeps its authoritative time bases as
			// doubles, and the float sweep above cannot see one: an 8-byte double
			// read as two dwords looks like noise under every key.
			if (outCands && outCandCount) {
				for (size_t i = 0; i + 8 <= kSpan && *outCandCount < maxCands; i += 8) {
					double da = 0.0, db = 0.0;
					memcpy(&da, s_first.data() + i, sizeof(da));
					memcpy(&db, now.data() + i, sizeof(db));
					if (!std::isfinite(da) || !std::isfinite(db) || da <= 0.0 || db <= 0.0)
						continue;
					const double advanced = db - da;
					if (advanced < dt * 0.5 || advanced > dt * 1.5)
						continue;
					auto& c = outCands[(*outCandCount)++];
					c.offset = kBegin + (uint32_t)i;
					c.key = kDoubleMarkerKey;
					c.value = (float)db;
					c.delta = worldNow - db;
					c.advanced = advanced;
					c.strict = false;
				}
			}

			// Release the slot either way so a failed mesh does not monopolise the next pass.
			s_first.clear();
			s_firstMs = 0;
			s_firstMesh = 0;

			if (!bestOff)
				return StampProbeResult::Failed;
			outOffset = bestOff;
			outKey = bestKey;
			return StampProbeResult::Found;
		}

		// Mesh vis sources, in order: LastSubmit/LastRender/LastRenderOnScreen triplet,
		// decrypted dump stamps, learned stamp vs world clock, then a bool that actually
		// toggles. 0xA57 is dumped bRecentlyRendered but reads 0 on every player mesh
		// this CL — treating that as occluded painted every skeleton red.
		struct MeshVisRead {
			bool visible = true;
			uint32_t method = 0;
			float submit = 0.f;
			float render = 0.f;
			float onscreen = 0.f;
			uint8_t a57 = 0;
			uint8_t a97 = 0;
			uint32_t flagOff = 0;
		};

		inline bool RenderTimesUsable(float a, float b, float c) {
			if (!std::isfinite(a) || !std::isfinite(b) || !std::isfinite(c))
				return false;
			if (a < 0.f || b < 0.f || c < 0.f)
				return false;
			if (a > 1.0e7f || b > 1.0e7f || c > 1.0e7f)
				return false;
			if (a == 0.f && b == 0.f && c == 0.f)
				return false;
			return (a > 1.f) || (b > 1.f) || (c > 1.f);
		}

		inline bool VisibleFromRenderTimes(float submit, float render, float onscreen) {
			if (!std::isfinite(render) || !std::isfinite(onscreen))
				return false;
			if (render < 1.f && onscreen < 1.f)
				return false;

			const double worldNow = g_worldTimeSeconds.load(std::memory_order_relaxed);
			const bool haveClock = std::isfinite(worldNow) && worldNow > 1.0 && onscreen > 1.f;

			// Primary wall signal: LastRenderTimeOnScreen age vs world clock.
			// Occluded meshes freeze OnScreen while the world clock keeps moving.
			if (haveClock) {
				const double dt = worldNow - (double)onscreen;
				if (dt > (double)STEAM_VISIBLE_EPSILON)
					return false;
				if (dt >= -0.25 && dt <= (double)STEAM_VISIBLE_EPSILON)
					return true;
			}

			// Secondary: LRT vs LRTS. Behind a wall OnScreen freezes while Render
			// (or Submit) may still tick — gap grows. Do not accept a small gap
			// alone without a fresh OnScreen vs clock when the clock is live.
			const float frozen = std::fabs(render - onscreen);
			if (frozen > 0.08f)
				return false;
			if (!haveClock && frozen <= 0.02f)
				return true;
			return false;
		}

		inline MeshVisRead ClassifyVisible(uintptr_t mesh) {
			MeshVisRead out{};
			if (!ctx || !mesh || !validpointer(mesh)) {
				out.visible = false;
				return out;
			}

			const double worldNow = g_worldTimeSeconds.load(std::memory_order_relaxed);
			const bool haveClock = std::isfinite(worldNow) && worldNow > 1.0;

			// Preferred: runtime-locked LRTS vs worldTime (plaintext or XOR).
			if (g_lrts.lrtsOff && g_lrts.keyN > 0 && haveClock) {
				const float wt = (float)worldNow;
				const uint32_t raw = ctx->read<uint32_t>(mesh + g_lrts.lrtsOff);
				if (ctx->last_status() == TransportStatus::Ok && raw != 0) {
					float bestVal = 0.f;
					float bestAbs = 1.0e9f;
					bool found = false;
					for (int k = 0; k < g_lrts.keyN; ++k) {
						const float stamp = DecryptRenderFloat(raw, g_lrts.keys[k]);
						if (!std::isfinite(stamp) || stamp <= 1.f || stamp >= wt + 5.f)
							continue;
						const float d = wt - stamp;
						// Reject keys that decrypt to ancient / far-future junk.
						if (d < -0.5f || d > 30.f)
							continue;
						const float ad = std::fabs(d);
						if (ad < bestAbs) {
							bestAbs = ad;
							bestVal = stamp;
							found = true;
						}
					}
					if (found) {
						out.onscreen = bestVal;
						out.method = 5;
						const float delta = wt - bestVal;
						// Visible only while OnScreen is fresh. Slight future
						// (clock/stamp tear) still counts; stale = occluded.
						out.visible = delta <= STEAM_VISIBLE_EPSILON && delta >= -0.25f;

						// If we also locked LRT, a large LRT−LRTS gap confirms cover
						// even when the absolute stamp is still within epsilon.
						if (out.visible && g_lrts.lrtOff && g_lrts.lrtKey) {
							const uint32_t rawLrt = ctx->read<uint32_t>(mesh + g_lrts.lrtOff);
							if (ctx->last_status() == TransportStatus::Ok && rawLrt != 0) {
								const float lrt = DecryptRenderFloat(rawLrt, g_lrts.lrtKey);
								if (std::isfinite(lrt) && lrt > 1.f) {
									out.render = lrt;
									if (std::fabs(lrt - bestVal) > 0.08f)
										out.visible = false;
								}
							}
						}
						return out;
					}
				}
			}

			const uint32_t bounds = (uint32_t)Offsets::Get().USkeletalMeshComponent.BoundsScale;

			// CL-1372005 dump pair (LRT / LRTS) — plaintext floats at 0x4C0 / 0x4C4.
			{
				const uint32_t rawRender = ctx->read<uint32_t>(mesh + STEAM_MESH_LAST_RENDER_TIME);
				const uint32_t rawOn = ctx->read<uint32_t>(mesh + STEAM_MESH_LAST_RENDER_TIME_ONSCREEN);
				if (rawRender != 0 && rawOn != 0) {
					const float render = DecryptRenderFloat(rawRender, STEAM_MESH_LAST_RENDER_TIME_KEY);
					const float onscreen = DecryptRenderFloat(rawOn, STEAM_MESH_LAST_RENDER_ONSCREEN_KEY);
					const uint32_t rawSubmit = ctx->read<uint32_t>(mesh + STEAM_MESH_LAST_RENDER_TIME - 4);
					float submit = render;
					if (rawSubmit != 0)
						submit = DecryptRenderFloat(rawSubmit, 0);
					if (RenderTimesUsable(submit, render, onscreen) ||
						(std::isfinite(render) && std::isfinite(onscreen) && render > 1.f && onscreen > 1.f)) {
						out.submit = submit;
						out.render = render;
						out.onscreen = onscreen;
						out.method = 2;
						out.visible = VisibleFromRenderTimes(submit, render, onscreen);
						return out;
					}
				}
			}

			float times[3]{};
			if (ctx->read_array(mesh + bounds + 4, times, sizeof(times)) &&
				RenderTimesUsable(times[0], times[1], times[2])) {
				out.submit = times[0];
				out.render = times[1];
				out.onscreen = times[2];
				out.method = 1;
				out.visible = VisibleFromRenderTimes(times[0], times[1], times[2]);
				return out;
			}

			if (g_stampResolved.load(std::memory_order_relaxed) && haveClock) {
				const uint32_t off = g_renderStampOffset.load(std::memory_order_relaxed);
				const uint32_t key = g_renderStampKey.load(std::memory_order_relaxed);
				if (off) {
					const uint32_t raw = ctx->read<uint32_t>(mesh + off);
					const bool rawOk = ctx->last_status() == TransportStatus::Ok;
					if (rawOk && raw != 0 && raw != 0xFFFFFFFFu) {
						const float stamp = DecryptRenderFloat(raw, key);
						if (std::isfinite(stamp) && stamp > 1.f) {
							out.onscreen = stamp;
							out.method = 3;
							const double dt = worldNow - (double)stamp;
							out.visible = dt <= (double)STEAM_VISIBLE_EPSILON && dt >= -0.25;
							return out;
						}
					}
				}
			}

			// Do NOT use bRecentlyRendered / bit flags as wall occlusion — frustum only.
			// Leave method=0 so callers can fall back to pose or treat as unknown.
			out.visible = false;
			out.method = 0;
			return out;
		}

		inline bool IsVisible(uintptr_t mesh) {
			return ClassifyVisible(mesh).visible;
		}

		// Same read, reporting its working. Nothing anywhere logged the visibility decision,
		// which is why a permanently-red ESP could not be narrowed down from a log at all.
		struct VisibilitySample {
			bool ok = false;
			uint32_t stampOffset = 0;
			uint32_t stampKey = 0;
			uint32_t rawStamp = 0;
			float stamp = 0.0f;
			double worldNow = 0.0;
			double delta = 0.0;
			bool visible = false;
			uint32_t clockOffset = 0;
			bool clockIsDouble = false;
			int clockCandidates = 0;
			int clockIndex = 0;
		};

		inline VisibilitySample SampleVisibility(uintptr_t mesh) {
			VisibilitySample s{};
			if (!ctx || !mesh || !validpointer(mesh))
				return s;

			s.clockOffset = g_worldClockOffset.load(std::memory_order_relaxed);
			s.clockIsDouble = g_worldClockIsDouble.load(std::memory_order_relaxed) != 0;
			s.clockCandidates = g_clockCandidateCount;
			s.clockIndex = g_clockCandidateIndex;
			s.worldNow = g_worldTimeSeconds.load(std::memory_order_relaxed);
			s.stampOffset = STEAM_MESH_LAST_RENDER_TIME_ONSCREEN;
			s.stampKey = STEAM_MESH_LAST_RENDER_ONSCREEN_KEY;
			s.rawStamp = ctx->read<uint32_t>(mesh + s.stampOffset);
			s.stamp = DecryptRenderFloat(s.rawStamp, s.stampKey);
			s.delta = s.worldNow - (double)s.stamp;
			s.visible = std::isfinite(s.stamp) && s.delta < STEAM_VISIBLE_EPSILON;
			if (!std::isfinite(s.stamp) || std::abs(s.delta) > 120.0) {
				s.stampOffset = STEAM_MESH_LAST_RENDER_TIME;
				s.stampKey = STEAM_MESH_LAST_RENDER_TIME_KEY;
				s.rawStamp = ctx->read<uint32_t>(mesh + s.stampOffset);
				s.stamp = DecryptRenderFloat(s.rawStamp, s.stampKey);
				s.delta = s.worldNow - (double)s.stamp;
				s.visible = std::isfinite(s.stamp) && s.delta < STEAM_VISIBLE_EPSILON;
			}
			s.ok = true;
			return s;
		}

		// Reads both halves of the double-buffered transform array rather than only the
		// one the selector bits point at.
		//
		// The slot maths here is not verifiable from a log, and two independent public
		// implementations disagree with it: on selector 0x80000809 this yields slot 0
		// while (x >> 27) & 0xFFFFFFF0 yields 0x10, and on 0x89E0 the two swap again.
		// Picking the buffer the game is still writing gives data that is mostly fine and
		// intermittently non-finite, which is what a mesh whose spine reads as garbage on
		// every pass while its head and feet stay clean looks like. The mask can also only
		// ever produce two slots, so a mesh rendering at a deeper LOD is forced onto one of
		// them regardless. Returning both lets the caller decide on geometry instead of on
		// a constant nobody can confirm.
		// slotsOut/bonesOut receive every LOD slot, not just the selected pair.
		// LOD index is (x >> 27) & 0xFFFFFFF0, stepped by 0x10.
		// Decrypts the bone-array base from the 16-byte seed at mesh+0x7B0.
		// CL-1341255: ROL64(50) + PSHUFB{5,0,4,6,7,2,3,1} + ROL32(22), no XOR.
		inline uint64_t DecryptBoneArrayBase(const uint8_t seed[16]) {
			__m128i value = _mm_loadu_si128(reinterpret_cast<const __m128i*>(seed));
			value = _mm_or_si128(
				_mm_slli_epi64(value, STEAM_BONEARRAY_PTR_ROL64),
				_mm_srli_epi64(value, 64 - STEAM_BONEARRAY_PTR_ROL64));
			const __m128i shuffleMask = _mm_setr_epi8(
				5, 0, 4, 6, 7, 2, 3, 1,
				0, 0, 0, 0, 0, 0, 0, 0);
			value = _mm_shuffle_epi8(value, shuffleMask);
			value = _mm_or_si128(
				_mm_slli_epi32(value, STEAM_BONEARRAY_PTR_ROL32),
				_mm_srli_epi32(value, 32 - STEAM_BONEARRAY_PTR_ROL32));
			return static_cast<uint64_t>(_mm_cvtsi128_si64(value));
		}

		inline bool GetBoneArrayPtrSlotsAll(uint64_t Mesh,
			uint64_t* slotsOut, uint32_t* bonesOut, int maxSlots, int& outSelectedIndex)
		{
			outSelectedIndex = 0;
			for (int i = 0; i < maxSlots; ++i) { slotsOut[i] = 0; bonesOut[i] = 0; }
			if (!ctx || !Mesh || !validpointer(Mesh)) return false;

			alignas(16) uint8_t seed[16]{};
			if (!ctx->read_array(Mesh + STEAM_BONEARRAY_ENC_OFFSET, seed, sizeof(seed)))
				return false;

			const uint64_t base_ptr = DecryptBoneArrayBase(seed);
			if (!base_ptr || !validpointer(base_ptr)) return false;

			const uint32_t lod = ctx->read<uint32_t>(Mesh + STEAM_BONEARRAY_LOD_OFFSET);
			const uint32_t sel = (lod >> STEAM_BONEARRAY_LOD_SHIFT) & STEAM_BONEARRAY_LOD_MASK;
			outSelectedIndex = (int)(sel / 0x10u);
			if (outSelectedIndex < 0 || outSelectedIndex >= maxSlots)
				outSelectedIndex = 0;

			// The descriptor holds the array pointer with its count directly behind it,
			// rather than the separate 0x90/0x98 pair the old layout used.
			bool any = false;
			for (int i = 0; i < maxSlots; ++i) {
				const uint64_t slot = (uint64_t)i * 0x10ull;
				const uint64_t descriptor = base_ptr + slot + STEAM_BONEARRAY_DESCRIPTOR_OFFSET;
				if (!validpointer(descriptor))
					continue;
				const uint64_t p = ctx->read<uint64_t>(descriptor);
				const uint32_t count = ctx->read<uint32_t>(descriptor + 8);
				if (p && validpointer(p)) {
					slotsOut[i] = p;
					bonesOut[i] = count;
					any = true;
				}
			}
			return any;
		}

		inline bool GetBoneArrayPtrSlots(uint64_t Mesh,
			uint64_t& outPrimary, uint64_t& outAlt,
			uint32_t& outPrimaryBones, uint32_t& outAltBones)
		{
			uint64_t slots[2]{};
			uint32_t bones[2]{};
			int sel = 0;
			if (!GetBoneArrayPtrSlotsAll(Mesh, slots, bones, 2, sel))
				return false;
			const int other = sel ? 0 : 1;
			outPrimary = slots[sel];
			outAlt = slots[other];
			outPrimaryBones = bones[sel];
			outAltBones = bones[other];
			return outPrimary != 0 || outAlt != 0;
		}

		inline uint64_t GetBoneArrayPtr(uint64_t Mesh) {
			uint64_t primary = 0, alt = 0;
			uint32_t pb = 0, ab = 0;
			if (!GetBoneArrayPtrSlots(Mesh, primary, alt, pb, ab))
				return 0;
			return primary ? primary : alt;
		}

		// There is deliberately no per-bone decrypt here. Bone transforms on this build are
		// plaintext component-space FTransforms: quaternion at +0x00, translation at +0x20,
		// scale at +0x40, 0x60 to a bone. An earlier attempt to decrypt them with a key block
		// at mesh+0xAF8 corrupted the data instead, and the stretched poses that were blamed
		// on it were actually a wrong ComponentToWorld base feeding a garbage scale.

		// Diagnostic-only twin of GetBoneArrayPtr() above: also reads the slot that
		// was NOT selected, so a one-shot log can confirm STEAM_BONEARRAY_LOD_SHIFT/
		// MASK are still picking the live (not being-written) buffer against a
		// known-good entity. TODO: verify against current build with a live dump if
		// [BoneBuffer] logs show readArrayPtr flipping to editArrayPtr's value.
		inline BoneArrayDebugInfo GetBoneArrayDebugInfo(uint64_t Mesh) {
			BoneArrayDebugInfo info{};
			if (!ctx || !Mesh || !validpointer(Mesh)) return info;

			alignas(16) uint8_t seed[16]{};
			if (!ctx->read_array(Mesh + STEAM_BONEARRAY_ENC_OFFSET, seed, sizeof(seed)))
				return info;

			const uint64_t base_ptr = DecryptBoneArrayBase(seed);
			if (!base_ptr || !validpointer(base_ptr)) return info;

			const uint32_t lod = ctx->read<uint32_t>(Mesh + STEAM_BONEARRAY_LOD_OFFSET);
			const uint32_t idx = (lod >> STEAM_BONEARRAY_LOD_SHIFT) & STEAM_BONEARRAY_LOD_MASK;
			const uint32_t otherIdx = (idx == 0) ? 0x10u : 0u;

			info.selectorField = lod;
			info.selectedSlot = idx;
			info.numBones = ctx->read<uint32_t>(
				base_ptr + idx + STEAM_BONEARRAY_DESCRIPTOR_OFFSET + 8);
			info.readArrayPtr = ctx->read<uint64_t>(
				base_ptr + idx + STEAM_BONEARRAY_DESCRIPTOR_OFFSET);
			info.editArrayPtr = ctx->read<uint64_t>(
				base_ptr + otherIdx + STEAM_BONEARRAY_DESCRIPTOR_OFFSET);
			info.valid = true;
			return info;
		}
	}
}
