#include "GObjectsRecovery.hpp"

#include "Offsets.hpp"
#include "SDK.hpp"
#include "Decryptions.hpp"
#include "memory/interface/ctx.hpp"

#include <emmintrin.h>
#include <cstdio>
#include <cstring>
#include <vector>

namespace GObjectsRecovery {

namespace {

// Pipeline inputs differ per platform only in which macro block they come
// from; the shape is identical, so the search runs over one description.
struct PipelineSpec {
    uint64_t encRva;
    uint64_t xorKeyRva;
    int rot64;
    int shufloImm;
    uint64_t vtableOffset;
    uint64_t vtableFnIndex;
};

// Steam only. Xbox decrypts the base with a different pipeline entirely -
// rotate-32, PSHUFB, rotate-16, and no XOR stage - so searching it means a
// separate implementation with its own unknowns. Adding one blind, against a
// platform there is no live capture from, would produce a scanner nobody can
// tell is working.
PipelineSpec CurrentSpec() {
    return { STEAM_GOBJECTS_BASE_ENC_RVA, STEAM_GOBJECTS_BASE_XOR_KEY_RVA,
             STEAM_GOBJECTS_BASE_ROT64_AMOUNT, STEAM_GOBJECTS_BASE_SHUFLO_IMM,
             STEAM_GOBJECTS_VTABLE_OFFSET, STEAM_GOBJECTS_VTABLE_FN_INDEX };
}

// The decrypt, evaluated on locally held bytes rather than through the
// transport. Both inputs are 16 bytes; only the low 64 bits of the result are
// used, matching DecryptGObjectsBasePtr.
inline uint64_t Decrypt(const uint8_t* enc16, const uint8_t* key16, int rot, int shufloImm) {
    if (rot <= 0 || rot >= 64)
        return 0;

    __m128i enc = _mm_loadu_si128(reinterpret_cast<const __m128i*>(enc16));
    __m128i key = _mm_loadu_si128(reinterpret_cast<const __m128i*>(key16));
    __m128i xored = _mm_xor_si128(enc, key);
    __m128i r64 = _mm_or_si128(_mm_slli_epi64(xored, rot), _mm_srli_epi64(xored, 64 - rot));

    // _mm_shufflelo_epi16 needs a compile-time immediate, so the runtime value
    // is applied as an equivalent word permutation on the low half.
    alignas(16) uint16_t w[8];
    _mm_store_si128(reinterpret_cast<__m128i*>(w), r64);
    const uint16_t lo[4] = { w[0], w[1], w[2], w[3] };
    uint16_t out[4];
    for (int i = 0; i < 4; ++i)
        out[i] = lo[(shufloImm >> (i * 2)) & 0x3];

    uint64_t result = 0;
    std::memcpy(&result, out, sizeof(out));
    return result;
}

uint64_t g_imageEnd = 0;

inline bool InModule(uint64_t p, uint64_t imageBase) {
    return g_imageEnd && p >= imageBase && p < g_imageEnd;
}

// A pointer worth spending a round trip on. GUObjectArray is a heap
// allocation, so anything inside the module or obviously non-canonical is
// rejected without touching the game.
//
// The 4 GB floor matters more than it looks. This game's heap sits around
// 0x11'0000'0000 and up - UWorld, PersistentLevel and the camera manager all
// land there - so anything in the low tens of megabytes cannot be a live
// allocation. Without this floor the sweep produced ~165k probe candidates and
// eventually hit a structural false positive at 0x13E1760.
inline bool PlausibleHeapPtr(uint64_t p, uint64_t imageBase) {
    if (p < 0x100000000ULL || p > 0x7FFFFFFFFFFFULL)
        return false;
    if (p & 0x7ULL)
        return false;
    if (InModule(p, imageBase))
        return false;
    return true;
}

// Structural test for a candidate GUObjectArray.
//
// The vtable checks alone are not enough. Across a full-image sweep the
// scanner probes six figures' worth of candidates, and at that volume "two
// consecutive reads happen to land in the module" comes up by chance - it did,
// at 0x13E1760. So a candidate only passes if the object array itself
// dereferences: the chunks pointer must resolve, chunk zero must be a live
// allocation, and the first object in it must carry its own module vtable.
// That is four dependent dereferences deep, which noise does not survive.
bool ValidateBase(uint64_t base, uint64_t imageBase, const PipelineSpec& spec, uint64_t& outVtable) {
    outVtable = 0;
    if (!is_valid_pointer_optimized(base))
        return false;

    auto [vt, vtStatus] = ctx->read_status<uint64_t>(base + spec.vtableOffset);
    if (vtStatus != TransportStatus::Ok || !InModule(vt, imageBase))
        return false;

    auto [fn, fnStatus] = ctx->read_status<uint64_t>(vt + spec.vtableFnIndex);
    if (fnStatus != TransportStatus::Ok || !InModule(fn, imageBase))
        return false;

    // ObjObjects is reached through the emulated vtable getter, the same path
    // the live code uses, so a pass here means GObjects genuinely works.
    const uint64_t chunks = EmulateVTableCallWithTLS(
        imageBase, fn, base + spec.vtableOffset, base + STEAM_GOBJECTS_CTX_OFFSET);
    if (!PlausibleHeapPtr(chunks, imageBase))
        return false;

    auto [chunk0, c0Status] = ctx->read_status<uint64_t>(chunks);
    if (c0Status != TransportStatus::Ok || !PlausibleHeapPtr(chunk0, imageBase))
        return false;

    auto [obj0, o0Status] = ctx->read_status<uint64_t>(chunk0);
    if (o0Status != TransportStatus::Ok || !PlausibleHeapPtr(obj0, imageBase))
        return false;

    auto [objVt, ovStatus] = ctx->read_status<uint64_t>(obj0);
    if (ovStatus != TransportStatus::Ok || !InModule(objVt, imageBase))
        return false;

    outVtable = vt;
    return true;
}

// Pulls an image range into a local buffer. Blocks that fail to read are
// zero-filled and simply produce no candidates.
bool ReadWindow(uint64_t addr, std::vector<uint8_t>& buf, size_t size) {
    buf.assign(size, 0);
    constexpr size_t kBlock = 0x10000;
    bool any = false;
    for (size_t off = 0; off < size; off += kBlock) {
        const size_t n = (size - off < kBlock) ? (size - off) : kBlock;
        if (ctx->read_array(addr + off, buf.data() + off, n))
            any = true;
    }
    return any;
}

} // namespace

Result Run(uint64_t imageBase) {
    Result r{};
    if (!ctx || !imageBase)
        return r;

    if (g_PlatformVersion == PlatformVersion::Xbox) {
        std::snprintf(r.note, sizeof(r.note), "xbox base decrypt has a different shape; scanner is steam-only");
        return r;
    }

    const PipelineSpec spec = CurrentSpec();

    uint64_t moduleEnd = 0;
    Platform::Steam::GetModuleBounds(imageBase, moduleEnd);
    g_imageEnd = moduleEnd ? moduleEnd : (imageBase + 0x11800000ULL);
    const uint64_t imageSize = g_imageEnd - imageBase;

    auto tryCandidate = [&](uint64_t base, uint64_t encRva, uint64_t keyRva, int rot, int shuf, int tier) -> bool {
        r.probesValidated++;
        uint64_t vt = 0;
        if (!ValidateBase(base, imageBase, spec, vt))
            return false;
        r.found = true;
        r.encRva = encRva;
        r.xorKeyRva = keyRva;
        r.rot64 = rot;
        r.shufloImm = shuf;
        r.basePtr = base;
        r.vtable = vt;
        r.tier = tier;
        return true;
    };

    // Tier 0 - the constants already in the table. Cheap, and it keeps this
    // from reporting a "recovery" on a build where nothing was broken.
    {
        uint8_t enc[16] = {}, key[16] = {};
        if (ctx->read_array(imageBase + spec.encRva, enc, sizeof(enc)) &&
            ctx->read_array(imageBase + spec.xorKeyRva, key, sizeof(key))) {
            r.candidatesTested++;
            const uint64_t base = Decrypt(enc, key, spec.rot64, spec.shufloImm);
            if (PlausibleHeapPtr(base, imageBase) &&
                tryCandidate(base, spec.encRva, spec.xorKeyRva, spec.rot64, spec.shufloImm, 0)) {
                std::snprintf(r.note, sizeof(r.note), "table constants still valid");
                return r;
            }
        }
    }

    // Tier 1 - both constants shifted by the same delta. A rebuild moves image
    // globals as a block far more often than it moves two of them apart, so
    // this one-dimensional sweep is tried before anything wider.
    {
        constexpr int64_t kSpan = 0x400000; // +/- 4 MB
        const uint64_t encLo = (spec.encRva > (uint64_t)kSpan) ? (spec.encRva - kSpan) : 0;
        const uint64_t keyLo = (spec.xorKeyRva > (uint64_t)kSpan) ? (spec.xorKeyRva - kSpan) : 0;
        const size_t span = (size_t)kSpan * 2;

        if (encLo + span < imageSize && keyLo + span < imageSize) {
            std::vector<uint8_t> encBuf, keyBuf;
            if (ReadWindow(imageBase + encLo, encBuf, span) &&
                ReadWindow(imageBase + keyLo, keyBuf, span)) {
                const int64_t deltaLo = -(int64_t)(spec.encRva - encLo);
                for (int64_t d = deltaLo; d + 16 <= (int64_t)span + deltaLo; d += 16) {
                    const int64_t ei = (int64_t)(spec.encRva - encLo) + d;
                    const int64_t ki = (int64_t)(spec.xorKeyRva - keyLo) + d;
                    if (ei < 0 || ki < 0 || (size_t)ei + 16 > span || (size_t)ki + 16 > span)
                        continue;

                    r.candidatesTested++;
                    const uint64_t base = Decrypt(encBuf.data() + ei, keyBuf.data() + ki,
                                                  spec.rot64, spec.shufloImm);
                    if (!PlausibleHeapPtr(base, imageBase))
                        continue;
                    if (tryCandidate(base, spec.encRva + d, spec.xorKeyRva + d,
                                     spec.rot64, spec.shufloImm, 1)) {
                        std::snprintf(r.note, sizeof(r.note), "shared delta %+lld", (long long)d);
                        return r;
                    }
                }
            }
        }
    }

    // Tier 2 - the encrypted blob moved on its own. Sweeps the image against
    // the key still named by the table.
    {
        uint8_t key[16] = {};
        if (ctx->read_array(imageBase + spec.xorKeyRva, key, sizeof(key))) {
            constexpr size_t kBlock = 0x10000;
            std::vector<uint8_t> buf(kBlock);
            for (uint64_t rva = 0x400000; rva + kBlock < imageSize; rva += kBlock) {
                if (!ctx->read_array(imageBase + rva, buf.data(), kBlock))
                    continue;
                for (size_t i = 0; i + 16 <= kBlock; i += 16) {
                    r.candidatesTested++;
                    const uint64_t base = Decrypt(buf.data() + i, key, spec.rot64, spec.shufloImm);
                    if (!PlausibleHeapPtr(base, imageBase))
                        continue;
                    if (tryCandidate(base, rva + i, spec.xorKeyRva, spec.rot64, spec.shufloImm, 2)) {
                        std::snprintf(r.note, sizeof(r.note), "enc moved, key unchanged");
                        return r;
                    }
                }
            }
        }
    }

    // Tier 3 - the pipeline itself changed. Re-sweeps the image trying every
    // rotation, which is how the bone-array decrypt changed shape between
    // builds. The shuffle immediate is left alone: a wrong one only permutes
    // bytes within the result, so a correct rotation still surfaces a pointer
    // shaped candidate that Tier 3's validation would reject, and widening to
    // 256 immediates costs more than it has ever bought.
    {
        uint8_t key[16] = {};
        if (ctx->read_array(imageBase + spec.xorKeyRva, key, sizeof(key))) {
            constexpr size_t kBlock = 0x10000;
            std::vector<uint8_t> buf(kBlock);
            for (uint64_t rva = 0x400000; rva + kBlock < imageSize; rva += kBlock) {
                if (!ctx->read_array(imageBase + rva, buf.data(), kBlock))
                    continue;
                for (size_t i = 0; i + 16 <= kBlock; i += 16) {
                    for (int rot = 1; rot < 64; ++rot) {
                        if (rot == spec.rot64)
                            continue; // already covered by Tier 2
                        r.candidatesTested++;
                        const uint64_t base = Decrypt(buf.data() + i, key, rot, spec.shufloImm);
                        if (!PlausibleHeapPtr(base, imageBase))
                            continue;
                        if (tryCandidate(base, rva + i, spec.xorKeyRva, rot, spec.shufloImm, 3)) {
                            std::snprintf(r.note, sizeof(r.note), "rotation changed to %d", rot);
                            return r;
                        }
                    }
                }
            }
        }
    }

    std::snprintf(r.note, sizeof(r.note), "no candidate validated");
    return r;
}

} // namespace GObjectsRecovery
