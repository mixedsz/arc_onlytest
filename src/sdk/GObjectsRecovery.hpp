#pragma once

#include <cstdint>

// Recovery scan for the GObjects base-pointer decrypt.
//
// The base pointer is produced by a fixed pipeline over two 16-byte image
// constants:
//
//     base = shufflelo( rol64( [encRva] ^ [xorKeyRva], rot ), shufloImm )
//
// All four inputs are build-specific and none of them appear in the SDK dump,
// so when the game patches they go stale together and every consumer of
// GObjects dies at once: world ESP, actor class identification, and the
// viewport lookup that the local-player chain depends on.
//
// Rather than hand-reverse them each patch, this recovers them by search. The
// pipeline is cheap to evaluate locally, so candidate image words are pulled
// down in bulk and run through it on the CPU; only the handful of candidates
// that produce a plausible pointer cost a round trip to validate. Validation
// is structural - a real GUObjectArray has a vtable in the module's read-only
// data whose slot points back into the module's code - which makes a false
// positive very unlikely and means a hit needs no human confirmation.
namespace GObjectsRecovery {

struct Result {
    bool found = false;

    // Recovered constants, ready to be written into Steam.hpp / Xbox.hpp.
    uint64_t encRva = 0;
    uint64_t xorKeyRva = 0;
    int rot64 = 0;
    int shufloImm = 0;

    // What the recovered constants produced, for the report.
    uint64_t basePtr = 0;
    uint64_t vtable = 0;

    // Which tier hit, and how much was searched. Reported either way so a
    // failed scan still says how hard it looked.
    int tier = 0;
    uint64_t candidatesTested = 0;
    uint64_t probesValidated = 0;
    char note[128] = {};
};

// Runs the tiers in increasing cost order and stops at the first structurally
// valid hit. Safe to call when GObjects is healthy - it verifies the current
// constants first and returns immediately if they still work.
//
// Read-only: this never writes to the game.
Result Run(uint64_t imageBase);

} // namespace GObjectsRecovery
