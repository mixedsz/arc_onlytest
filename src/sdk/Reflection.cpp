#include "Reflection.hpp"

#include "Offsets.hpp"
#include "SDK.hpp"
#include "Decryptions.hpp"
#include "memory/interface/ctx.hpp"

namespace Reflection {

namespace {

// Both walks are over game-controlled linked structures, so every loop needs a
// hard bound: a torn read can produce a cycle, and this runs on the frame
// thread.
constexpr int kMaxClassDepth = 24;

std::string Narrow(const std::wstring& w) {
    if (w.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), nullptr, 0, nullptr, nullptr);
    if (n <= 0)
        return {};
    std::string out((size_t)n, '\0');
    WideCharToMultiByte(CP_UTF8, 0, w.c_str(), (int)w.size(), out.data(), n, nullptr, nullptr);
    return out;
}

// Walks the class chain, calling fn for each property. fn returns false to
// stop early.
template <typename Fn>
void ForEachProperty(uint64_t imageBase, uint64_t instance, int maxProps, Fn&& fn) {
    if (!ctx || !imageBase || !instance || !is_valid_pointer_optimized(instance))
        return;

    uint64_t cls = NameResolver::ClassPrivate((uintptr_t)imageBase, instance);
    if (!is_valid_pointer_optimized(cls))
        return;

    int seen = 0;
    for (int depth = 0; depth < kMaxClassDepth && is_valid_pointer_optimized(cls); ++depth) {
        uint64_t field = NameResolver::StructChildProperties(cls);

        while (is_valid_pointer_optimized(field) && seen < maxProps) {
            const std::string name = Narrow(NameResolver::GetFFieldName((uintptr_t)imageBase, field));
            const int32_t off = NameResolver::GetFFieldOffset((uintptr_t)field);
            ++seen;

            if (!name.empty() && !fn(name, off))
                return;

            const uint64_t next = NameResolver::FieldNext(field);
            if (next == field)
                break; // self-referential: torn read
            field = next;
        }

        if (seen >= maxProps)
            return;

        const uint64_t super = NameResolver::StructSuper(cls);
        if (super == cls)
            break;
        cls = super;
    }
}

} // namespace

std::vector<Property> DumpProperties(uint64_t imageBase, uint64_t instance, int maxProps) {
    std::vector<Property> out;
    ForEachProperty(imageBase, instance, maxProps, [&](const std::string& name, int32_t off) {
        out.push_back({ name, off });
        return true;
    });
    return out;
}

void ProbeObjectSlots(uint64_t imageBase, uint64_t instance, const char* label) {
    char l[320];
    auto emit = [&]() { DebugSystem::LogConsole(l); };

    if (!ctx || !imageBase || !instance || !is_valid_pointer_optimized(instance)) {
        snprintf(l, sizeof(l), "[SlotProbe] %s abort: ctx=%d base=0x%llX inst=0x%llX",
            label, ctx ? 1 : 0, (unsigned long long)imageBase,
            (unsigned long long)instance);
        emit();
        return;
    }

    if (g_PlatformVersion == PlatformVersion::Xbox) {
        snprintf(l, sizeof(l), "[SlotProbe] %s skipped: steam-only probe", label);
        emit();
        return;
    }

    const uint32_t hash = Platform::Steam::UObjectComputeHash_Steam((uintptr_t)instance);
    const uint32_t base = hash & 3u;
    snprintf(l, sizeof(l),
        "[SlotProbe] %s inst=0x%llX hash=0x%08X base=%u table says name=%u class=%u outer=%u",
        label, (unsigned long long)instance, hash, base,
        Platform::Steam::UObjectSlotFor_Steam(instance, STEAM_UOBJECT_SLOT_NAME_XOR),
        Platform::Steam::UObjectSlotFor_Steam(instance, STEAM_UOBJECT_SLOT_CLASS_ADJ),
        Platform::Steam::UObjectSlotFor_Steam(instance, STEAM_UOBJECT_SLOT_OUTER_ADJ));
    emit();

    for (uint32_t slot = 0; slot < 4; ++slot) {
        const uint64_t addr = instance + STEAM_UOBJECT_SLOT_BASE_OFFSET +
            (uint64_t)STEAM_UOBJECT_SLOT_STRIDE * slot;

        alignas(16) uint8_t raw[16]{};
        if (!ctx->read_array(addr, raw, sizeof(raw))) {
            snprintf(l, sizeof(l), "[SlotProbe]   slot%u @0x%llX READ FAILED",
                slot, (unsigned long long)addr);
            emit();
            continue;
        }

        uint64_t lo = 0, hi = 0;
        memcpy(&lo, raw, 8);
        memcpy(&hi, raw + 8, 8);

        const uint64_t dec = (lo || hi) ? Platform::Steam::FNameDecryptSlot16(lo, hi) : 0;

        // Both readings of the same 64 bits, since which one is right is the
        // open question: as a pointer, and as an FName comparison index.
        const bool ptrOk = is_valid_pointer_optimized(dec);
        std::string asClassName;
        if (ptrOk)
            asClassName = Narrow(NameResolver::GetObjectName((uintptr_t)imageBase, dec));

        const uint32_t ci = (uint32_t)(dec & 0xFFFFFFFFu);
        std::string asOwnName;
        if (ci != 0)
            asOwnName = Narrow(DecryptFNameEntry(imageBase, ci));

        snprintf(l, sizeof(l),
            "[SlotProbe]   slot%u lo=0x%016llX hi=0x%016llX dec=0x%016llX "
            "ptrOk=%d asPtr->name=\"%s\" ci=0x%X asCI->name=\"%s\"",
            slot, (unsigned long long)lo, (unsigned long long)hi,
            (unsigned long long)dec, ptrOk ? 1 : 0, asClassName.c_str(),
            ci, asOwnName.c_str());
        emit();
    }

    // Un-rotating the final ROL64(32) costs one line and settles whether the
    // pointer slots want the rotate the name slot needs.
    for (uint32_t slot = 0; slot < 4; ++slot) {
        const uint64_t addr = instance + STEAM_UOBJECT_SLOT_BASE_OFFSET +
            (uint64_t)STEAM_UOBJECT_SLOT_STRIDE * slot;
        alignas(16) uint8_t raw[16]{};
        if (!ctx->read_array(addr, raw, sizeof(raw)))
            continue;
        uint64_t lo = 0, hi = 0;
        memcpy(&lo, raw, 8);
        memcpy(&hi, raw + 8, 8);
        if (!lo && !hi)
            continue;

        const uint64_t dec = Platform::Steam::FNameDecryptSlot16(lo, hi);
        const uint64_t unrot = (dec >> 32) | (dec << 32);
        const bool unrotOk = is_valid_pointer_optimized(unrot);
        std::string unrotName;
        if (unrotOk)
            unrotName = Narrow(NameResolver::GetObjectName((uintptr_t)imageBase, unrot));

        snprintf(l, sizeof(l),
            "[SlotProbe]   slot%u unrotated=0x%016llX ptrOk=%d name=\"%s\"",
            slot, (unsigned long long)unrot, unrotOk ? 1 : 0, unrotName.c_str());
        emit();
    }
}

void ProbeStructLayout(uint64_t imageBase, uint64_t classPtr, const char* label) {
    char l[320];
    auto emit = [&]() { DebugSystem::LogConsole(l); };

    if (!ctx || !imageBase || !is_valid_pointer_optimized(classPtr)) {
        snprintf(l, sizeof(l), "[LayoutProbe] %s abort: cls=0x%llX",
            label, (unsigned long long)classPtr);
        emit();
        return;
    }

    const std::string clsName = Narrow(NameResolver::GetObjectName((uintptr_t)imageBase, classPtr));
    snprintf(l, sizeof(l), "[LayoutProbe] %s cls=0x%llX name=\"%s\" scanning 0x40..0x180",
        label, (unsigned long long)classPtr, clsName.c_str());
    emit();

    // Any qword in the class that names itself is a UObject reference. The one
    // that says "Actor" is SuperStruct; the rest are context.
    uint64_t childCandidate = 0;
    for (uint64_t off = 0x40; off <= 0x180; off += 8) {
        auto [q, st] = ctx->read_status<uint64_t>(classPtr + off);
        if (st != TransportStatus::Ok || q == 0)
            continue;

        const bool ptrOk = is_valid_pointer_optimized(q);
        std::string nm;
        if (ptrOk)
            nm = Narrow(NameResolver::GetObjectName((uintptr_t)imageBase, q));

        // A pointer with no resolvable name is the shape a ChildProperties head
        // has, since FFields are not UObjects and carry no object name.
        if (ptrOk && nm.empty() && childCandidate == 0 && off >= 0xC0)
            childCandidate = q;

        if (!ptrOk && q > 0xFFFFFFFFULL)
            continue;  // dense non-pointer payload, not worth a line each

        snprintf(l, sizeof(l), "[LayoutProbe]   +0x%03llX = 0x%016llX ptrOk=%d name=\"%s\"",
            (unsigned long long)off, (unsigned long long)q, ptrOk ? 1 : 0, nm.c_str());
        emit();
    }

    const uint64_t child = ctx->read<uint64_t>(classPtr + STEAM_USTRUCT_CHILD_PROPERTIES);
    const uint64_t field = is_valid_pointer_optimized(child) ? child : childCandidate;
    if (!is_valid_pointer_optimized(field)) {
        snprintf(l, sizeof(l), "[LayoutProbe] %s no FField candidate found", label);
        emit();
        return;
    }

    snprintf(l, sizeof(l), "[LayoutProbe] %s field=0x%llX scanning 0x00..0x140 "
        "(owner should read 0x%llX or 0x%llX)",
        label, (unsigned long long)field,
        (unsigned long long)classPtr, (unsigned long long)(classPtr | 1ULL));
    emit();

    for (uint64_t off = 0; off <= 0x140; off += 8) {
        auto [q, st] = ctx->read_status<uint64_t>(field + off);
        if (st != TransportStatus::Ok || q == 0)
            continue;

        const char* tag = "";
        if (q == classPtr) tag = " <-- OWNER (untagged)";
        else if (q == (classPtr | 1ULL)) tag = " <-- OWNER (tagged)";

        const bool ptrOk = is_valid_pointer_optimized(q);
        // A sibling FField lands in the same allocation region as this one, so
        // a near pointer is the Next candidate.
        const bool nearField = ptrOk && (q >> 32) == (field >> 32);

        snprintf(l, sizeof(l),
            "[LayoutProbe]   fld+0x%03llX = 0x%016llX ptrOk=%d near=%d lo32=0x%08X%s",
            (unsigned long long)off, (unsigned long long)q, ptrOk ? 1 : 0,
            nearField ? 1 : 0, (unsigned)(q & 0xFFFFFFFFu), tag);
        emit();
    }
}

void TraceWalk(uint64_t imageBase, uint64_t instance, const char* label) {
    char l[256];
    auto emit = [&]() { DebugSystem::LogConsole(l); };

    if (!ctx || !imageBase || !instance) {
        snprintf(l, sizeof(l), "[RefTrace] %s abort: ctx=%d base=0x%llX inst=0x%llX",
            label, ctx ? 1 : 0, (unsigned long long)imageBase, (unsigned long long)instance);
        emit();
        return;
    }

    const bool instOk = is_valid_pointer_optimized(instance);
    auto [vt, vtSt] = ctx->read_status<uint64_t>(instance);
    snprintf(l, sizeof(l), "[RefTrace] %s inst=0x%llX valid=%d vtable=0x%llX st=%d",
        label, (unsigned long long)instance, instOk ? 1 : 0,
        (unsigned long long)vt, (int)vtSt);
    emit();
    if (!instOk)
        return;

    const uint64_t cls = NameResolver::ClassPrivate((uintptr_t)imageBase, instance);
    const bool clsOk = is_valid_pointer_optimized(cls);
    snprintf(l, sizeof(l), "[RefTrace] ClassPrivate=0x%llX valid=%d",
        (unsigned long long)cls, clsOk ? 1 : 0);
    emit();
    if (!clsOk)
        return;

    // If the class name decrypts, the name pool and ClassPrivate are both fine
    // and any failure below is isolated to the FField chain offsets.
    const std::string clsName = Narrow(NameResolver::GetObjectName((uintptr_t)imageBase, cls));
    snprintf(l, sizeof(l), "[RefTrace] class name=\"%s\" len=%d",
        clsName.c_str(), (int)clsName.size());
    emit();

    uint64_t walk = cls;
    for (int depth = 0; depth < 4 && is_valid_pointer_optimized(walk); ++depth) {
        const uint64_t child = NameResolver::StructChildProperties(walk);
        const uint64_t super = NameResolver::StructSuper(walk);
        snprintf(l, sizeof(l),
            "[RefTrace] depth=%d cls=0x%llX child=0x%llX childValid=%d super=0x%llX superValid=%d",
            depth, (unsigned long long)walk, (unsigned long long)child,
            is_valid_pointer_optimized(child) ? 1 : 0,
            (unsigned long long)super, is_valid_pointer_optimized(super) ? 1 : 0);
        emit();

        uint64_t field = child;
        for (int i = 0; i < 3 && is_valid_pointer_optimized(field); ++i) {
            const std::string fname = Narrow(NameResolver::GetFFieldName((uintptr_t)imageBase, field));
            const int32_t foff = NameResolver::GetFFieldOffset((uintptr_t)field);
            const uint64_t next = NameResolver::FieldNext(field);
            snprintf(l, sizeof(l),
                "[RefTrace]   field[%d]=0x%llX name=\"%s\" len=%d off=0x%X next=0x%llX nextValid=%d",
                i, (unsigned long long)field, fname.c_str(), (int)fname.size(),
                (unsigned)foff, (unsigned long long)next,
                is_valid_pointer_optimized(next) ? 1 : 0);
            emit();
            if (next == field)
                break;
            field = next;
        }

        if (super == walk)
            break;
        walk = super;
    }
}

int32_t FindPropertyOffset(uint64_t imageBase, uint64_t instance, const char* propertyName) {
    if (!propertyName || !*propertyName)
        return -1;

    int32_t found = -1;
    ForEachProperty(imageBase, instance, 4096, [&](const std::string& name, int32_t off) {
        if (name == propertyName) {
            found = off;
            return false;
        }
        return true;
    });
    return found;
}

} // namespace Reflection
