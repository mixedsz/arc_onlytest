#pragma once
#include "Platform.hpp"
#include "Decryptions.hpp"
#include "Unicorn.hpp"
#include <Windows.h>
#include <intrin.h>
#include <tmmintrin.h>

#ifndef BYTE2
#define BYTE2(x)    ((uint8_t)(((uint32_t)(x) >> 16) & 0xFF))
#endif

#ifndef HIWORD
#define HIWORD(x)   ((uint16_t)(((uint32_t)(x) >> 16) & 0xFFFF))
#endif

#ifndef XBOX_ROR_DEFINED
#define XBOX_ROR_DEFINED
inline uint32_t __ROR4__(uint32_t v, unsigned int c) { c &= 31; return (v >> c) | (v << (32 - c)); }
inline uint64_t __ROR8__(uint64_t v, unsigned int c) { c &= 63; return (v >> c) | (v << (64 - c)); }
#endif

#define XBOX_USTRUCT_SUPERSTRUCT         0x68
#define XBOX_USTRUCT_CHILD_PROPERTIES    0xA8
#define XBOX_USTRUCT_PROPERTIES_SIZE     0xA8
#define XBOX_USTRUCT_CHILDREN            0xA0
#define XBOX_ELEMENT_SIZE                0x90

#define XBOX_FFIELD_NEXT                 0x48
#define XBOX_FFIELD_CLASS                0x68
#define XBOX_FFIELD_NAME                 0x70
#define XBOX_UFIELD_NEXT                 0x50
#define XBOX_UENUM_NAMES                 0x68

#define XBOX_FPROPERTY_OFFSET            0x98
#define XBOX_FPROPERTY_ELEMENT_SIZE      0xA4
#define XBOX_FPROPERTY_ARRAY_DIM         0xA8
#define XBOX_FPROPERTY_STRUCT            0xE0
#define XBOX_FPROPERTY_INNER             0xE0
#define XBOX_FPROPERTY_VALUE             0xE8

#define XBOX_FBOOLPROPERTY_BASE          0x80
#define XBOX_FPROPERTY_PROPERTY_FLAGS    0x40

#define XBOX_FFIELD_NAME_XOR_KEY_RVA     0x91B8BA0
#define XBOX_FFIELD_NAME_SHUFLO_IMM_1    75
#define XBOX_FFIELD_NAME_SHUFLO_IMM_2    57
#define XBOX_FPROPERTY_OFFSET_XOR_CONST  0xF6E2C582u

#define XBOX_UOBJECT_SLOT_STRIDE         16
#define XBOX_UOBJECT_SLOT_BASE_OFFSET    16
#define XBOX_UOBJECT_FNV32_PRIME         16777619u
#define XBOX_UOBJECT_FNV32_SUB1          538958825u
#define XBOX_UOBJECT_FNV32_SUB2          682625555u
#define XBOX_UOBJECT_FNV32_ROR_7         7
#define XBOX_UOBJECT_FNV32_ROR_12        12
#define XBOX_UOBJECT_SLOTBYTE_MULT       ((uint32_t)(-109))
#define XBOX_UOBJECT_SLOTBYTE_SUB        19u
#define XBOX_UOBJECT_SLOT_XOR_KEY_ED0_RVA 0x91B4ED0
#define XBOX_UOBJECT_SLOT_XOR_KEY_EF0_RVA 0x91B4EF0
#define XBOX_UOBJECT_SLOT_SHUFLO_IMM     75

#define XBOX_GOBJECTS_BASE_ENC_RVA          0xC65AD70
#define XBOX_GOBJECTS_BASE_SHUF_KEY_RVA     0x915ECA0
#define XBOX_GOBJECTS_BASE_ROT32_AMOUNT     26
#define XBOX_GOBJECTS_BASE_ROT16_AMOUNT     4
#define XBOX_GOBJECTS_COUNT_OFFSET          0x70
#define XBOX_GOBJECTS_COUNT_XOR_KEY_RVA     0x91B4EC0
#define XBOX_GOBJECTS_COUNT_ROT64_AMOUNT    42
#define XBOX_GOBJECTS_COUNT_SHUFLO_IMM      27
#define XBOX_GOBJECTS_VTABLE_OFFSET         0x30
#define XBOX_GOBJECTS_CTX_OFFSET            0x60

#define XBOX_BONEARRAY_ENC_OFFSET           0x760
#define XBOX_BONEARRAY_LOD_OFFSET           0x790
#define XBOX_BONEARRAY_SHUF_KEY_RVA         0x915ECA0
#define XBOX_BONEARRAY_TARRAY_OFFSET        0xF0
#define XBOX_GOBJECTS_VTABLE_FN_INDEX       32
#define XBOX_GOBJECTS_ITEM_SIZE             24
#define XBOX_GOBJECTS_ITEM_FLAGS_OFFSET     8
#define XBOX_GOBJECTS_ITEM_SERIAL_OFFSET    16
#define XBOX_GOBJECTS_INVALID_FLAGS_MASK    0x30200000u

#define XBOX_FNAME_SHUF_A_RVA               0x919CCE0
#define XBOX_FNAME_SHUF_B_RVA               0x919CCC0
#define XBOX_FNAME_SLOT_XOR_KEY_RVA         0x919CD00
#define XBOX_FNAME_POOL_RVA                 0xC4A02C0
#define XBOX_FNAME_POOL_INIT_FLAG_RVA       0xC4A0057
#define XBOX_FNAME_XOR_TABLE_RVA            0xC1BAC98
#define XBOX_FNAME_CHUNK_STRIDE             128
#define XBOX_FNAME_CHUNK_SLOT_BASE          96
#define XBOX_FNAME_SLOT_STRIDE              16
#define XBOX_FNAME_STAGE_A_ROT1             22
#define XBOX_FNAME_STAGE_A_ROT2             51
#define XBOX_FNAME_STAGE_B_ROT1             13
#define XBOX_FNAME_STAGE_B_ROT2             42
#define XBOX_FNAME_FNV32_PRIME              16777619u
#define XBOX_FNAME_FNV32_ADD1               2103174641u
#define XBOX_FNAME_FNV32_ADD2               1402223851u
#define XBOX_FNAME_FNV32_ROR_6              6
#define XBOX_FNAME_FNV32_ROR_10             10
#define XBOX_FNAME_SLOT_ROT32               12
#define XBOX_FNAME_SLOT_SHUFLO_IMM          114
#define XBOX_FNAME_FNV64_PRIME              0x100000001B3ULL
#define XBOX_FNAME_FNV64_SUB                0x6BC31471FB1C7CAFULL
#define XBOX_FNAME_FNV64_ROR1               25
#define XBOX_FNAME_FNV64_ROR2               19
#define XBOX_FNAME_WIDE_FLAG                0x8000u
#define XBOX_FNAME_LEN_LOW_MASK             0x1FFu
#define XBOX_FNAME_LEN_HIGHBIT_SHIFT        9
#define XBOX_FNAME_KEY_START                27360u
#define XBOX_FNAME_KEY_UPDATE_MULT          27360u
#define XBOX_FNAME_KEY_UPDATE_CONST         8812u
#define XBOX_FNAME_KEY_MASK                 0x3Fu

#define XBOX_UFUNCTION_FLAGS             0xE0
#define XBOX_UFUNCTION_NUMPARMS          0xE4
#define XBOX_UFUNCTION_PARMSSIZE         0xE6
#define XBOX_UFUNCTION_RETURNOFFSET      0xE8
#define XBOX_UFUNCTION_FUNC              0x108



std::wstring GetNameFromIndexW(uint64_t BaseAddress, uint32_t name_index);
uint64_t EmulateVTableCallWithTLS(uint64_t GameBase, uint64_t funcPtr, uint64_t rcx_param, uint64_t rdx_param);
uint64_t EmulateVTableCallWithTLS_R8(uint64_t GameBase, uint64_t funcPtr, uint64_t rcx_raw, uint64_t r8_param_addr);

namespace Platform {
	namespace Xbox {

		inline uint32_t GetNamePrivate(uintptr_t GameBase, uint64_t object);

		inline __m128i FNameStageA_Xbox(__m128i input, __m128i shuf_a) {
			__m128i r1 = _mm_or_si128(
				_mm_slli_epi64(input, XBOX_FNAME_STAGE_A_ROT1),
				_mm_srli_epi64(input, 64 - XBOX_FNAME_STAGE_A_ROT1));
			__m128i s = _mm_shuffle_epi8(r1, shuf_a);
			return _mm_or_si128(
				_mm_slli_epi64(s, XBOX_FNAME_STAGE_A_ROT2),
				_mm_srli_epi64(s, 64 - XBOX_FNAME_STAGE_A_ROT2));
		}

		inline __m128i FNameStageB_Xbox(__m128i input, __m128i shuf_b) {
			__m128i r1 = _mm_or_si128(
				_mm_slli_epi64(input, XBOX_FNAME_STAGE_B_ROT1),
				_mm_srli_epi64(input, 64 - XBOX_FNAME_STAGE_B_ROT1));
			__m128i s = _mm_shuffle_epi8(r1, shuf_b);
			return _mm_or_si128(
				_mm_slli_epi64(s, XBOX_FNAME_STAGE_B_ROT2),
				_mm_srli_epi64(s, 64 - XBOX_FNAME_STAGE_B_ROT2));
		}

		inline uint64_t FNameDecryptSlot_Xbox(__m128i raw, __m128i slot_xor_key) {
			__m128i lo = _mm_move_epi64(raw);
			__m128i r32 = _mm_or_si128(
				_mm_slli_epi32(lo, XBOX_FNAME_SLOT_ROT32),
				_mm_srli_epi32(lo, 32 - XBOX_FNAME_SLOT_ROT32));
			__m128i sh1 = _mm_shufflelo_epi16(r32, XBOX_FNAME_SLOT_SHUFLO_IMM);
			__m128i x = _mm_xor_si128(sh1, slot_xor_key);
			__m128i sh2 = _mm_shufflelo_epi16(x, XBOX_FNAME_SLOT_SHUFLO_IMM);
			return sh2.m128i_u64[0];
		}

		inline uintptr_t RequestGNames(uint64_t BaseAddress, uint32_t name_index) {
			if (!ctx || !BaseAddress) return 0;

			uint8_t pool_ready = ctx->read<uint8_t>(BaseAddress + XBOX_FNAME_POOL_INIT_FLAG_RVA);
			if (!pool_ready) return 0;

			__m128i shuf_a = ctx->read<__m128i>(BaseAddress + XBOX_FNAME_SHUF_A_RVA);
			__m128i shuf_b = ctx->read<__m128i>(BaseAddress + XBOX_FNAME_SHUF_B_RVA);
			__m128i slot_xor_key = ctx->read<__m128i>(BaseAddress + XBOX_FNAME_SLOT_XOR_KEY_RVA);

			__m128i stage1_in = _mm_cvtsi32_si128((int)name_index);
			__m128i stage1_out = FNameStageA_Xbox(stage1_in, shuf_a);

			int32_t stage2_lo32 = _mm_cvtsi128_si32(FNameStageB_Xbox(stage1_out, shuf_b));

			__m128i stage3_in = _mm_cvtsi32_si128(stage2_lo32);
			__m128i stage3_out = FNameStageA_Xbox(stage3_in, shuf_a);

			uint32_t hash32 = (uint32_t)_mm_cvtsi128_si32(FNameStageB_Xbox(stage3_out, shuf_b));

			uint32_t v22 = hash32 & 0xFFFFu;
			uint32_t chunk_idx = (hash32 >> 16) & 0xFFFFu;

			uint64_t chunk_ptr = BaseAddress + XBOX_FNAME_POOL_RVA + (uint64_t)XBOX_FNAME_CHUNK_STRIDE * chunk_idx;
			uint64_t slot_addr = chunk_ptr + XBOX_FNAME_CHUNK_SLOT_BASE;

			uint32_t addr_lo = (uint32_t)slot_addr;
			uint32_t addr_hi = (uint32_t)(slot_addr >> 32);
			const uint32_t P = XBOX_FNAME_FNV32_PRIME;
			const uint32_t A1 = XBOX_FNAME_FNV32_ADD1;
			const uint32_t A2 = XBOX_FNAME_FNV32_ADD2;

			uint32_t L1 = __ROR4__(addr_lo, XBOX_FNAME_FNV32_ROR_6);
			uint32_t L2 = P * L1 + A1;
			uint32_t L3 = __ROR4__(L2, XBOX_FNAME_FNV32_ROR_10);
			uint32_t L4 = P * L3 + A1 + addr_hi;
			uint32_t L5 = __ROR4__(L4, XBOX_FNAME_FNV32_ROR_6);
			uint32_t L6 = L5 + A2;
			uint32_t L7 = P * L6;
			uint32_t L8 = __ROR4__(L7, XBOX_FNAME_FNV32_ROR_10);
			uint32_t v15 = L8 + A2;

			uint32_t mix = P * v15;
			uint32_t slot0_idx = (mix ^ (mix >> 16)) & 7u;
			uint32_t slot1_idx = (slot0_idx + 1u) & 7u;

			__m128i slot0_raw = ctx->read<__m128i>(slot_addr + (uint64_t)XBOX_FNAME_SLOT_STRIDE * slot0_idx);
			__m128i slot1_raw = ctx->read<__m128i>(slot_addr + (uint64_t)XBOX_FNAME_SLOT_STRIDE * slot1_idx);

			uint64_t slot0_val = FNameDecryptSlot_Xbox(slot0_raw, slot_xor_key);
			uint64_t slot1_val = FNameDecryptSlot_Xbox(slot1_raw, slot_xor_key);

			uint64_t t1 = XBOX_FNAME_FNV64_PRIME * __ROR8__(slot0_val, XBOX_FNAME_FNV64_ROR1) - XBOX_FNAME_FNV64_SUB;
			uint64_t t2 = XBOX_FNAME_FNV64_PRIME * __ROR8__(t1, XBOX_FNAME_FNV64_ROR2) - XBOX_FNAME_FNV64_SUB;

			uint64_t entry_ptr = slot0_val + (slot1_val ^ t2) + 2ULL * v22;
			return (uintptr_t)entry_ptr;
		}

		inline bool GetNameFromIndex(uint64_t BaseAddress, uint32_t name_index, char* out_name, size_t max_len) {
			if (!ctx || !BaseAddress || !out_name || max_len == 0) return false;

			uintptr_t name_entry_ptr = RequestGNames(BaseAddress, name_index);
			if (!name_entry_ptr || name_entry_ptr < 0x10000) return false;

			uint16_t header = ctx->read<uint16_t>(name_entry_ptr);
			if (header == 0) return false;

			bool     is_wide = (header & XBOX_FNAME_WIDE_FLAG) != 0;
			uint32_t str_len = (2u * ((uint32_t)header & XBOX_FNAME_LEN_LOW_MASK))
				| (((uint32_t)header >> XBOX_FNAME_LEN_HIGHBIT_SHIFT) & 1u);
			if (str_len == 0 || str_len > 1024) return false;

			uint16_t xor_table[64];
			if (!ctx->read_array(BaseAddress + XBOX_FNAME_XOR_TABLE_RVA, xor_table, sizeof(xor_table))) {
				return false;
			}

			if (is_wide) {
				if (str_len >= max_len) return false;

				uint16_t wide_name[1025];
				if (!ctx->read_array(name_entry_ptr + 2, wide_name, 2u * str_len)) {
					return false;
				}

				uint32_t key = str_len + XBOX_FNAME_KEY_START;
				for (uint32_t i = 0; i < str_len; i++) {
					wide_name[i] ^= xor_table[key & XBOX_FNAME_KEY_MASK];
					key = XBOX_FNAME_KEY_UPDATE_CONST - XBOX_FNAME_KEY_UPDATE_MULT * key;
				}

				wide_name[str_len] = 0;
				int converted = WideCharToMultiByte(CP_UTF8, 0, (const wchar_t*)wide_name, (int)str_len,
					out_name, (int)(max_len - 1), NULL, NULL);
				if (converted <= 0) return false;
				out_name[converted] = '\0';
			}
			else {
				if (str_len >= max_len) return false;

				uint8_t encrypted_data[1025];
				if (!ctx->read_array(name_entry_ptr + 2, encrypted_data, str_len)) {
					return false;
				}

				uint32_t key = str_len + XBOX_FNAME_KEY_START;
				for (uint32_t i = 0; i < str_len; i++) {
					encrypted_data[i] ^= (uint8_t)(xor_table[key & XBOX_FNAME_KEY_MASK] >> 3);
					key = XBOX_FNAME_KEY_UPDATE_CONST - XBOX_FNAME_KEY_UPDATE_MULT * key;
				}

				memcpy(out_name, encrypted_data, str_len);
				out_name[str_len] = '\0';
			}

			return strlen(out_name) > 0;
		}


		inline uint32_t UObjectComputeHash_Xbox(uintptr_t a1) {
			uint64_t addr = (uint64_t)(a1 + 16);
			uint32_t lo = (uint32_t)addr;
			uint32_t hi = (uint32_t)(addr >> 32);
			const uint32_t P = XBOX_UOBJECT_FNV32_PRIME;
			const uint32_t S1 = XBOX_UOBJECT_FNV32_SUB1;
			const uint32_t S2 = XBOX_UOBJECT_FNV32_SUB2;

			uint32_t L1 = __ROR4__(lo, XBOX_UOBJECT_FNV32_ROR_7);
			uint32_t L2 = P * L1 - S1;
			uint32_t L3 = __ROR4__(L2, XBOX_UOBJECT_FNV32_ROR_12);
			uint32_t L4 = P * L3 - S1 + hi;
			uint32_t L5 = __ROR4__(L4, XBOX_UOBJECT_FNV32_ROR_7);
			uint32_t L7 = P * (L5 - S2);
			return __ROR4__(L7, XBOX_UOBJECT_FNV32_ROR_12);
		}

		inline uint32_t UObjectSlotIndex_Xbox(uintptr_t a1) {
			uint32_t v3 = UObjectComputeHash_Xbox(a1);
			uint32_t A_val = XBOX_UOBJECT_SLOTBYTE_MULT * (v3 - XBOX_UOBJECT_SLOTBYTE_SUB);
			uint32_t B_val = XBOX_UOBJECT_FNV32_PRIME * (v3 - XBOX_UOBJECT_FNV32_SUB2);
			uint8_t  sb = (uint8_t)A_val ^ (uint8_t)(B_val >> 16);
			return (uint32_t)(sb & 3u);
		}

		inline uint64_t UObjectDecryptSlot_Xbox(uintptr_t GameBase, uint64_t object, uint32_t slot) {
			uint64_t addr = object + (uint64_t)XBOX_UOBJECT_SLOT_STRIDE * slot + XBOX_UOBJECT_SLOT_BASE_OFFSET;

			__m128i raw = ctx->read<__m128i>(addr);
			__m128i K_ED0 = ctx->read<__m128i>(GameBase + XBOX_UOBJECT_SLOT_XOR_KEY_ED0_RVA);
			__m128i K_EF0 = ctx->read<__m128i>(GameBase + XBOX_UOBJECT_SLOT_XOR_KEY_EF0_RVA);

			__m128i loaded = _mm_move_epi64(raw);
			__m128i xored1 = _mm_xor_si128(loaded, K_ED0);
			__m128i shuf = _mm_shufflelo_epi16(xored1, XBOX_UOBJECT_SLOT_SHUFLO_IMM);
			__m128i result = _mm_xor_si128(shuf, K_EF0);
			return result.m128i_u64[0];
		}

		inline uintptr_t ClassPrivate(uintptr_t GameBase, uint64_t object) {
			if (!ctx || !object || !GameBase) return 0;
			uint32_t s = UObjectSlotIndex_Xbox((uintptr_t)object);
			return (uintptr_t)UObjectDecryptSlot_Xbox(GameBase, object, s);
		}

		inline uint64_t OuterPrivate(uintptr_t GameBase, uint64_t object) {
			if (!ctx || !object || !GameBase) return 0;
			uint32_t s = UObjectSlotIndex_Xbox((uintptr_t)object);
			uint32_t outer_slot = (s + 1u) & 3u;
			return UObjectDecryptSlot_Xbox(GameBase, object, outer_slot);
		}

		inline uint32_t GetNamePrivate(uintptr_t GameBase, uint64_t object) {
			if (!ctx || !object || !GameBase) return 0;
			uint32_t s = UObjectSlotIndex_Xbox((uintptr_t)object);
			uint32_t name_slot = (s + 2u) & 3u;
			uint64_t dec = UObjectDecryptSlot_Xbox(GameBase, object, name_slot);
			return (uint32_t)(dec >> 32);
		}

		inline int32_t GetFFieldOffset(uintptr_t ffield_ptr) {
			if (!ctx || !ffield_ptr) return 0;
			uint32_t encrypted = ctx->read<uint32_t>(ffield_ptr + XBOX_FPROPERTY_OFFSET);
			if (encrypted == 0) return 0;
			return static_cast<int32_t>(_byteswap_ulong(encrypted) ^ XBOX_FPROPERTY_OFFSET_XOR_CONST);
		}

		inline std::wstring GetFFieldName(uintptr_t GameBase, uint64_t ffield_ptr) {
			if (!ctx || !GameBase || !ffield_ptr) return L"";

			__m128i xor_key = ctx->read<__m128i>(GameBase + XBOX_FFIELD_NAME_XOR_KEY_RVA);
			__m128i raw = ctx->read<__m128i>(ffield_ptr + XBOX_FFIELD_NAME);

			__m128i loaded = _mm_move_epi64(raw);
			__m128i shuf1 = _mm_shufflelo_epi16(loaded, XBOX_FFIELD_NAME_SHUFLO_IMM_1);
			__m128i xored = _mm_xor_si128(shuf1, xor_key);
			__m128i shuf2 = _mm_shufflelo_epi16(xored, XBOX_FFIELD_NAME_SHUFLO_IMM_2);
			uint64_t v38 = shuf2.m128i_u64[0];

			uint32_t name_idx = (uint32_t)(v38 >> 32);
			if (name_idx == 0 || name_idx == 0xFFFFFFFF) return L"";

			return GetNameFromIndexW(GameBase, name_idx);
		}
		inline uint64_t DecryptGObjectsBasePtr(uint64_t GameBase) {
			if (!ctx || !GameBase) return 0;

			__m128i enc = ctx->read<__m128i>(GameBase + XBOX_GOBJECTS_BASE_ENC_RVA);
			__m128i shuf_key = ctx->read<__m128i>(GameBase + XBOX_GOBJECTS_BASE_SHUF_KEY_RVA);

			__m128i v5 = _mm_move_epi64(enc);
			__m128i r32 = _mm_or_si128(
				_mm_slli_epi32(v5, XBOX_GOBJECTS_BASE_ROT32_AMOUNT),
				_mm_srli_epi32(v5, 32 - XBOX_GOBJECTS_BASE_ROT32_AMOUNT));
			__m128i v6 = _mm_shuffle_epi8(r32, shuf_key);
			__m128i r16 = _mm_or_si128(
				_mm_slli_epi16(v6, XBOX_GOBJECTS_BASE_ROT16_AMOUNT),
				_mm_srli_epi16(v6, 16 - XBOX_GOBJECTS_BASE_ROT16_AMOUNT));

			return r16.m128i_u64[0];
		}

		inline uint32_t GetGObjectsCount(uint64_t GameBase) {
			if (!ctx || !GameBase) return 0;

			uint64_t base_ptr = DecryptGObjectsBasePtr(GameBase);
			if (!base_ptr) return 0;

			__m128i enc = ctx->read<__m128i>(base_ptr + XBOX_GOBJECTS_COUNT_OFFSET);
			__m128i xor_key = ctx->read<__m128i>(GameBase + XBOX_GOBJECTS_COUNT_XOR_KEY_RVA);

			__m128i loaded = _mm_move_epi64(enc);
			__m128i v7 = _mm_xor_si128(loaded, xor_key);
			__m128i r64 = _mm_or_si128(
				_mm_slli_epi64(v7, XBOX_GOBJECTS_COUNT_ROT64_AMOUNT),
				_mm_srli_epi64(v7, 64 - XBOX_GOBJECTS_COUNT_ROT64_AMOUNT));
			__m128i shuflo = _mm_shufflelo_epi16(r64, XBOX_GOBJECTS_COUNT_SHUFLO_IMM);
			int32_t count = _mm_cvtsi128_si32(shuflo);

			if (count <= 0 || count > 20000000) return 0;
			return (uint32_t)count;
		}

		inline uint64_t GetGObjectsChunksPtr(uint64_t GameBase) {
			if (!ctx || !GameBase) return 0;

			uint64_t base_ptr = DecryptGObjectsBasePtr(GameBase);
			if (!base_ptr) return 0;

			uint64_t vtable = ctx->read<uint64_t>(base_ptr + XBOX_GOBJECTS_VTABLE_OFFSET);
			if (!vtable) return 0;
			uint64_t funcPtr = ctx->read<uint64_t>(vtable + XBOX_GOBJECTS_VTABLE_FN_INDEX);
			if (!funcPtr) return 0;

			return EmulateVTableCallWithTLS_R8(GameBase, funcPtr,
				base_ptr + XBOX_GOBJECTS_VTABLE_OFFSET,
				base_ptr + XBOX_GOBJECTS_CTX_OFFSET);
		}

		inline uint64_t GetObjectFromGObjects(uint64_t GameBase, uint64_t chunksPtr, int32_t objectIndex) {
			(void)GameBase;
			if (!chunksPtr || objectIndex < 0) return 0;

			uint32_t chunkIndex = ((uint32_t)objectIndex >> 16) & 0xFFFF;
			uint32_t elementIndex = (uint32_t)objectIndex & 0xFFFF;

			uint64_t chunkPtr = ctx->read<uint64_t>(chunksPtr + 8ULL * chunkIndex);
			if (!chunkPtr) return 0;

			uint64_t itemAddr = chunkPtr + (uint64_t)XBOX_GOBJECTS_ITEM_SIZE * elementIndex;

			uint32_t flags = ctx->read<uint32_t>(itemAddr + XBOX_GOBJECTS_ITEM_FLAGS_OFFSET);
			if ((flags & XBOX_GOBJECTS_INVALID_FLAGS_MASK) != 0) return 0;

			uint64_t objPtr = ctx->read<uint64_t>(itemAddr);
			if (!objPtr) return 0;

			return objPtr;
		}

		inline uint64_t GetObjectFromGObjectsWithSerial(uint64_t GameBase, uint64_t chunksPtr, int32_t objectIndex, int32_t serial) {
			(void)GameBase;
			if (!serial || !chunksPtr || objectIndex < 0) return 0;

			uint32_t chunkIndex = ((uint32_t)objectIndex >> 16) & 0xFFFF;
			uint32_t elementIndex = (uint32_t)objectIndex & 0xFFFF;

			uint64_t chunkPtr = ctx->read<uint64_t>(chunksPtr + 8ULL * chunkIndex);
			if (!chunkPtr) return 0;

			uint64_t itemAddr = chunkPtr + (uint64_t)XBOX_GOBJECTS_ITEM_SIZE * elementIndex;

			int32_t itemSerial = ctx->read<int32_t>(itemAddr + XBOX_GOBJECTS_ITEM_SERIAL_OFFSET);
			if (itemSerial != serial) return 0;

			uint32_t flags = ctx->read<uint32_t>(itemAddr + XBOX_GOBJECTS_ITEM_FLAGS_OFFSET);
			if ((flags & XBOX_GOBJECTS_INVALID_FLAGS_MASK) != 0) return 0;

			uint64_t objPtr = ctx->read<uint64_t>(itemAddr);
			if (!objPtr) return 0;

			return objPtr;
		}

		inline void DecodeInPlace(uint16_t* s, int32_t maxLenChars) {
			if (!s || maxLenChars <= 0 || !ctx)
				return;

			if (maxLenChars - 1 <= 0)
				return;
			uint32_t v8 = 0;
			uint16_t* v6 = s;

			for (uint16_t i = *v6; *v6; i = *v6)
			{
				v8 = 16777619 * (v8 + __ROR4__(16777619 * v8 + 260052961, 5));

				uint32_t v10 = (uint32_t)i ^ (v8 & 0x1F);

				uint32_t v11 = ((uint32_t)(v10 - 33) >= 0x2F)
					? (((uint32_t)(v10 - 80) < 0x2F) ? 0xFFFFFFD1u : 0u)
					: 47u;
				uint32_t v12 = v11 + v10;

				uint32_t v13 = ((uint32_t)(v12 - 48) >= 5)
					? (((uint32_t)(v12 - 53) < 5) ? 0xFFFFFFFBu : 0u)
					: 5u;
				uint32_t v14 = v13 + v12;

				uint32_t v15 = ((uint32_t)(v14 - 97) >= 0xD)
					? (((uint32_t)(v14 - 110) < 0xD) ? 0xFFFFFFF3u : 0u)
					: 13u;
				uint32_t v16 = v15 + v14;

				uint32_t v17 = ((uint32_t)(v16 - 65) >= 0xD)
					? (((uint32_t)(v16 - 78) < 0xD) ? 0xFFFFFFF3u : 0u)
					: 13u;
				uint32_t v18 = v17 + v16;

				uint32_t v19 = ((uint32_t)(v18 - 33) >= 0x2F)
					? (((uint32_t)(v18 - 80) < 0x2F) ? 0xFFFFFFD1u : 0u)
					: 47u;

				*v6++ = (uint16_t)(v18 + v19);
			}
		}

		// Both halves of the double buffer; see the Steam twin for why the selector is not
		// trusted on its own. Xbox exposes no bone-count field at this offset, so the
		// counts come back zero and the caller falls back to scoring on geometry alone.
		// See the Steam twin: the selector names only two slots but the array steps by 0x10
		// across every LOD, and a pawn on a deeper one is unreachable otherwise.
		inline bool GetBoneArrayPtrSlotsAll(uint64_t Mesh,
			uint64_t* slotsOut, uint32_t* bonesOut, int maxSlots, int& outSelectedIndex)
		{
			outSelectedIndex = 0;
			for (int i = 0; i < maxSlots; ++i) { slotsOut[i] = 0; bonesOut[i] = 0; }
			if (!ctx || !Mesh || !validpointer(Mesh) || !ctx->image_base) return false;

			uint64_t rawEnc = ctx->read<uint64_t>(Mesh + XBOX_BONEARRAY_ENC_OFFSET);
			if (!rawEnc) return false;

			__m128i vv = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&rawEnc));
			__m128i shufKey = ctx->read<__m128i>(ctx->image_base + XBOX_BONEARRAY_SHUF_KEY_RVA);
			__m128i rr32 = _mm_castps_si128(_mm_xor_ps(
				_mm_castsi128_ps(_mm_slli_epi32(vv, 0x1Au)),
				_mm_castsi128_ps(_mm_srli_epi32(vv, 0x6u))));
			__m128i rr16 = _mm_shuffle_epi8(rr32, shufKey);

			uint64_t basePtr = static_cast<uint64_t>(_mm_cvtsi128_si64(rr16));
			if (!basePtr || !validpointer(basePtr)) return false;

			uint32_t lodField = ctx->read<uint32_t>(Mesh + XBOX_BONEARRAY_LOD_OFFSET);
			outSelectedIndex = (int)((lodField >> 30) & 1u);

			bool any = false;
			for (int i = 0; i < maxSlots; ++i) {
				const uint64_t slot = (uint64_t)i * 0x10ull;
				uint64_t p = ctx->read<uint64_t>(basePtr + XBOX_BONEARRAY_TARRAY_OFFSET + slot);
				if (p && validpointer(p)) {
					slotsOut[i] = p;
					bonesOut[i] = 0; // no count field observed at this offset on Xbox
					any = true;
				}
			}
			return any;
		}

		inline bool GetBoneArrayPtrSlots(uint64_t Mesh,
			uint64_t& outPrimary, uint64_t& outAlt,
			uint32_t& outPrimaryBones, uint32_t& outAltBones)
		{
			outPrimary = outAlt = 0;
			outPrimaryBones = outAltBones = 0;
			if (!ctx || !Mesh || !validpointer(Mesh) || !ctx->image_base) return false;

			uint64_t rawEnc = ctx->read<uint64_t>(Mesh + XBOX_BONEARRAY_ENC_OFFSET);
			if (!rawEnc) return false;

			__m128i vv = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&rawEnc));
			__m128i shufKey = ctx->read<__m128i>(ctx->image_base + XBOX_BONEARRAY_SHUF_KEY_RVA);

			__m128i rr32 = _mm_castps_si128(_mm_xor_ps(
				_mm_castsi128_ps(_mm_slli_epi32(vv, 0x1Au)),
				_mm_castsi128_ps(_mm_srli_epi32(vv, 0x6u))));
			__m128i rr16 = _mm_shuffle_epi8(rr32, shufKey);

			uint64_t basePtr = static_cast<uint64_t>(_mm_cvtsi128_si64(rr16));
			if (!basePtr || !validpointer(basePtr)) return false;

			uint32_t lodField = ctx->read<uint32_t>(Mesh + XBOX_BONEARRAY_LOD_OFFSET);
			uint64_t slot = (((lodField >> 30) & 1u) << 4);
			uint64_t otherSlot = (slot == 0) ? 0x10ull : 0ull;

			uint64_t p = ctx->read<uint64_t>(basePtr + XBOX_BONEARRAY_TARRAY_OFFSET + slot);
			uint64_t a = ctx->read<uint64_t>(basePtr + XBOX_BONEARRAY_TARRAY_OFFSET + otherSlot);
			if (p && validpointer(p)) outPrimary = p;
			if (a && validpointer(a)) outAlt = a;
			return outPrimary != 0 || outAlt != 0;
		}

		inline uint64_t GetBoneArrayPtr(uint64_t Mesh) {
			if (!ctx || !Mesh || !validpointer(Mesh) || !ctx->image_base) return 0;

			uint64_t raw64 = ctx->read<uint64_t>(Mesh + XBOX_BONEARRAY_ENC_OFFSET);
			if (!raw64) return 0;

			__m128i v = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&raw64));
			__m128i shuf_key = ctx->read<__m128i>(ctx->image_base + XBOX_BONEARRAY_SHUF_KEY_RVA);

			__m128i r32 = _mm_castps_si128(_mm_xor_ps(
				_mm_castsi128_ps(_mm_slli_epi32(v, 0x1Au)),
				_mm_castsi128_ps(_mm_srli_epi32(v, 6u))));
			__m128i shuf = _mm_shuffle_epi8(r32, shuf_key);
			__m128i r16 = _mm_castps_si128(_mm_xor_ps(
				_mm_castsi128_ps(_mm_slli_epi16(shuf, 4)),
				_mm_castsi128_ps(_mm_srli_epi16(shuf, 0xCu))));

			uint64_t basePtr = static_cast<uint64_t>(_mm_cvtsi128_si64(r16));
			if (!basePtr || !validpointer(basePtr)) return 0;

			uint32_t lodField = ctx->read<uint32_t>(Mesh + XBOX_BONEARRAY_LOD_OFFSET);
			uint64_t slot = (((lodField >> 30) & 1u) << 4);

			uint64_t boneData = ctx->read<uint64_t>(basePtr + XBOX_BONEARRAY_TARRAY_OFFSET + slot);
			if (!boneData || !validpointer(boneData)) return 0;

			return boneData;
		}

		// Diagnostic-only twin of GetBoneArrayPtr() above: also reads the slot that
		// was NOT selected, so a one-shot log can confirm bit 30 of XBOX_BONEARRAY_LOD_OFFSET
		// is still the live-buffer selector against a known-good entity. TODO: verify
		// against current build with a live dump if [BoneBuffer] logs show readArrayPtr
		// flipping to editArrayPtr's value.
		inline BoneArrayDebugInfo GetBoneArrayDebugInfo(uint64_t Mesh) {
			BoneArrayDebugInfo info{};
			if (!ctx || !Mesh || !validpointer(Mesh) || !ctx->image_base) return info;

			uint64_t raw64 = ctx->read<uint64_t>(Mesh + XBOX_BONEARRAY_ENC_OFFSET);
			if (!raw64) return info;

			__m128i v = _mm_loadl_epi64(reinterpret_cast<const __m128i*>(&raw64));
			__m128i shuf_key = ctx->read<__m128i>(ctx->image_base + XBOX_BONEARRAY_SHUF_KEY_RVA);

			__m128i r32 = _mm_castps_si128(_mm_xor_ps(
				_mm_castsi128_ps(_mm_slli_epi32(v, 0x1Au)),
				_mm_castsi128_ps(_mm_srli_epi32(v, 6u))));
			__m128i shuf = _mm_shuffle_epi8(r32, shuf_key);
			__m128i r16 = _mm_castps_si128(_mm_xor_ps(
				_mm_castsi128_ps(_mm_slli_epi16(shuf, 4)),
				_mm_castsi128_ps(_mm_srli_epi16(shuf, 0xCu))));

			uint64_t basePtr = static_cast<uint64_t>(_mm_cvtsi128_si64(r16));
			if (!basePtr || !validpointer(basePtr)) return info;

			uint32_t lodField = ctx->read<uint32_t>(Mesh + XBOX_BONEARRAY_LOD_OFFSET);
			uint64_t slot = (((lodField >> 30) & 1u) << 4);
			uint64_t otherSlot = (slot == 0) ? 0x10ull : 0ull;

			info.selectorField = lodField;
			info.selectedSlot = static_cast<uint32_t>(slot);
			info.readArrayPtr = ctx->read<uint64_t>(basePtr + XBOX_BONEARRAY_TARRAY_OFFSET + slot);
			info.editArrayPtr = ctx->read<uint64_t>(basePtr + XBOX_BONEARRAY_TARRAY_OFFSET + otherSlot);
			info.numBones = 0; // no dedicated count field observed at this offset on Xbox
			info.valid = true;
			return info;
		}
	}
}

