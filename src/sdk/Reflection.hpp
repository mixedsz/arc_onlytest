#pragma once

#include <cstdint>
#include <string>
#include <vector>

// Offset lookup through the engine's own reflection data.
//
// Most of the offset table is transcribed from an SDK dump, which means every
// patch invalidates it wholesale and any field the dump did not cover is a
// guess. Several entries currently carry "estimated" or "RE-VERIFY LIVE" and at
// least one - CameraManager LockedFov - has been demonstrably wrong while
// looking plausible, costing two debugging sessions.
//
// UObject carries a UClass, and a UClass carries a linked list of FProperty
// describing every reflected field by name and offset. Reading that list is
// strictly better than transcribing a dump: it comes from the build actually
// running, it needs no writes, and a name either matches or it does not, so
// there is no confidence judgement to get wrong.
//
// This does not need GObjects. The class comes from a live instance pointer,
// so it works while the GObjects decrypt is broken.
namespace Reflection {

struct Property {
    std::string name;
    int32_t offset = 0;
};

// Every reflected property on the object's class, walking up through parent
// classes. Ordered nearest-class-first. Empty if the class or its property
// list could not be read.
std::vector<Property> DumpProperties(uint64_t imageBase, uint64_t instance, int maxProps = 512);

// Offset of a single named property, searched over the same chain.
// Returns -1 when the class resolves but carries no property by that name,
// which distinguishes "renamed or not reflected" from a broken read.
int32_t FindPropertyOffset(uint64_t imageBase, uint64_t instance, const char* propertyName);

// Read-only slot probe. ClassPrivate decodes to 0x91B8AF0000000000 for every
// instance of a class, which is class-consistent but not a pointer, so the
// slot machinery is reaching something real and either the slot index or the
// pointer-side transform is wrong. Guessing between those is what the table
// cannot settle.
//
// So: decode all four slots and let them identify themselves. Name resolution
// already works, which gives a ground truth to grade against - the class slot
// is whichever one decodes to a pointer whose own name reads back as a class
// name, and the name slot is whichever one's low 32 bits resolve to the
// object's own name. Nothing here writes; the FOV probe that crashed the game
// did, into memory it had not identified.
//
// Steam only. Xbox packs slots at base 0x10 stride 0x10 with a different hash
// and decode, and there is no live Xbox capture to grade a probe against.
void ProbeObjectSlots(uint64_t imageBase, uint64_t instance, const char* label);

// Read-only layout probe for the level below ProbeObjectSlots. The class
// pointer now resolves and names off it read correctly, but the property walk
// still returns nothing: SuperStruct at the table's 0xA8 reads 0x2, and the
// first FField's supposed Next at 0x80 reads back as the class pointer with
// bit 0 set - which is an FFieldVariant Owner, not a Next. The dump's FField
// and UStruct sections are dated 2026-08-11 against an 08-18 game build, so
// they are simply from the wrong build.
//
// Ground truth this can grade itself against: PlayerCameraManager derives from
// Actor, so whichever qword in the class resolves to the name "Actor" is
// SuperStruct. Inside the FField, Owner is identifiable as classPtr with or
// without the tag bit.
void ProbeStructLayout(uint64_t imageBase, uint64_t classPtr, const char* label);

// One-shot diagnostic. DumpProperties returning empty is ambiguous - it covers
// a bad instance, a failed class decrypt, a stale ChildProperties offset and a
// stale FField chain equally. This logs each hop so the failure lands on one
// specific offset. Rate-limited by being called once per session; output is
// dropped entirely in Release, where DBG_LOG compiles away.
void TraceWalk(uint64_t imageBase, uint64_t instance, const char* label);

} // namespace Reflection
