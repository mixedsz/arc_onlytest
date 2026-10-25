#pragma once
#include "EntityCache.hpp"
#include "HeldItems.hpp"
#include "dgb/Spoof.h"
#include <iostream>

namespace NT::Store {

    inline void GetArcHealth(uintptr_t actor, bool isDestroyed, double& outCurrent, double& outMax, bool& stale) {
        SPOOF_FUNC;
        outCurrent = 0.0;
        outMax = 0.0;
        const auto& arc = Offsets::Get().Arc;

        // Do not early-out on IsDestroyed — that flag offset false-positives and hides live AI ESP.

        auto saneMax = [](double v) -> bool {
            // Reject absurd maxes from wrong AHC offsets (seen ~14k garbage).
            return std::isfinite(v) && v > 1.0 && v <= 5000.0;
        };
        auto isUnorm = [](float v) -> bool {
            return std::isfinite(v) && v >= 0.0f && v <= 1.0f;
        };

        // --- Max HP from ActorHealthComponent (probe alts; float or double) ---
        double maxHp = 0.0;
        uintptr_t usedAhc = 0;
        const uintptr_t ahcOffs[] = {
            arc.ActorHealthComponent,
            0x5E0, 0x590, 0x600, 0x5C0,
            Offsets::Get().UPawn.HealthComponent,
        };
        const uintptr_t maxOffs[] = {
            arc.ActorHealthComponent_MaxHealth,
            0x168, 0x188, 0x2B8, 0x308,
        };
        for (uintptr_t ahcOff : ahcOffs) {
            uintptr_t ahc = ctx->read<uintptr_t>(actor + ahcOff);
            if (!is_valid_pointer_optimized(ahc))
                continue;
            for (uintptr_t mOff : maxOffs) {
                float mf = ctx->read<float>(ahc + mOff);
                if (saneMax(mf)) { maxHp = mf; usedAhc = ahcOff; break; }
                double md = ctx->read<double>(ahc + mOff);
                if (saneMax(md)) { maxHp = md; usedAhc = ahcOff; break; }
            }
            if (maxHp > 0.0) break;
        }
        const bool hasMaxHp = saneMax(maxHp);

        // Sticky DDS layout once discovered (avoids probing every ARC every tick).
        struct DdsLayout {
            uintptr_t hsOff = 0, ddsOff = 0, arrDataOff = 0, arrNumOff = 0;
            size_t stride = 0, unormOff = 0;
            bool valid = false;
        };
        static DdsLayout s_dds;

        auto tryReadDds = [&](uintptr_t hsOff, uintptr_t ddsOff, uintptr_t arrDataOff, uintptr_t arrNumOff,
                              size_t stride, size_t uOff, double& outAvg, int& outValid) -> bool {
            outAvg = -1.0;
            outValid = 0;
            uintptr_t healthService = ctx->read<uintptr_t>(actor + hsOff);
            if (!is_valid_pointer_optimized(healthService))
                return false;
            uintptr_t dds = ctx->read<uintptr_t>(healthService + ddsOff);
            if (!is_valid_pointer_optimized(dds))
                return false;
            uintptr_t arrayData = ctx->read<uintptr_t>(dds + arrDataOff);
            int32_t arrayNum = ctx->read<int32_t>(dds + arrNumOff);
            if (!is_valid_pointer_optimized(arrayData) || arrayNum <= 0 || arrayNum > 128)
                return false;
            if (stride == 0 || stride > 0x200 || uOff + sizeof(float) > stride)
                return false;
            size_t totalReadSize = (size_t)arrayNum * stride;
            if (totalReadSize > 0x10000)
                return false;
            std::vector<uint8_t> ddsBuf(totalReadSize);
            if (!ctx->read_array(arrayData, ddsBuf.data(), totalReadSize))
                return false;
            double sum = 0.0;
            double minU = 2.0;
            int validCount = 0;
            for (int32_t i = 0; i < arrayNum; ++i) {
                float unorm = *(float*)(ddsBuf.data() + (size_t)i * stride + uOff);
                if (!isUnorm(unorm))
                    continue;
                sum += unorm;
                if (unorm < minU) minU = unorm;
                validCount++;
            }
            if (validCount < (arrayNum + 1) / 2)
                return false;
            double avg = sum / validCount;
            outAvg = avg * 0.65 + minU * 0.35; // move bar when one weak point is hit
            outValid = validCount;
            return true;
        };

        double bestAvg = -1.0;
        int bestValid = 0;
        const char* hitPath = "none";
        uintptr_t usedHs = 0, usedDdsStore = 0, usedArr = 0;

        if (s_dds.valid) {
            double avg = -1.0;
            int valid = 0;
            if (tryReadDds(s_dds.hsOff, s_dds.ddsOff, s_dds.arrDataOff, s_dds.arrNumOff,
                           s_dds.stride, s_dds.unormOff, avg, valid)) {
                bestAvg = avg;
                bestValid = valid;
                usedHs = s_dds.hsOff;
                usedDdsStore = s_dds.ddsOff;
                usedArr = s_dds.arrDataOff;
                hitPath = "dds";
            } else {
                s_dds.valid = false; // layout broke — rediscover
            }
        }

        // Full DDS layout rediscovery is extremely expensive — rate-limit globally.
        static uint64_t s_lastDdsProbeMs = 0;
        const uint64_t nowProbe = GetTickCount64();
        const bool allowDdsProbe = !s_dds.valid && (nowProbe - s_lastDdsProbeMs > 1500);

        if (allowDdsProbe) {
            s_lastDdsProbeMs = nowProbe;
            const uintptr_t hsOffs[] = { arc.HealthService, 0x1288, 0x1238, 0x12A8 };
            const uintptr_t ddsStoreOffs[] = { arc.HealthService_DamageDataStore, 0x148, 0x160 };
            const uintptr_t arrDataOffs[] = { arc.DDS_ArrayData, 0xA8, 0x68 };
            const uintptr_t arrNumOffs[] = { arc.DDS_ArrayNum, 0xB0, 0x70 };
            const size_t strides[] = { (size_t)arc.DDS_ElementStride, 0x70, 0x68 };
            const size_t unormOffs[] = { (size_t)arc.DDS_Element_CurrentHealthUNorm, 0x34, 0x30, 0x38 };

            for (uintptr_t hsOff : hsOffs) {
                for (uintptr_t ddsOff : ddsStoreOffs) {
                    for (size_t ai = 0; ai < 3; ++ai) {
                        for (size_t stride : strides) {
                            for (size_t uOff : unormOffs) {
                                double avg = -1.0;
                                int valid = 0;
                                if (!tryReadDds(hsOff, ddsOff, arrDataOffs[ai], arrNumOffs[ai],
                                                stride, uOff, avg, valid))
                                    continue;
                                if (valid > bestValid || (valid == bestValid && avg < bestAvg)) {
                                    bestValid = valid;
                                    bestAvg = avg;
                                    usedHs = hsOff;
                                    usedDdsStore = ddsOff;
                                    usedArr = arrDataOffs[ai];
                                    hitPath = "dds";
                                    s_dds = { hsOff, ddsOff, arrDataOffs[ai], arrNumOffs[ai], stride, uOff, true };
                                }
                                if (bestAvg >= 0.0 && bestAvg < 0.999)
                                    goto dds_done;
                            }
                        }
                    }
                }
            }
        dds_done:;
        }

        if (bestAvg >= 0.0) {
            outMax = hasMaxHp ? maxHp : 100.0;
            outCurrent = outMax * bestAvg;
            if (outCurrent < 0.0) outCurrent = 0.0;
            if (outCurrent > outMax) outCurrent = outMax;

            // If sticky DDS always reports full, rediscover periodically (wrong layout lock-in).
            static uint64_t s_fullSince = 0;
            uint64_t t = GetTickCount64();
            if (bestAvg >= 0.999 && hitPath && hitPath[0] == 'd') {
                if (!s_fullSince) s_fullSince = t;
                else if (t - s_fullSince > 12000)
                    s_dds.valid = false;
            } else {
                s_fullSince = 0;
            }
        }

        // --- HealthGroupService destroyed-parts fraction ---
        if (bestAvg < 0.0) {
            const uintptr_t hgsOffs[] = { arc.HealthGroupService, 0x12B0, 0x1260, 0x12D8 };
            for (uintptr_t hgsOff : hgsOffs) {
                uintptr_t hgs = ctx->read<uintptr_t>(actor + hgsOff);
                if (!is_valid_pointer_optimized(hgs))
                    continue;
                uintptr_t baseGroupAddr = hgs + arc.HGS_BaseGroup;
                int32_t slotsNum = ctx->read<int32_t>(baseGroupAddr + arc.BaseGroup_PartIDs_SlotsNum);
                int32_t numFreeSlots = ctx->read<int32_t>(baseGroupAddr + arc.BaseGroup_PartIDs_NumFreeSlots);
                int32_t numDestroyed = ctx->read<int32_t>(baseGroupAddr + arc.BaseGroup_NumOfDestroyedParts);
                int32_t totalParts = slotsNum - numFreeSlots;
                if (totalParts <= 0 || totalParts > 256 || numDestroyed < 0 || numDestroyed > totalParts)
                    continue;
                double fraction = 1.0 - (static_cast<double>(numDestroyed) / static_cast<double>(totalParts));
                if (fraction < 0.0) fraction = 0.0;
                if (fraction > 1.0) fraction = 1.0;
                outMax = hasMaxHp ? maxHp : 100.0;
                outCurrent = outMax * fraction;
                hitPath = "hgs";
                bestAvg = fraction;
                break;
            }
        }

        // --- Last resort: pawn HealthComponent CachedHealth (some ARC pawns share it) ---
        if (bestAvg < 0.0) {
            uintptr_t hc = ctx->read<uintptr_t>(actor + Offsets::Get().UPawn.HealthComponent);
            if (ctx->last_status() != TransportStatus::Ok)
                stale = true;
            if (is_valid_pointer_optimized(hc)) {
                const auto& hcOff = Offsets::Get().UHealthComponent;
                double ch = ctx->read<double>(hc + hcOff.CurrentHealth);
                if (ctx->last_status() != TransportStatus::Ok)
                    stale = true;
                double mh = ctx->read<double>(hc + hcOff.MaxHealth);
                if (ctx->last_status() != TransportStatus::Ok)
                    stale = true;
                if (!saneMax(mh)) {
                    mh = ctx->read<double>(hc + 0x2B8);
                    if (ctx->last_status() != TransportStatus::Ok)
                        stale = true;
                }
                if (saneMax(mh) && std::isfinite(ch) && ch >= 0.0 && ch <= mh * 1.05) {
                    outCurrent = ch;
                    outMax = mh;
                    hitPath = "hc";
                    bestAvg = ch / mh;
                }
            }
        }

        // Do NOT invent full health when we never found a live fraction — hide the bar instead.
        if (bestAvg < 0.0) {
            outCurrent = 0.0;
            outMax = 0.0;
            hitPath = "fail";
        }

        (void)isDestroyed;
        (void)usedAhc; (void)usedHs; (void)usedDdsStore; (void)usedArr; (void)hitPath;
    }

    class CAsyncPlayerSync {
    private:
        std::unordered_map<uintptr_t, SlowPlayerData> m_cache;
        std::shared_mutex m_mtx;

        void Cache() {
            SPOOF_FUNC;
            if (Entity->IsTransitioning())
                return;

            auto snapshot = Entity->GetSnapshot();
            if (snapshot.empty())
                return;

            // Held item actors, republished for the world threat scan. Only
            // filled on a clean pass over the whole snapshot - a run cut short
            // by a transition would look like "nobody is holding anything".
            std::vector<uint64_t> heldActors;
            heldActors.reserve(snapshot.size());
            bool fullPass = true;

            for (auto& ent : snapshot) {
                if (Entity->IsTransitioning()) {
                    fullPass = false;
                    break;
                }

                if (!is_valid_pointer_optimized(ent.Entity))
                    continue;

                SlowPlayerData slowData;
                slowData.stale = ent.stale;

                auto sane = [](double v) -> bool {
                    return !std::isnan(v) && std::isfinite(v) && v >= 0.0 && v <= 100000.0;
                };
                auto looksLikeVitals = [&](double ch, double mh) -> bool {
                    return sane(ch) && sane(mh) && mh > 1.0 && mh <= 5000.0 && ch <= mh * 1.05;
                };
                auto looksLikePioneerVitals = [&](double ch, double mh, double ca, double ma) -> bool {
                    // Pioneer max HP is typically ~100; armor ~0-100. Reject huge ARC-like maxes here.
                    if (!looksLikeVitals(ch, mh) || mh < 20.0 || mh > 400.0)
                        return false;
                    if (!sane(ca) || !sane(ma))
                        return false;
                    if (ma > 400.0)
                        return false;
                    if (ma > 0.0 && ca > ma * 1.05)
                        return false;
                    return true;
                };

                // Sticky HealthInfo offset once discovered (logs show configured 0x540 is dead).
                static uintptr_t s_hiOff = 0;
                static bool s_hiIsFloat = false;

                double hiCh = -1.0, hiMh = -1.0, hiCa = -1.0, hiMa = -1.0;
                bool hiDbno = false;
                // Distinguishes "the byte says not-downed" from "the byte never arrived".
                // Without it both look like hiDbno == false, which is what forced the old
                // one-way latch and made a single bad sample permanent.
                bool hiDbnoValid = false;
                uintptr_t usedHiOff = 0;
                uint8_t dbnoRawByte = 0;

                // The HealthInfo probe now runs after the health component read, further
                // down. It needs the component's values first: they are an independent,
                // working read of the same two numbers, which turns locating this struct
                // from a guess into a match.

                double hcCh = -1.0, hcMh = -1.0, hcCa = -1.0, hcMa = -1.0;
                uintptr_t hcAddr = ent.HealthComponent;
                static uintptr_t s_hcPawnOff = 0;
                static uintptr_t s_hcCurOff = 0;
                static uintptr_t s_hcMaxOff = 0;

                auto tryReadHc = [&](uintptr_t addr) -> bool {
                    if (!is_valid_pointer_optimized(addr))
                        return false;
                    const uintptr_t curOffs[] = {
                        s_hcCurOff ? s_hcCurOff : Offsets::Get().UHealthComponent.CurrentHealth,
                        Offsets::Get().UHealthComponent.CurrentHealth, 0x668, 0x610,
                    };
                    const uintptr_t maxOffs[] = {
                        s_hcMaxOff ? s_hcMaxOff : Offsets::Get().UHealthComponent.MaxHealth,
                        Offsets::Get().UHealthComponent.MaxHealth, 0x308, 0x2B8,
                    };
                    for (uintptr_t cOff : curOffs) {
                        if (!cOff) continue;
                        for (uintptr_t mOff : maxOffs) {
                            if (!mOff) continue;
                            double ch = ctx->read<double>(addr + cOff);
                            double mh = ctx->read<double>(addr + mOff);
                            if (!looksLikeVitals(ch, mh))
                                continue;
                            hcCh = ch; hcMh = mh;
                            s_hcCurOff = cOff;
                            s_hcMaxOff = mOff;
                            const auto& hcOff = Offsets::Get().UHealthComponent;
                            // Dump HealthComponent ArmorSlot: Health @ 0x150, CurrentMaxArmor @ 0x158.
                            // Same 8-byte stride as CachedHealth. Float fallback if the slot is float.
                            double armorD[2] = { 0.0, 0.0 };
                            if (hcOff.CurrentArmor &&
                                ctx->read_array(addr + hcOff.CurrentArmor, armorD, sizeof(armorD)) &&
                                sane(armorD[0]) && sane(armorD[1]) && armorD[1] <= 400.0 &&
                                !(armorD[1] > 0.0 && armorD[0] > armorD[1] * 1.05)) {
                                hcCa = armorD[0];
                                hcMa = armorD[1];
                            } else if (hcOff.CurrentArmor) {
                                float fCur = ctx->read<float>(addr + hcOff.CurrentArmor);
                                float fMax = ctx->read<float>(addr + hcOff.CurrentArmor + 4);
                                if (!(sane((double)fMax) && (double)fMax <= 400.0))
                                    fMax = ctx->read<float>(addr + hcOff.MaxArmor);
                                if (sane((double)fCur) && sane((double)fMax) && (double)fMax <= 400.0 &&
                                    !((double)fMax > 0.0 && (double)fCur > (double)fMax * 1.05)) {
                                    hcCa = (double)fCur;
                                    hcMa = (double)fMax;
                                }
                            }
                            return true;
                        }
                    }
                    return false;
                };

                if (!tryReadHc(hcAddr) && is_valid_pointer_optimized(ent.Entity)) {
                    const uintptr_t hcPawnOffs[] = {
                        s_hcPawnOff ? s_hcPawnOff : Offsets::Get().UPawn.HealthComponent,
                        Offsets::Get().UPawn.HealthComponent,
                        0xDC8, 0xD68, 0xD78, 0xD30, 0xDA0, 0xDE0, 0xD50,
                    };
                    for (uintptr_t pOff : hcPawnOffs) {
                        if (!pOff) continue;
                        uintptr_t cand = ctx->read<uintptr_t>(ent.Entity + pOff);
                        if (tryReadHc(cand)) {
                            hcAddr = cand;
                            s_hcPawnOff = pOff;
                            break;
                        }
                    }
                }

                // Downed flag, read straight off the PlayerState.
                //
                // This does not go through FPlayerHealthInfo, because that struct cannot be
                // found on this build: [HiScan] sweeps the whole PlayerState and reports one
                // loosely health-shaped pair, matching nothing. Everything addressed relative
                // to it - including bIsDbno - was therefore unreadable, dbnoValid was 0 on all
                // 741 samples, and the ghost filter's !isDBNO term was vacuously true. That is
                // what declared knocked players dead 329 times in a single session.
                //
                // The offset came from observation rather than a struct guess ([DbnoScan]
                // diffing a healthy-to-downed transition), and one sample is not proof, so it
                // has to earn trust before it is used: the byte must be boolean-valued and
                // must have been seen reading 0 while the player was healthy. An offset that
                // happens to hold something else fails that and is ignored, which leaves the
                // conservative path below rather than a confidently wrong answer.
                if (is_valid_pointer_optimized(ent.PlayerState) &&
                    Offsets::Get().UPlayerState.IsDbno != 0)
                {
                    struct DbnoTrust { bool sawZeroWhileHealthy = false; bool poisoned = false; };
                    static std::unordered_map<uint64_t, DbnoTrust> s_dbnoTrust;
                    if (s_dbnoTrust.size() > 512)
                        s_dbnoTrust.clear();

                    const uint8_t raw = ctx->read<uint8_t>(
                        ent.PlayerState + Offsets::Get().UPlayerState.IsDbno);
                    dbnoRawByte = raw;
                    if (ctx->last_status() == TransportStatus::Ok) {
                        auto& trust = s_dbnoTrust[ent.Entity];
                        // The dump identifies this as PioneerPlayerState::PlayerStatus, an
                        // enum rather than a bool, so the old "anything above 1 is the wrong
                        // byte" rule would reject legitimate states. Only a value too large
                        // to be a small status enum disqualifies the offset now. Which
                        // specific value means downed is not known yet - [VitalsDiag] prints
                        // the raw byte so the mapping can be read off a knock.
                        if (raw > 8)
                            trust.poisoned = true;
                        if (!trust.poisoned) {
                            const bool healthy = (hcMh > 0.0 && hcCh > hcMh * 0.5);
                            if (healthy) {
                                if (raw == 0)
                                    trust.sawZeroWhileHealthy = true;
                                else
                                    trust.poisoned = true; // set while at full health - wrong byte
                            }
                            if (trust.sawZeroWhileHealthy) {
                                hiDbno = (raw != 0);
                                hiDbnoValid = true;
                            }
                        }
                    }
                }

                // Retired. Every version of this probe failed for the same reason, and the
                // Dumper-7 dump finally explains it: there is no FPlayerHealthInfo struct in
                // this game. Not at 0x540, not anywhere in 750,000 lines - the offset we were
                // using lands inside PioneerPlayerState::RespawnInfo. That is why [HiScan]
                // reported hiOff=0x0 on all 699 attempts across every heuristic we tried,
                // including matching against known-good values from the health component.
                //
                // Health comes from HealthComponent::CachedHealth and the downed state from
                // PioneerPlayerState::PlayerStatus, both read above, so nothing depends on
                // this any more. Left compiled rather than deleted so it can be switched back
                // on in one line if a later build introduces the struct, but not executed:
                // it cost a full PlayerState sweep per player per pass to produce noise.
                constexpr bool kProbeHealthInfo = false;
                if (kProbeHealthInfo && is_valid_pointer_optimized(ent.PlayerState)) {
                    auto readHiAt = [&](uintptr_t off, bool asFloat,
                                        double& ch, double& mh, double& ca, double& ma) -> bool {
                        uintptr_t hiBase = ent.PlayerState + off;
                        if (asFloat) {
                            float fbuf[4] = {};
                            if (!ctx->read_array(hiBase, fbuf, sizeof(fbuf)))
                                return false;
                            ch = fbuf[0]; mh = fbuf[1]; ca = fbuf[2]; ma = fbuf[3];
                        } else {
                            double dbuf[4] = {};
                            if (!ctx->read_array(hiBase, dbuf, sizeof(dbuf)))
                                return false;
                            ch = dbuf[0]; mh = dbuf[1]; ca = dbuf[2]; ma = dbuf[3];
                        }
                        return true;
                    };

                    // Health pair must stand on its own; armour is recorded when it happens
                    // to be plausible and ignored when it is not.
                    auto acceptHiAt = [&](uintptr_t off, bool asFloat) -> bool {
                        double ch = 0, mh = 0, ca = 0, ma = 0;
                        if (!readHiAt(off, asFloat, ch, mh, ca, ma))
                            return false;
                        if (!looksLikeVitals(ch, mh) || mh < 20.0 || mh > 400.0)
                            return false;
                        // Cross-check against the component whenever it read, so a relaxed
                        // armour rule cannot let an unrelated pair of numbers stick.
                        if (hcMh > 0.0 && (fabs(mh - hcMh) > 1.0 || fabs(ch - hcCh) > 5.0))
                            return false;

                        const bool armorOk = sane(ca) && sane(ma) && ma <= 400.0 &&
                            !(ma > 0.0 && ca > ma * 1.05);
                        hiCh = ch; hiMh = mh;
                        hiCa = armorOk ? ca : -1.0;
                        hiMa = armorOk ? ma : -1.0;
                        usedHiOff = off;
                        s_hiOff = off;
                        s_hiIsFloat = asFloat;

                        // bIsDbno sits past the four values; doubles => ~0x21, floats denser
                        // => 0x11. Only consulted if the validated absolute read above did
                        // not already produce an answer - that one has been checked against
                        // the player's actual state, this is still a guess at the layout.
                        if (!hiDbnoValid) {
                            const uintptr_t dbnoAddr =
                                ent.PlayerState + off + (asFloat ? 0x11 : 0x21);
                            uint8_t dbno = ctx->read<uint8_t>(dbnoAddr);
                            if (ctx->last_status() == TransportStatus::Ok) {
                                hiDbno = (dbno != 0);
                                hiDbnoValid = true;
                            }
                        }
                        return true;
                    };

                    bool gotHi = false;
                    if (s_hiOff != 0)
                        gotHi = acceptHiAt(s_hiOff, s_hiIsFloat);

                    if (!gotHi) {
                        const uintptr_t hiBases[] = {
                            Offsets::Get().UPlayerState.HealthInfo,
                            0x540, 0x530, 0x548, 0x520, 0x550, 0x560, 0x510, 0x500,
                            0x4F0, 0x4E0, 0x580, 0x590, 0x4C0, 0x480,
                        };
                        for (uintptr_t off : hiBases) {
                            if (acceptHiAt(off, false) || acceptHiAt(off, true)) {
                                gotHi = true;
                                break;
                            }
                        }
                    }
                    // Fingerprint sweep. Only worth running while no offset is known and the
                    // component gave us something to match against.
                    static uint64_t s_lastHiFullScanMs = 0;
                    if (!gotHi && s_hiOff == 0 && hcMh > 0.0 && hcCh >= 0.0 &&
                        (GetTickCount64() - s_lastHiFullScanMs) > 500)
                    {
                        s_lastHiFullScanMs = GetTickCount64();
                        // Widened from 0x3C0-0x6C0: that window was itself a guess, and if the
                        // struct sits outside it no heuristic inside could ever have found it.
                        constexpr uintptr_t kScanStart = 0x280;
                        constexpr size_t kScanLen = 0x600;
                        uint8_t blob[kScanLen] = {};
                        const bool readOk = ctx->read_array(ent.PlayerState + kScanStart, blob, kScanLen);

                        uintptr_t bestOff = 0;
                        bool bestFloat = false;
                        double bestDelta = 1.0e18;
                        int nearMisses = 0;

                        if (readOk) {
                            auto consider = [&](double ch, double mh, uintptr_t off, bool asFloat) {
                                if (!std::isfinite(ch) || !std::isfinite(mh))
                                    return;
                                if (mh < 20.0 || mh > 400.0 || ch < 0.0 || ch > mh * 1.05)
                                    return;
                                ++nearMisses;
                                const double delta = fabs(ch - hcCh) + fabs(mh - hcMh);
                                if (delta > 1.0)
                                    return;
                                if (delta < bestDelta) {
                                    bestDelta = delta;
                                    bestOff = off;
                                    bestFloat = asFloat;
                                }
                            };
                            for (size_t rel = 0; rel + 16 <= kScanLen; rel += 4) {
                                if ((rel % 8) == 0) {
                                    double d0 = 0.0, d1 = 0.0;
                                    memcpy(&d0, blob + rel, sizeof(d0));
                                    memcpy(&d1, blob + rel + 8, sizeof(d1));
                                    consider(d0, d1, kScanStart + rel, false);
                                }
                                float f0 = 0.f, f1 = 0.f;
                                memcpy(&f0, blob + rel, sizeof(f0));
                                memcpy(&f1, blob + rel + 4, sizeof(f1));
                                consider((double)f0, (double)f1, kScanStart + rel, true);
                            }
                            if (bestOff != 0)
                                gotHi = acceptHiAt(bestOff, bestFloat);
                        }

                        if (debugConsoleEnabled) {
                            char sline[256];
                            snprintf(sline, sizeof(sline),
                                "[HiScan] name=%s readOk=%d target(cur=%.1f max=%.1f) "
                                "plausiblePairs=%d bestOff=0x%llX bestLayout=%s bestDelta=%.2f locked=%d",
                                ent.Name.empty() ? "<empty>" : ent.Name.c_str(),
                                readOk ? 1 : 0, hcCh, hcMh, nearMisses,
                                (unsigned long long)bestOff, bestFloat ? "float" : "double",
                                (bestDelta > 1.0e17) ? -1.0 : bestDelta, gotHi ? 1 : 0);
                            DebugSystem::LogConsole(sline);
                        }
                    }
                }

                // Merge: take the lower current when both valid (damage wins over stale full),
                // except when the two disagree about zero. A real death shows up on both
                // sources within a sample or two, whereas a torn read zeroes one of them on
                // its own - and taking the minimum unconditionally let that single bad read
                // through as a confirmed kill, which is how healthy players ended up drawn
                // as knocked. When exactly one source reads zero, believe the other one.
                if (hiMh > 0.0 || hcMh > 0.0) {
                    if (hiMh > 0.0 && hcMh > 0.0) {
                        slowData.maxHealth = (hiMh >= hcMh) ? hiMh : hcMh;
                        if (hiCh >= 0.0 && hcCh >= 0.0) {
                            const bool hiZero = (hiCh <= 0.0);
                            const bool hcZero = (hcCh <= 0.0);
                            if (hiZero != hcZero)
                                slowData.currentHealth = hiZero ? hcCh : hiCh;
                            else
                                slowData.currentHealth = (hiCh < hcCh) ? hiCh : hcCh;
                        } else {
                            slowData.currentHealth = (hiCh >= 0.0) ? hiCh : hcCh;
                        }
                    } else if (hiMh > 0.0) {
                        slowData.currentHealth = hiCh;
                        slowData.maxHealth = hiMh;
                    } else {
                        slowData.currentHealth = hcCh;
                        slowData.maxHealth = hcMh;
                    }
                }

                if (hiMa > 0.0 || hiCa >= 0.0) {
                    slowData.currentArmor = (hiCa >= 0.0) ? hiCa : 0.0;
                    slowData.maxArmor = (hiMa > 0.0) ? hiMa : ((hiCa > 0.0) ? hiCa : 0.0);
                }
                if ((slowData.maxArmor <= 0.0 && slowData.currentArmor <= 0.0) &&
                    (hcMa > 0.0 || hcCa >= 0.0)) {
                    slowData.currentArmor = (hcCa >= 0.0) ? hcCa : 0.0;
                    slowData.maxArmor = (hcMa > 0.0) ? hcMa : slowData.currentArmor;
                } else if (hcCa >= 0.0 && slowData.currentArmor >= 0.0 && hcCa < slowData.currentArmor) {
                    slowData.currentArmor = hcCa;
                    if (hcMa > 0.0) slowData.maxArmor = hcMa;
                }

                // DBNO is debounced symmetrically and only ever driven by reads that landed.
                // It used to be latched on: a false read could set it, and nothing could
                // ever clear it, so one spurious non-zero byte marked a player knocked for
                // the rest of the raid. That byte also sits at a guessed offset past the
                // vitals, which makes a stray non-zero entirely plausible. Requiring the
                // same answer several polls running in both directions costs under a second
                // of latency on a real knock and rejects isolated garbage outright.
                struct DbnoTrack { bool state = false; int disagree = 0; bool everValid = false; };
                static std::unordered_map<uint64_t, DbnoTrack> s_dbnoTrack;
                constexpr int kDbnoFlipSamples = 3;

                if (s_dbnoTrack.size() > 512)
                    s_dbnoTrack.clear();
                if (hiDbnoValid) {
                    auto& dt = s_dbnoTrack[ent.Entity];
                    if (hiDbno == dt.state) {
                        dt.disagree = 0;
                    } else if (++dt.disagree >= kDbnoFlipSamples) {
                        dt.state = hiDbno;
                        dt.disagree = 0;
                    }
                    dt.everValid = true;
                    slowData.isDbno = dt.state;
                    slowData.isDbnoKnown = true;
                } else {
                    auto dtIt = s_dbnoTrack.find(ent.Entity);
                    const bool havePrior = (dtIt != s_dbnoTrack.end());
                    slowData.isDbno = havePrior ? dtIt->second.state : false;
                    // A read that missed this pass does not undo what earlier passes
                    // established, but never having had one at all is not knowledge.
                    slowData.isDbnoKnown = havePrior && dtIt->second.everValid;
                }

                // A fall to zero has to repeat before it is believed. Zero health is what
                // drives the knocked label, the aim skip and the death path in the bone
                // cache, so a single torn sample was enough to blank a healthy player.
                struct HpZeroTrack { int zeroStreak = 0; };
                static std::unordered_map<uint64_t, HpZeroTrack> s_hpZeroTrack;
                constexpr int kHpZeroConfirmSamples = 2;

                if (s_hpZeroTrack.size() > 512)
                    s_hpZeroTrack.clear();
                bool zeroUnconfirmed = false;
                if (slowData.maxHealth > 0.0 && slowData.currentHealth <= 0.0) {
                    auto& hz = s_hpZeroTrack[ent.Entity];
                    if (++hz.zeroStreak < kHpZeroConfirmSamples)
                        zeroUnconfirmed = true;
                } else {
                    s_hpZeroTrack.erase(ent.Entity);
                }

                // Keep last good vitals so bars don't disappear when HC is briefly null.
                {
                    std::shared_lock<std::shared_mutex> lk(m_mtx);
                    auto prevIt = m_cache.find(ent.Entity);
                    if (prevIt != m_cache.end()) {
                        if (slowData.maxHealth <= 0.0 && prevIt->second.maxHealth > 0.0) {
                            slowData.currentHealth = prevIt->second.currentHealth;
                            slowData.maxHealth = prevIt->second.maxHealth;
                        } else if (zeroUnconfirmed && prevIt->second.currentHealth > 0.0) {
                            slowData.currentHealth = prevIt->second.currentHealth;
                        }
                        if (slowData.maxArmor <= 0.0 && prevIt->second.maxArmor > 0.0) {
                            slowData.currentArmor = prevIt->second.currentArmor;
                            slowData.maxArmor = prevIt->second.maxArmor;
                        }
                    }
                }

                // Locate the downed flag by watching one change rather than guessing where it
                // lives. Every byte in a window of the PlayerState is snapshotted while the
                // player is healthy; the first time the health component reports them at zero
                // the window is read again and the offsets that went from 0 to a small
                // non-zero value are printed. A boolean that is false when someone is up and
                // true when they are down has to be in that list, and almost nothing else in
                // a replicated struct behaves that way at the same instant.
                //
                // This does not depend on FPlayerHealthInfo being found, which matters
                // because it currently is not, and the health component read that drives it
                // is the one part of the vitals path that has always worked.
                if (debugConsoleEnabled && is_valid_pointer_optimized(ent.PlayerState) && hcMh > 0.0) {
                    constexpr uintptr_t kDbnoScanStart = 0x280;
                    constexpr size_t kDbnoScanLen = 0x600;

                    struct DbnoProbe {
                        std::vector<uint8_t> alive;
                        uint64_t aliveMs = 0;
                        bool reported = false;
                    };
                    static std::unordered_map<uint64_t, DbnoProbe> s_dbnoProbe;
                    if (s_dbnoProbe.size() > 128)
                        s_dbnoProbe.clear();

                    auto& probe = s_dbnoProbe[ent.Entity];
                    const uint64_t nowDbno = GetTickCount64();
                    const bool healthy = (hcCh > hcMh * 0.5);
                    const bool atZero = (hcCh <= 0.0);

                    if (healthy) {
                        probe.reported = false;
                        // Refreshed periodically so the baseline stays close in time to the
                        // knock; a snapshot minutes old would diff against unrelated churn.
                        if ((nowDbno - probe.aliveMs) > 3000) {
                            std::vector<uint8_t> buf(kDbnoScanLen, 0);
                            if (ctx->read_array(ent.PlayerState + kDbnoScanStart, buf.data(), kDbnoScanLen)) {
                                probe.alive = std::move(buf);
                                probe.aliveMs = nowDbno;
                            }
                        }
                    } else if (atZero && !probe.reported && probe.alive.size() == kDbnoScanLen) {
                        std::vector<uint8_t> now(kDbnoScanLen, 0);
                        if (ctx->read_array(ent.PlayerState + kDbnoScanStart, now.data(), kDbnoScanLen)) {
                            probe.reported = true;
                            char cand[448];
                            int pos = snprintf(cand, sizeof(cand),
                                "[DbnoScan] name=%s hcCur=%.1f 0->1 offsets:",
                                ent.Name.empty() ? "<empty>" : ent.Name.c_str(), hcCh);
                            int found = 0;
                            for (size_t i = 0; i < kDbnoScanLen && pos < 400; ++i) {
                                // Boolean-looking only: a counter or a float byte rolling over
                                // would bury the flag in noise.
                                if (probe.alive[i] == 0 && now[i] != 0 && now[i] <= 4) {
                                    pos += snprintf(cand + pos, sizeof(cand) - pos, " 0x%llX=%u",
                                        (unsigned long long)(kDbnoScanStart + i), now[i]);
                                    if (++found >= 24)
                                        break;
                                }
                            }
                            if (found == 0)
                                snprintf(cand + pos, sizeof(cand) - pos, " none");
                            DebugSystem::LogConsole(cand);
                        }
                    }
                }

                if (debugConsoleEnabled) {
                    // Whether the debouncing above is protecting good data or just smoothing
                    // garbage depends on the HealthInfo offset being right, and nothing
                    // validates it: the configured 0x540 is noted dead further up, the
                    // working offset is found by a scoring scan, and the DBNO byte is then
                    // read from a fixed guess past the vitals. OffsetHealth.cpp checks the
                    // ARC health offsets but none of the player ones. This prints what was
                    // actually locked onto and what each source returned, so the numbers can
                    // be held against the health bars visible in game.
                    static std::unordered_map<std::string, uint64_t> s_vitalsLogMs;
                    if (s_vitalsLogMs.size() > 256)
                        s_vitalsLogMs.clear();
                    const std::string vKey = ent.Name.empty() ? std::string("<empty>") : ent.Name;
                    uint64_t& lastVitalsLog = s_vitalsLogMs[vKey];
                    const uint64_t nowVitals = GetTickCount64();
                    if ((nowVitals - lastVitalsLog) > 5000) {
                        lastVitalsLog = nowVitals;
                        char vline[448];
                        snprintf(vline, sizeof(vline),
                            "[VitalsDiag] name=%s hiOff=0x%llX layout=%s hi(cur=%.1f max=%.1f arm=%.1f/%.1f) "
                            "hc(cur=%.1f max=%.1f arm=%.1f/%.1f) merged(cur=%.1f max=%.1f) "
                            "dbnoRaw=%d dbnoValid=%d dbnoState=%d dbnoKnown=%d zeroUnconfirmed=%d",
                            vKey.c_str(),
                            (unsigned long long)usedHiOff, s_hiIsFloat ? "float" : "double",
                            hiCh, hiMh, hiCa, hiMa,
                            hcCh, hcMh, hcCa, hcMa,
                            slowData.currentHealth, slowData.maxHealth,
                            (int)dbnoRawByte, hiDbnoValid ? 1 : 0, slowData.isDbno ? 1 : 0,
                            slowData.isDbnoKnown ? 1 : 0, zeroUnconfirmed ? 1 : 0);
                        DebugSystem::LogConsole(vline);
                    }
                }

                if (is_valid_pointer_optimized(ent.SkeletalMesh)) {
                    CrossLayerBatch crossBatch;
                    if (ReadMeshAnimationBatch(ent.SkeletalMesh, crossBatch)) {
                        slowData.isZooming = crossBatch.bIsZooming;
                        slowData.isDodgeRolling = crossBatch.bIsDodgeRolling;
                        slowData.isInJump = crossBatch.bIsInJump;
                        slowData.characterStance = crossBatch.characterStance;
                        slowData.isSprinting = crossBatch.bIsSprinting;

                        const bool clapLive = crossBatch.fireWeaponCount >= 0 &&
                            crossBatch.fireWeaponCount <= 500000 &&
                            crossBatch.loopingReloadState <= 3 &&
                            crossBatch.ammoCount >= 0;
                        if (!clapLive) {
                            slowData.isFiring = false;
                            slowData.isReloading = false;
                        } else {
                            struct AnimTrack {
                                int32_t lastFireCnt = -1;
                                uint64_t firingUntilMs = 0;
                            };
                            static std::unordered_map<uint64_t, AnimTrack> s_animTrack;
                            if (s_animTrack.size() > 512)
                                s_animTrack.clear();
                            auto& tr = s_animTrack[ent.Entity];
                            const uint64_t nowAnimMs = GetTickCount64();
                            if (tr.lastFireCnt >= 0) {
                                const int32_t dFire = crossBatch.fireWeaponCount - tr.lastFireCnt;
                                if (dFire > 0 && dFire <= 40)
                                    tr.firingUntilMs = nowAnimMs + 350;
                            }
                            tr.lastFireCnt = crossBatch.fireWeaponCount;
                            slowData.isFiring = nowAnimMs < tr.firingUntilMs;
                            slowData.isReloading = crossBatch.bIsReloading;
                            slowData.isSprinting = crossBatch.bIsSprinting;
                        }

                        if (debugConsoleEnabled && clapLive) {
                            static std::unordered_map<std::string, uint64_t> s_animLogMs;
                            if (s_animLogMs.size() > 256)
                                s_animLogMs.clear();
                            const std::string aKey = ent.Name.empty() ? std::string("<empty>") : ent.Name;
                            uint64_t& lastAnimLog = s_animLogMs[aKey];
                            const uint64_t nowAnim = GetTickCount64();
                            const bool anyAnim = slowData.isFiring || slowData.isReloading ||
                                slowData.isSprinting || slowData.isZooming;
                            const uint64_t animGap = anyAnim ? 1000ull : 5000ull;
                            if ((nowAnim - lastAnimLog) > animGap) {
                                lastAnimLog = nowAnim;
                                char aline[512];
                                snprintf(aline, sizeof(aline),
                                    "[AnimDiag] name=%s rawFire=%d rawReload=%d sprint141=%d trySprint=%d inSprint=%d "
                                    "loco=%u zoomRaw=%u zoom=%d loopRld=%u rldIdx=%d fireCnt=%d ammo=%d "
                                    "derivedFire=%d derivedReload=%d derivedSprint=%d",
                                    aKey.c_str(),
                                    crossBatch.rawIsFiring ? 1 : 0,
                                    crossBatch.rawIsReloading ? 1 : 0,
                                    crossBatch.rawIsSprinting ? 1 : 0,
                                    crossBatch.bIsTryingToSprint ? 1 : 0,
                                    crossBatch.bInSprintState ? 1 : 0,
                                    (unsigned)crossBatch.locoMode,
                                    (unsigned)crossBatch.rawZooming,
                                    crossBatch.bIsZooming ? 1 : 0,
                                    (unsigned)crossBatch.loopingReloadState,
                                    crossBatch.reloadActionIndex,
                                    crossBatch.fireWeaponCount,
                                    crossBatch.ammoCount,
                                    slowData.isFiring ? 1 : 0,
                                    slowData.isReloading ? 1 : 0,
                                    slowData.isSprinting ? 1 : 0);
                                DebugSystem::LogConsole(aline);
                            }
                        }

                        // Visibility offset probe. bWasRecentlyRendered at 0x10B is
                        // derived from a cross-build delta, not dumped, so this
                        // verifies it before anything is wired to the ESP colours.
                        //
                        // Edges only - a per-frame dump of a boolean tells us nothing
                        // and buries the raid. bIsFiring rides along as the control:
                        // it is a known-good offset, and if our candidate's edges
                        // track firing rather than cover then 0x10B is landing in the
                        // wrong field and the derivation is wrong.
                        // Only players whose CLAP is demonstrably populated. The
                        // first run logged raw=0x00 for all 36 players and never
                        // toggled, but ammo and fireWeaponCount were zero or garbage
                        // on nearly every sample that raid and on 08-20 and 08-21
                        // too, so the object is simply not filled in for most
                        // players. Reading a byte out of an empty object proves
                        // nothing either way. A non-zero magazine is the cheapest
                        // proof the object is real.
                        const bool clapPopulated = crossBatch.ammoCount > 0;

                        if (debugConsoleEnabled && clapPopulated && crossBatch.boolWindowValid) {
                            static std::unordered_map<std::string, uint64_t> s_clapWinMs;
                            if (s_clapWinMs.size() > 256)
                                s_clapWinMs.clear();
                            const std::string wKey = ent.Name.empty() ? std::string("<empty>") : ent.Name;
                            uint64_t& lastWin = s_clapWinMs[wKey];
                            const uint64_t nowWin = GetTickCount64();
                            if ((nowWin - lastWin) > 3000) {
                                lastWin = nowWin;
                                char hex[3 * 32 + 1];
                                int hp = 0;
                                for (int b = 0; b < 32; ++b)
                                    hp += snprintf(hex + hp, sizeof(hex) - hp, "%02X ",
                                        (unsigned)crossBatch.boolWindow[b]);
                                char wline[320];
                                snprintf(wline, sizeof(wline),
                                    "[ClapWin] name=%s ammo=%d 0x100..0x11F: %s",
                                    wKey.c_str(), crossBatch.ammoCount, hex);
                                DebugSystem::LogConsole(wline);
                            }
                        }

                        if (debugConsoleEnabled && clapPopulated && crossBatch.wasRecentlyRenderedValid) {
                            struct VisEdgeTrack {
                                int lastRendered = -1;
                                int lastFiring = -1;
                                uint64_t lastLogMs = 0;
                                uint32_t suppressed = 0;
                            };
                            static std::unordered_map<uint64_t, VisEdgeTrack> s_visEdge;
                            if (s_visEdge.size() > 512)
                                s_visEdge.clear();
                            auto& ve = s_visEdge[ent.Entity];

                            const uint8_t rawVis = crossBatch.rawWasRecentlyRendered;
                            // 2 means the byte is neither 0 nor 1, which a real bool
                            // never is - on its own that condemns the offset.
                            const int rendered = (rawVis == 0) ? 0 : ((rawVis == 1) ? 1 : 2);
                            const int firing = crossBatch.rawIsFiring ? 1 : 0;

                            const bool first = ve.lastRendered < 0;
                            const bool visEdge = !first && rendered != ve.lastRendered;
                            const bool fireEdge = !first && firing != ve.lastFiring;

                            if (first || visEdge || fireEdge) {
                                const uint64_t nowEdge = GetTickCount64();
                                const uint64_t dtMs = ve.lastLogMs ? (nowEdge - ve.lastLogMs) : 0;
                                // Floor per player so a byte stuck mid-toggle cannot
                                // flood. Anything dropped is counted, not hidden.
                                if (first || dtMs > 250) {
                                    const char* what = first ? "FIRSTSEEN"
                                        : (visEdge && fireEdge) ? "VIS+FIRE"
                                        : visEdge ? (rendered == 1 ? "HIDDEN->VISIBLE"
                                                                   : "VISIBLE->HIDDEN")
                                                  : (firing ? "FIRE-START" : "FIRE-STOP");
                                    char eline[384];
                                    snprintf(eline, sizeof(eline),
                                        "[VisEdge] %-15s name=%s ent=0x%llX rendered=%d "
                                        "raw=0x%02X firing=%d ammo=%d fireCnt=%d "
                                        "dtMs=%llu suppressed=%u",
                                        what,
                                        ent.Name.empty() ? "<empty>" : ent.Name.c_str(),
                                        (unsigned long long)ent.Entity,
                                        rendered, (unsigned)rawVis, firing,
                                        crossBatch.ammoCount, crossBatch.fireWeaponCount,
                                        (unsigned long long)dtMs, ve.suppressed);
                                    DebugSystem::LogConsole(eline);
                                    ve.lastLogMs = nowEdge;
                                    ve.suppressed = 0;
                                } else {
                                    ++ve.suppressed;
                                }
                                ve.lastRendered = rendered;
                                ve.lastFiring = firing;
                            }
                        }
                    } else if (ctx->last_status() != TransportStatus::Ok) {
                        slowData.stale = true;
                    }
                }

                if (Entity->IsTransitioning()) {
                    fullPass = false;
                    break;
                }

                UPawn pawn(ent.Entity);
                UInventoryComponent inventory = pawn.GetInventoryComponent();
                // Held item: CurrentItemActors, LocalCurrentItems, then EquippedPrimary /
                // stowed slots. InvDiag src=local itemsCount=0 was the two TArrays empty
                // while the pawn still had a gun in EquippedPrimaryItem (0x500).
                // Held forward and emitted after resolution below. This block probes
                // the inventory independently and never runs the stowed-data-asset
                // fallback, so its pretty="" was being read as "the ESP showed
                // nothing" when it only meant "this probe found nothing" - two
                // different code paths. The real outcome is appended before it logs.
                std::string pendingInvDiag;
                if (debugConsoleEnabled) {
                    static uint64_t s_invLogMs = 0;
                    const uint64_t nowInv = GetTickCount64();
                    if ((nowInv - s_invLogMs) > 5000) {
                        s_invLogMs = nowInv;
                        TArray probe{};
                        const char* src = XC("none");
                        uintptr_t firstItem = 0;
                        std::string firstName;
                        if (inventory.IsValid()) {
                            probe = inventory.GetCurrentItemActors();
                            src = XC("current");
                            if (!probe.Data || probe.Count <= 0) {
                                probe = inventory.GetLocalCurrentItems();
                                src = XC("local");
                            }
                            if (probe.Data && probe.Count > 0) {
                                firstItem = ctx->read<uintptr_t>(probe.Data);
                                if (is_valid_pointer_optimized(firstItem))
                                    firstName = NameResolver::GetObjectNameA(ctx->image_base, firstItem);
                            } else {
                                const char* resolvedSrc = XC("none");
                                firstItem = inventory.ResolveHeldItemActor(&resolvedSrc);
                                src = resolvedSrc;
                                if (is_valid_pointer_optimized(firstItem))
                                    firstName = NameResolver::GetObjectNameA(ctx->image_base, firstItem);
                            }
                        }
                        char iline[512];
                        int8_t q = -99;
                        std::string pretty0;
                        if (is_valid_pointer_optimized(firstItem)) {
                            pretty0 = PrettyHeldItemName(firstName);
                            if (!ReadItemActorWeaponQuality(firstItem, q))
                                q = -99;
                        }
                        // stowedQ is an independent read off FStowedWeaponInfo. Where the
                        // two disagree on a real WeaponActor, the actor-side offset is
                        // wrong for that class.
                        const int32_t stowedQ = inventory.GetStowedWeapon0Quality();
                        snprintf(iline, sizeof(iline),
                            "[InvDiag] invOff=0x%llX invPtr=0x%llX valid=%d src=%s "
                            "itemsData=0x%llX itemsCount=%d item0=0x%llX name0=\"%s\" pretty=\"%s\" quality=%d stowedQ=%d",
                            (unsigned long long)g_resolvedInventoryOffset.load(),
                            (unsigned long long)inventory.address,
                            inventory.IsValid() ? 1 : 0, src,
                            (unsigned long long)probe.Data, probe.Count,
                            (unsigned long long)firstItem, firstName.c_str(),
                            pretty0.c_str(), (int)q, (int)stowedQ);
                        pendingInvDiag = iline;
                    }
                }
                if (inventory.IsValid()) {
                    const char* heldSrc = XC("none");
                    uintptr_t itemActorPtr = inventory.ResolveHeldItemActor(&heldSrc);
                    (void)heldSrc;
                    std::string rawName;
                    const bool haveActor =
                        (itemActorPtr != 0) && is_valid_pointer_optimized(itemActorPtr);
                    if (haveActor) {
                        rawName = NameResolver::GetObjectNameA(ctx->image_base, itemActorPtr);
                        heldActors.push_back((uint64_t)itemActorPtr);
                    }
                    // Also register every live CurrentItemActors entry so world threat
                    // ESP does not suppress a smoke/trigger that is in hand while the
                    // primary resolve still points at the gun for display.
                    {
                        auto collectHeld = [&](const TArray& arr) {
                            if (!arr.Data || arr.Count <= 0)
                                return;
                            const int n = arr.Count > 16 ? 16 : arr.Count;
                            uintptr_t cands[16];
                            if (!ctx->read_array(arr.Data, cands, (size_t)n * sizeof(uintptr_t)))
                                return;
                            for (int i = 0; i < n; ++i) {
                                if (!is_valid_pointer_optimized(cands[i]))
                                    continue;
                                const std::string nm =
                                    NameResolver::GetObjectNameA(ctx->image_base, cands[i]);
                                if (nm.empty() || nm.find(X("Stowed")) != std::string::npos)
                                    continue;
                                if (IsThreatItemActor(nm) ||
                                    nm.find(X("ItemActor")) != std::string::npos ||
                                    nm.find(X("Throwable")) != std::string::npos ||
                                    nm.find(X("Grenade")) != std::string::npos ||
                                    nm.find(X("Mine")) != std::string::npos ||
                                    nm.find(X("Trap")) != std::string::npos ||
                                    nm.find(X("Smoke")) != std::string::npos ||
                                    nm.find(X("Trigger")) != std::string::npos)
                                    heldActors.push_back((uint64_t)cands[i]);
                            }
                        };
                        collectHeld(inventory.GetCurrentItemActors());
                        collectHeld(inventory.GetLocalCurrentItems());
                    }
                    if (rawName.find(X("Stowed")) != std::string::npos)
                        rawName.clear();
                    if (rawName.empty()) {
                        uintptr_t da = inventory.GetStowedWeapon0DataAsset();
                        if (is_valid_pointer_optimized(da))
                            rawName = NameResolver::GetObjectNameA(ctx->image_base, da);
                    }
                    slowData.heldItem = PrettyHeldItemName(rawName);
                    slowData.weaponLevel.clear();
                    if (!slowData.heldItem.empty()) {
                        // "WeaponActor" only. A bare "Weapon" substring also matched
                        // BP_ItemActor_BasicMeleeWeapon_C, which has no quality field
                        // and was being labelled [I] off an unrelated byte.
                        static const std::string s_wa = std::string(X("WeaponActor"));
                        const bool isWeapon = rawName.find(s_wa) != std::string::npos;

                        int quality = -1;
                        if (haveActor && isWeapon) {
                            int8_t q8 = 0;
                            if (ReadItemActorWeaponQuality(itemActorPtr, q8))
                                quality = (int)q8;
                        }
                        // ResolveHeldItemActor comes back empty on roughly a third of
                        // polls; the name then comes from the stowed data asset, so take
                        // the quality from that same slot instead of giving up.
                        if (quality < 0 || quality > 3) {
                            const int32_t stowedQ = inventory.GetStowedWeapon0Quality();
                            if (stowedQ >= 0 && stowedQ <= 3)
                                quality = (int)stowedQ;
                        }
                        if (quality >= 0)
                            slowData.weaponLevel = WeaponQualityToLevel(quality);
                    }
                }

                // What the ESP will actually draw, which is the only figure worth
                // counting when judging the held-item gap.
                if (!pendingInvDiag.empty()) {
                    char oline[160];
                    snprintf(oline, sizeof(oline), " -> shown held=\"%s\" level=\"%s\"",
                        slowData.heldItem.c_str(), slowData.weaponLevel.c_str());
                    DebugSystem::LogConsole(pendingInvDiag + oline);
                }

                if (ctx->last_status() != TransportStatus::Ok)
                    slowData.stale = true;

                slowData.lastUpdateMs = GetNowMs();

                if (Entity->IsTransitioning()) {
                    fullPass = false;
                    break;
                }

                {
                    std::unique_lock<std::shared_mutex> lk(m_mtx);
                    m_cache[ent.Entity] = std::move(slowData);
                }
            }

            if (fullPass)
                HeldItems::Publish(heldActors);
            else
                HeldItems::Clear();
        }

    public:
        std::unordered_map<uintptr_t, SlowPlayerData> GetSnapshot() {
            SPOOF_FUNC;
            std::shared_lock<std::shared_mutex> lk(m_mtx);
            return m_cache;
        }

        void PruneStale(const std::unordered_set<uint64_t>& activeEntities) {
            SPOOF_FUNC;
            std::unique_lock<std::shared_mutex> lk(m_mtx);
            auto it = m_cache.begin();
            while (it != m_cache.end()) {
                if (activeEntities.find(it->first) == activeEntities.end())
                    it = m_cache.erase(it);
                else
                    ++it;
            }
        }

        void Clear() {
            std::unique_lock<std::shared_mutex> lk(m_mtx);
            m_cache.clear();
        }

        void CacheWithSEH() {
            Cache();
        }

    };

    class CAsyncWorldSync {
    private:
        std::unordered_map<uintptr_t, SlowWorldData> m_cache;
        std::shared_mutex m_mtx;

        std::vector<WorldDrawInfo>* m_worldSource = nullptr;
        std::shared_mutex* m_worldSourceMtx = nullptr;

    public:
        void SetWorldSource(std::vector<WorldDrawInfo>* src, std::shared_mutex* mtx) {
            m_worldSource = src;
            m_worldSourceMtx = mtx;
        }

    private:
        void Cache() {
            SPOOF_FUNC;
            if (Entity->IsTransitioning())
                return;

            if (!m_worldSource || !m_worldSourceMtx)
                return;

            std::vector<WorldDrawInfo> worldSnapshot;
            {
                std::shared_lock<std::shared_mutex> lk(*m_worldSourceMtx);
                worldSnapshot = *m_worldSource;
            }

            if (worldSnapshot.empty())
                return;

            for (auto& item : worldSnapshot) {
                if (Entity->IsTransitioning())
                    break;

                if (!is_valid_pointer_optimized(item.actor))
                    continue;

                SlowWorldData wd{};
                wd.stale = item.stale;
                UObject obj(item.actor);
                if (!obj.IsValid()) {
                    if (ctx->last_status() != TransportStatus::Ok)
                        wd.stale = true;
                    continue;
                }
                if (ctx->last_status() != TransportStatus::Ok)
                    wd.stale = true;

                wd.destroyed = obj.IsDestroyed();
                if (ctx->last_status() != TransportStatus::Ok)
                    wd.stale = true;

                if (item.type == WorldItemType::SocketContainer ||
                    item.type == WorldItemType::SalvageContainer ||
                    item.type == WorldItemType::RaiderCache)
                {
                    UInteractionComponent inter = obj.GetInteractionComponent();
                    if (inter.IsValid())
                    {
                        wd.searched = inter.IsOpened();
                    }
                    if (ctx->last_status() != TransportStatus::Ok)
                        wd.stale = true;
                }

                if (item.type == WorldItemType::Extraction)
                {
                    ASalvageExtractionPointBase ext(item.actor);
                    if (ext.IsValid())
                    {
                        wd.extractionState = static_cast<uint8_t>(ext.GetState());
                    }
                    if (ctx->last_status() != TransportStatus::Ok)
                        wd.stale = true;
                }

                if (item.type == WorldItemType::Arc)
                {
                    GetArcHealth(item.actor, wd.destroyed, wd.currentHealth, wd.maxHealth, wd.stale);
                    // Live health wins over a flaky IsDestroyed byte.
                    if (wd.maxHealth > 0.0 && wd.currentHealth > 0.0)
                        wd.destroyed = false;
                    else if (wd.maxHealth > 0.0 && wd.currentHealth <= 0.0)
                        wd.destroyed = true;
                }

                if (Entity->IsTransitioning())
                    break;

                {
                    std::unique_lock<std::shared_mutex> lk(m_mtx);
                    m_cache[item.actor] = std::move(wd);
                }
            }
        }

    public:
        std::unordered_map<uintptr_t, SlowWorldData> GetSnapshot() {
            SPOOF_FUNC;
            std::shared_lock<std::shared_mutex> lk(m_mtx);
            return m_cache;
        }

        void PruneStale(const std::unordered_set<uint64_t>& activeActors) {
            SPOOF_FUNC;
            std::unique_lock<std::shared_mutex> lk(m_mtx);
            auto it = m_cache.begin();
            while (it != m_cache.end()) {
                if (activeActors.find(it->first) == activeActors.end())
                    it = m_cache.erase(it);
                else
                    ++it;
            }
        }

        void Clear() {
            std::unique_lock<std::shared_mutex> lk(m_mtx);
            m_cache.clear();
        }

        void CacheWithSEH() {
            Cache();
        }

    };

    // Selector calibration scan (SelectorCalib) was removed after live data proved
    // the hypothesis wrong: every candidate bit produced ~87cm distances, which is
    // just the anatomical pelvis-to-mesh-origin distance for a standing character,
    // not a multi-meter divergence a wrong buffer pick would cause. Both slots hold
    // equivalent data - the double-buffer selector was never the problem. See
    // commit history for the removed scan if this needs revisiting.

    class CBoneSync {
    private:
        struct BoneResult {
            uint64_t entity;
            SkeletonWorldBones bones;
            std::unordered_map<Bones, Vector3> boneMap;
            bool valid;
            // Whether boneMap's Head/Root came off the mesh's bone array or from the
            // worldLoc approximation at the bottom of the per-target block. ApplyToCache
            // has to tell them apart: a real sample is allowed to refresh the position and
            // the headRootMs freshness stamp even when valid==false (spine/coherent only
            // gate the drawn skeleton), while an approximation must not overwrite either.
            bool headFromBones = false;
            bool rootFromBones = false;
            // Set when the pose was read successfully and then rejected for its shape,
            // as opposed to a read that never landed. ApplyToCache holds the previous
            // skeleton across the latter and must not across the former - see the hold
            // there for why the two cannot share one path.
            bool rejectedGeometry = false;
        };

        std::vector<BoneResult> m_results;
        std::unordered_set<uint64_t> m_frozenGhosts;
        std::shared_mutex m_mtx;

        typedef void* (*AcquireBufferFn)();
        typedef void (*ReleaseBufferFn)(void*);
        typedef const std::vector<PlayerDynamicData>* (*GetBufferDataFn)(void*);

        AcquireBufferFn m_acquireFn = nullptr;
        ReleaseBufferFn m_releaseFn = nullptr;
        GetBufferDataFn m_getDataFn = nullptr;

    public:
        void SetPlayerSource(AcquireBufferFn acq, ReleaseBufferFn rel, GetBufferDataFn getData) {
            m_acquireFn = acq;
            m_releaseFn = rel;
            m_getDataFn = getData;
        }

        bool IsFrozenGhost(uint64_t entity) {
            SPOOF_FUNC;
            std::shared_lock<std::shared_mutex> lk(m_mtx);
            return m_frozenGhosts.find(entity) != m_frozenGhosts.end();
        }

    private:
        void Cache() {
            SPOOF_FUNC;
            if (Entity->IsTransitioning())
                return;

            struct BoneTarget {
                uint64_t entity;
                uintptr_t skeletalMesh;
                Vector3 worldLoc;
                bool npc;
                std::string name;
                double currentHealth;
                double maxHealth;
                bool isDBNO;
                bool isDBNOKnown;
            };
            std::vector<BoneTarget> allTargets;

            // Players discarded here never reach the bone pass at all, so none of the failure
            // counters below can account for them - targets= only counts what got in, which is
            // why targets= and realPlayers could disagree with nothing to explain the gap.
            int droppedNoMesh = 0;

            if (m_acquireFn) {
                void* buf = m_acquireFn();
                if (buf) {
                    const auto* players = m_getDataFn(buf);
                    if (players) {
                        allTargets.reserve(players->size());
                        for (const auto& pd : *players) {
                            if (!is_valid_pointer_optimized(pd.entity))
                                continue;
                            if (!is_valid_pointer_optimized(pd.skeletalMesh)) {
                                // Only real players count. Every world-ESP entity shares this
                                // buffer and most carry no mesh by design, so testing npc alone
                                // reported 513 where the true figure was one - loot, containers
                                // and grenades are not players that lost a skeleton.
                                const bool worldItem = pd.npc || pd.loot || pd.container ||
                                    pd.salvage || pd.grenade || pd.mine || pd.trap ||
                                    pd.extraction || pd.raidercache || pd.corpse || pd.arcprobe;
                                if (!worldItem)
                                    droppedNoMesh++;
                                continue;
                            }
                            allTargets.push_back({ pd.entity, pd.skeletalMesh, pd.worldLoc, pd.npc, pd.name,
                                pd.currentHealth, pd.maxHealth, pd.isDBNO, pd.isDBNOKnown });
                        }
                    }
                    m_releaseFn(buf);
                }
            }

            if (allTargets.empty()) {
                static uint64_t s_emptyLog = 0;
                uint64_t nowEmpty = GetTickCount64();
                if (debugConsoleEnabled && (nowEmpty - s_emptyLog) > 2000) {
                    s_emptyLog = nowEmpty;
                    // noMesh is carried here too, otherwise the case where every player lost
                    // their mesh reports the same line as an empty lobby.
                    char eline[160];
                    snprintf(eline, sizeof(eline),
                        "[SlowCache] no bone targets (player buffer empty or no valid mesh) noMesh=%d",
                        droppedNoMesh);
                    DebugSystem::LogConsole(eline);
                }
                return;
            }

            PointerCache ptrs = Entity->GetPointerSnapshot();
            Vector3 localPos = ptrs.RelativeLocation;
            if (localPos.IsZero()) {
                CachedCameraData cam = Entity->GetCameraSnapshot();
                if (cam.valid && LooksValidWorldPos(cam.location) &&
                    (cam.uworld == 0 || ptrs.UWorld == 0 || cam.uworld == ptrs.UWorld))
                    localPos = cam.location;
            }

            // Confirm the double-buffer selector math against the local pawn's own mesh
            // (a known-good, always-present entity) before trusting it for remote
            // players. Samples roughly every 1000ms of WALL-CLOCK time for the first
            // 10s after the first successful read, capped at 10 samples.
            //
            // Bug fix: this used to gate the interval on a per-Cache()-call tick
            // counter ("every 60 ticks") while gating the overall window on wall
            // time. Those two are only equivalent if Cache() runs at a steady ~8ms,
            // which does not hold under real load (driver I/O per entity, entity
            // count, etc.) - a slower real cadence meant tick 120+ landed outside
            // the 10s wall-clock window before ever firing, so only 1-2 samples
            // (tick 1, maybe tick 60) ever printed. Both timers are wall-clock now.
            static std::atomic<bool> s_boneBufferDiagStarted{ false };
            static uint64_t s_boneBufferDiagStartMs = 0;
            static uint64_t s_boneBufferDiagLastSampleMs = 0;
            static int s_boneBufferDiagSampleCount = 0;
            constexpr uint64_t kBoneBufferDiagSampleIntervalMs = 1000;
            constexpr int kBoneBufferDiagMaxSamples = 10;
            constexpr uint64_t kBoneBufferDiagWindowMs = 10000;

            if (debugConsoleEnabled && s_boneBufferDiagSampleCount < kBoneBufferDiagMaxSamples &&
                is_valid_pointer_optimized(ptrs.LocalPawn))
            {
                uint64_t nowDiagMs = GetTickCount64();
                bool withinWindow = !s_boneBufferDiagStarted.load(std::memory_order_relaxed) ||
                    (nowDiagMs - s_boneBufferDiagStartMs) <= kBoneBufferDiagWindowMs;
                bool dueNow = s_boneBufferDiagSampleCount == 0 ||
                    (nowDiagMs - s_boneBufferDiagLastSampleMs) >= kBoneBufferDiagSampleIntervalMs;

                if (withinWindow && dueNow) {
                    uintptr_t localMesh = ctx->read<uintptr_t>(ptrs.LocalPawn + Offsets::Get().UPawn.Mesh);
                    if (is_valid_pointer_optimized(localMesh)) {
                        BoneArrayDebugInfo dbg = NameResolver::GetBoneArrayDebugInfo(localMesh);
                        if (dbg.valid) {
                            if (!s_boneBufferDiagStarted.load(std::memory_order_relaxed)) {
                                s_boneBufferDiagStarted.store(true, std::memory_order_relaxed);
                                s_boneBufferDiagStartMs = nowDiagMs;
                            }
                            s_boneBufferDiagLastSampleMs = nowDiagMs;
                            char line[256];
                            snprintf(line, sizeof(line),
                                "[BoneBuffer] sample=%d selector=%u readArray=0x%llX editArray=0x%llX numBones=%u",
                                s_boneBufferDiagSampleCount + 1,
                                dbg.selectorField,
                                (unsigned long long)dbg.readArrayPtr,
                                (unsigned long long)dbg.editArrayPtr,
                                dbg.numBones);
                            DebugSystem::LogConsole(line);
                            ++s_boneBufferDiagSampleCount;
                        }
                    }
                }
            }

            std::vector<BoneResult> results;
            results.reserve(allTargets.size());

            // skipMesh is unreachable: entry into allTargets already requires a valid mesh, so
            // the mesh test inside the loop below can never fire. It has been reading 0 for
            // that reason rather than because nothing fails - noMesh above is the real count.
            int skipMesh = 0, skipNpc = 0, skipMbc = 0, okResults = 0;
            int failNoSpine = 0, failLimbs = 0, failCoherent = 0, okFull = 0;
            int failRange = 0, skipGhost = 0, skipRagdoll = 0, skipFrozen = 0;
            std::unordered_set<uint64_t> frozenThisPass;
            std::unordered_set<uint64_t> thawedThisPass;
            struct FailSample {
                std::string name;
                std::string reason;
                uintptr_t mesh = 0;
                uint64_t boneArr = 0;
                Vector3 c2w{};
                float distM = 0.f;
                // Filled for any failure that implies the bone data itself is wrong.
                //
                // Originally mbc-only: GetBoneArrayPtr() has five separate return-0
                // branches (bad mesh, zero/invalid decrypt, count==0, invalid TArray
                // pointer) and GetBoneArrayPtrCached() adds a sixth by rejecting an
                // otherwise-good pointer via is_sane_bone_array_pointer(), and boneArr=0x0
                // alone cannot tell them apart.
                //
                // Widened because the recurring "one player per match has no skeleton"
                // cases never produce an mbc at all - the array resolves, it just decodes
                // to nonsense. Three of them across three logs: sk* with Z around 1e30,
                // Shaak with an all-zero spine, toestellweather with head and feet
                // coincident from 14m. Different symptoms, all consistent with the array
                // being shorter than the fixed indices read from it, since Head=7,
                // spine_02=3 and foot_l=71 past the end would give exactly that spread.
                // numBones is already captured here and is what settles it.
                BoneArrayDebugInfo dbg{};
                bool haveDbg = false;
                // What the slot search settled for. ScoreBoneLayout returns a soft floor
                // near -50 when a candidate is rejected on geometry, so a winner still down
                // there means nothing looked like a body and the pick was decided by the
                // tie-break bonus alone - a bad array chosen for lack of a better one.
                float mbcScore = -1.e9f;
                int slotsPresent = 0;
                RawBoneDump rawPelvis{};
                RawBoneDump rawHead{};
            };
            std::vector<FailSample> failSamples;
            FailSample okSample{};
            bool haveOkSample = false;

            // Tracks each entity's last mesh pointer that produced a valid bone context.
            // Cache() runs on a single dedicated thread (LaunchAsyncUpdater), so no lock needed.
            static std::unordered_map<uint64_t, uintptr_t> s_lastStableMesh;
            if (s_lastStableMesh.size() > 256)
                s_lastStableMesh.clear();

            // Consecutive bone-context misses per entity, used to disarm the Sleep(15) retry
            // below once an entity has proven it isn't a transient failure.
            static std::unordered_map<uint64_t, int> s_boneRetryMissStreak;
            if (s_boneRetryMissStreak.size() > 256)
                s_boneRetryMissStreak.clear();

            // Tracks per-entity componentToWorld staleness independent of the bone-array
            // pipeline entirely, to catch death/ragdoll state-decoupling: the mesh
            // component stops updating (frozen c2w) while the separate async worldLoc
            // poll keeps moving, so pelvis-from-boneArr vs worldLoc drift apart and the
            // coherent/spine checks fail even though nothing about bone reading is broken.
            struct MeshStaticTrack {
                Vector3 lastC2w{};
                bool hasC2w = false;
                int staticTicks = 0;
                uint64_t lastDiagLogMs = 0;

                Vector3 lastWorldLoc{};
                bool hasWorldLoc = false;
                bool worldLocMoving = false;

                // worldLoc as of the moment the mesh froze, and how far it has since travelled
                // from there. Displacement from an anchor, not a per-tick delta: see
                // kWorldLocDecoupleDistance below.
                Vector3 worldLocAtFreeze{};
                bool hasFreezeAnchor = false;
                uint64_t freezeStartMs = 0;

                bool skipLogged = false; // one-shot [MeshStatic] "skipping" log per entity
            };
            static std::unordered_map<uint64_t, MeshStaticTrack> s_meshStaticTrack;
            if (s_meshStaticTrack.size() > 256)
                s_meshStaticTrack.clear();
            constexpr double kMeshStaticMoveThreshold = 5.0;   // cm - below this counts as "unchanged"
            constexpr int kMeshStaticDiagTicks = 10;           // [MeshStatic] diagnostic log threshold
            constexpr int kMeshStaticRagdollTicks = 20;        // skip-render threshold
            // Debug 54 ghosts (CLIX/TOGDAEDAE) sat at ticksStatic=18k with freezeDrift=0
            // for the whole raid. Debug 55 live campers (Steve03, xBUTTHURTSx, YuhBoiVautz)
            // hit the old 20s wall-clock at only 319-632 ticks and got culled while still
            // on screen. Require both a long freeze and a large tick count so an angle-hold
            // does not look like an extracted leftover pawn.
            constexpr int kMeshStaticFrozenGhostTicks = 1500;
            constexpr uint64_t kMeshStaticFrozenGhostMs = 180000;

            // How far worldLoc must get from where it sat when the mesh froze before the two are
            // called decoupled. This replaced a per-tick "did worldLoc move more than 5cm since
            // last pass" test, which fired on any single jittery sample: a player standing still
            // holds a frozen c2w indefinitely (observed ticksStatic in the hundreds), so one
            // interpolation blip past 5cm was enough to declare a live, stationary player a
            // ragdoll and drop their ESP. The per-tick form was also comparing across mismatched
            // sample rates - c2w is read fresh here each pass while worldLoc arrives from the
            // slower entity poll, so consecutive passes often saw a byte-identical worldLoc.
            // Displacement from the freeze anchor is immune to both: jitter stays inside the
            // radius no matter how long the window, while a pawn whose position has genuinely
            // decoupled from its mesh keeps migrating away from it.
            constexpr double kWorldLocDecoupleDistance = 200.0; // cm

            // Bounds a head/foot pair has to satisfy before the torso fallback will
            // interpolate a spine between them. A standing pawn measures around 180cm sole
            // to crown; the range is loose enough for crouch and prone and for a foot read
            // that lands on the ankle instead of the sole, and the lean allowance covers a
            // fully horizontal dive without admitting a head that has landed somewhere
            // unrelated to its own legs.
            constexpr double kTorsoSynthMinRise = 60.0;   // cm
            constexpr double kTorsoSynthMaxRise = 250.0;  // cm
            constexpr double kTorsoSynthMaxLean = 120.0;  // cm, horizontal head-over-foot offset

            for (const auto& target : allTargets) {
                if (Entity->IsTransitioning())
                    break;

                // Mesh/npc checks come first so non-player world items (loot, containers,
                // corpses, etc. share this same buffer/struct) fall into skipMesh/skipNpc
                // like before, instead of inflating skipGhost - that inversion (ghost check
                // running first) is why skipGhost was exceeding realPlayers.
                if (!is_valid_pointer_optimized(target.skeletalMesh)) {
                    skipMesh++;
                    continue;
                }
                if (target.npc) {
                    skipNpc++;
                    continue;
                }

                // Corpse / spectator-ghost entities, evaluated only against players that
                // already passed the mesh+npc checks above.
                // - Name: only an actually-empty string is ghost-like. A short-but-present
                //   name ("K", "9") is a real player and must not be dropped for length.
                // - Health: maxHealth is written only when a fresh read is > 0 and is never
                //   reset to 0 elsewhere in the merge pipeline (see WorldCache.hpp), so
                //   maxHealth > 0 reliably means "health has loaded at least once" rather
                //   than "we read zero this frame".
                //
                // Zero health on its own is not death. A downed player reads 0 HP for as
                // long as they are bleeding out, so the old test dropped them from the bone
                // cache the instant they went down; bones then stopped being stamped and the
                // render freshness gate erased them a couple of seconds later, which is why
                // a knock made the whole overlay vanish instead of persisting until the kill
                // actually landed. Death now needs the DBNO flag to be clear as well, and
                // needs to stay that way - a single torn zero cannot end an entity's ESP,
                // and neither can one spurious clear of the DBNO byte. Genuinely static
                // corpses are still caught promptly by the ragdoll mesh-static detector
                // below, so this window does not put corpse ESP back on screen.
                struct DeathConfirm { uint64_t zeroSinceMs = 0; };
                static std::unordered_map<uint64_t, DeathConfirm> s_deathConfirm;
                // Two windows, because the confidence behind the decision differs. With a
                // validated downed flag saying "not downed", zero health is death and three
                // seconds is ample. Without one, zero health means only that - a knocked
                // player reads zero for as long as they are bleeding out, and killing their
                // ESP on that alone is what removed Mr man9570 236 times in one session
                // while he was lying there knocked, and ICEMAN9658 81 times before he was
                // revived back to 99 HP. The long window is a backstop against a corpse
                // lingering forever, not a death test; despawn and the ragdoll detector are
                // what actually retire dead players.
                constexpr uint64_t kDeathConfirmMs = 3000;
                constexpr uint64_t kDeathConfirmNoDbnoMs = 30000;

                const uint64_t nowDeathMs = GetTickCount64();
                const bool nameIsGhost = target.name.empty();
                const bool healthLoaded = target.maxHealth > 0.0;
                const bool healthZero = healthLoaded && target.currentHealth <= 0.0;

                bool healthConfirmedDead = false;
                if (healthZero && !target.isDBNO) {
                    if (s_deathConfirm.size() > 512)
                        s_deathConfirm.clear();
                    auto& dc = s_deathConfirm[target.entity];
                    if (dc.zeroSinceMs == 0)
                        dc.zeroSinceMs = nowDeathMs;
                    const uint64_t needMs = target.isDBNOKnown ? kDeathConfirmMs
                                                               : kDeathConfirmNoDbnoMs;
                    healthConfirmedDead = (nowDeathMs - dc.zeroSinceMs) >= needMs;
                } else {
                    s_deathConfirm.erase(target.entity);
                }

                if ((nameIsGhost && !healthLoaded) || healthConfirmedDead) {
                    skipGhost++;
                    if (debugConsoleEnabled) {
                        // Bug fix: this used to be a single global counter reset every 2s.
                        // Cache() re-evaluates every ghost-filtered entity on every tick (as
                        // fast as ~8ms), so within one 2s window the SAME handful of corpses
                        // (curHP=0/maxHP=100 "across the board", per report) blew through the
                        // shared budget in a fraction of a second and then reset and repeated -
                        // that's the "same entity spammed hundreds of times" symptom. Bucketed
                        // per entity name now: 3 logs per name per 5s, independent of every
                        // other entity and of how often Cache() actually runs.
                        struct GhostLogBucket { int count = 0; uint64_t windowStartMs = 0; };
                        static std::unordered_map<std::string, GhostLogBucket> s_ghostLogBuckets;
                        if (s_ghostLogBuckets.size() > 256)
                            s_ghostLogBuckets.clear();

                        const std::string& bucketKey = nameIsGhost ? "<empty>" : target.name;
                        auto& bucket = s_ghostLogBuckets[bucketKey];
                        uint64_t nowGhost = GetTickCount64();
                        if ((nowGhost - bucket.windowStartMs) > 5000) {
                            bucket.windowStartMs = nowGhost;
                            bucket.count = 0;
                        }
                        if (bucket.count < 3) {
                            ++bucket.count;
                            char gline[192];
                            snprintf(gline, sizeof(gline),
                                "[GhostFilter] name=%s hasHealth=%d curHP=%.0f maxHP=%.0f dbno=%d dbnoKnown=%d reason=%s",
                                nameIsGhost ? "<empty>" : target.name.c_str(),
                                healthLoaded ? 1 : 0,
                                target.currentHealth, target.maxHealth,
                                target.isDBNO ? 1 : 0,
                                target.isDBNOKnown ? 1 : 0,
                                nameIsGhost ? "emptyName" : "healthConfirmedDead");
                            DebugSystem::LogConsole(gline);
                        }
                    }
                    continue;
                }

                // Ragdoll / dead-body state-decoupling detection. Independent of the
                // bone-array/selector pipeline entirely - reads the mesh component's own
                // componentToWorld directly, since the actual bug is the mesh transform
                // freezing (death/ragdoll) while worldLoc (from the separate async vitals
                // poll) keeps moving, not anything about which bone buffer gets picked.
                {
                    auto& mst = s_meshStaticTrack[target.entity];
                    const auto& scMeshStatic = Offsets::Get().USceneComponent;
                    FTransform liveC2w = ctx->read<FTransform>(target.skeletalMesh + scMeshStatic.ComponentToWorld);
                    if (IsValidFTransform(liveC2w) && LooksValidWorldPos(liveC2w.translation)) {
                        if (mst.hasC2w && mst.lastC2w.distance(liveC2w.translation) <= kMeshStaticMoveThreshold) {
                            ++mst.staticTicks;
                            if (mst.freezeStartMs == 0)
                                mst.freezeStartMs = GetTickCount64();
                        } else {
                            if (mst.staticTicks > 0 || mst.freezeStartMs != 0)
                                thawedThisPass.insert(target.entity);
                            mst.staticTicks = 0;
                            mst.skipLogged = false;
                            mst.hasFreezeAnchor = false;
                            mst.worldLocMoving = false;
                            mst.freezeStartMs = 0;
                        }
                        mst.lastC2w = liveC2w.translation;
                        mst.hasC2w = true;
                    }

                    if (is_valid_bone_position(target.worldLoc)) {
                        // Anchor on the first static tick, then measure against that anchor.
                        if (mst.staticTicks > 0 && !mst.hasFreezeAnchor) {
                            mst.worldLocAtFreeze = target.worldLoc;
                            mst.hasFreezeAnchor = true;
                        }
                        if (mst.hasFreezeAnchor) {
                            mst.worldLocMoving =
                                mst.worldLocAtFreeze.distance(target.worldLoc) > kWorldLocDecoupleDistance;
                        }
                        mst.lastWorldLoc = target.worldLoc;
                        mst.hasWorldLoc = true;
                    }

                    if (mst.staticTicks >= kMeshStaticDiagTicks && debugConsoleEnabled) {
                        uint64_t nowMesh = GetTickCount64();
                        if ((nowMesh - mst.lastDiagLogMs) > 5000) {
                            mst.lastDiagLogMs = nowMesh;
                            // Centimetres between the mesh origin and the capsule origin, not
                            // a struct offset - the old name read like one and got mistaken
                            // for evidence of an unstable ComponentToWorld offset twice. It
                            // sits near 92cm standing and near 60cm crouched, so a spread of
                            // values across a log is stance, not drift. freezeDrift is what
                            // the decouple test below actually reads.
                            double meshToCapsuleCm = is_valid_bone_position(target.worldLoc)
                                ? mst.lastC2w.distance(target.worldLoc) : -1.0;
                            double freezeDrift = mst.hasFreezeAnchor && is_valid_bone_position(target.worldLoc)
                                ? mst.worldLocAtFreeze.distance(target.worldLoc) : -1.0;
                            char mline[256];
                            snprintf(mline, sizeof(mline),
                                "[MeshStatic] name=%s ticksStatic=%d c2w=(%.0f,%.0f,%.0f) meshToCapsuleCm=%.0f freezeDrift=%.0f",
                                target.name.empty() ? "<empty>" : target.name.c_str(),
                                mst.staticTicks, mst.lastC2w.x, mst.lastC2w.y, mst.lastC2w.z,
                                meshToCapsuleCm, freezeDrift);
                            DebugSystem::LogConsole(mline);
                        }
                    }

                    // Mesh frozen 20+ ticks while worldLoc keeps changing = state has
                    // decoupled (death/ragdoll). Treat as effectively dead and skip
                    // rendering even though curHP hasn't reported 0 yet.
                    if (mst.staticTicks >= kMeshStaticRagdollTicks && mst.worldLocMoving) {
                        if (!mst.skipLogged && debugConsoleEnabled) {
                            mst.skipLogged = true;
                            char rline[160];
                            snprintf(rline, sizeof(rline), "[MeshStatic] name=%s skipping ragdoll",
                                target.name.empty() ? "<empty>" : target.name.c_str());
                            DebugSystem::LogConsole(rline);
                        }
                        skipRagdoll++;
                        continue;
                    }

                    // Extracted / leftover PlayerArray pawns keep full HP and a frozen
                    // mesh+capsule (Debug 54: CLIX/TOGDAEDAE ticksStatic=18k, freezeDrift=0).
                    // Do not treat a 20s angle-hold as that — Debug 55 culled Steve03 /
                    // xBUTTHURTSx / YuhBoiVautz at 319-632 ticks and their ESP flickered
                    // because WorldCache then dropped them, which cleared the freeze set,
                    // which put them back next pass.
                    const uint64_t nowFrozenMs = GetTickCount64();
                    if (mst.freezeStartMs != 0 &&
                        (nowFrozenMs - mst.freezeStartMs) >= kMeshStaticFrozenGhostMs &&
                        mst.staticTicks >= kMeshStaticFrozenGhostTicks &&
                        !mst.worldLocMoving)
                    {
                        frozenThisPass.insert(target.entity);
                        if (!mst.skipLogged && debugConsoleEnabled) {
                            mst.skipLogged = true;
                            const double freezeDrift = mst.hasFreezeAnchor && is_valid_bone_position(target.worldLoc)
                                ? mst.worldLocAtFreeze.distance(target.worldLoc) : -1.0;
                            char rline[224];
                            snprintf(rline, sizeof(rline),
                                "[MeshStatic] name=%s skipping frozen ticksStatic=%d freezeMs=%llu freezeDrift=%.0f",
                                target.name.empty() ? "<empty>" : target.name.c_str(),
                                mst.staticTicks,
                                (unsigned long long)(nowFrozenMs - mst.freezeStartMs),
                                freezeDrift);
                            DebugSystem::LogConsole(rline);
                        }
                        skipFrozen++;
                        continue;
                    }
                }

                if (debugConsoleEnabled && target.name.length() > 0 && target.name.length() < 3) {
                    static int s_shortNameLogged = 0;
                    if (s_shortNameLogged < 20) {
                        ++s_shortNameLogged;
                        char sline[128];
                        snprintf(sline, sizeof(sline), "[GhostFilter] short name accepted: name=%s len=%zu",
                            target.name.c_str(), target.name.length());
                        DebugSystem::LogConsole(sline);
                    }
                }

                BoneResult result;
                result.entity = target.entity;
                result.valid = false;
                const char* failReason = nullptr;

                try {
                    uintptr_t meshForBones = target.skeletalMesh;
                    MeshBoneContext mbc = PrepareMeshBoneContext(meshForBones, target.worldLoc);
                    // Stale mesh on the player entry — refresh from pawn and retry once.
                    if (!mbc.valid && is_valid_pointer_optimized(target.entity)) {
                        uintptr_t freshMesh = ctx->read<uintptr_t>(
                            target.entity + Offsets::Get().UPawn.Mesh);
                        if (is_valid_pointer_optimized(freshMesh) && freshMesh != meshForBones) {
                            meshForBones = freshMesh;
                            mbc = PrepareMeshBoneContext(meshForBones, target.worldLoc);
                        }
                    }

                    // boneArr=0x0 on a mesh pointer that was stable last frame is usually a
                    // transient decrypt/read miss rather than a real invalidation — retry once
                    // after a short delay before giving up and marking it "mbc".
                    //
                    // Given up on after a few consecutive misses. The sleep runs on this worker
                    // thread inside the per-target loop, so one entity that resolves to nothing
                    // for minutes on end - which is exactly what a persistently unreadable mesh
                    // does - taxes every pass by 15ms and drags the whole bone update rate down
                    // for every other player. The retry only ever pays off for a genuinely
                    // transient miss, and the first (free) attempt above still runs every pass,
                    // so a mesh that recovers is picked up without needing the retry armed.
                    if (!mbc.valid && is_valid_pointer_optimized(meshForBones)) {
                        auto stableIt = s_lastStableMesh.find(target.entity);
                        const bool meshStable = stableIt != s_lastStableMesh.end() && stableIt->second == meshForBones;
                        constexpr int kMaxConsecutiveBoneRetries = 3;
                        auto& missStreak = s_boneRetryMissStreak[target.entity];
                        if (meshStable && missStreak < kMaxConsecutiveBoneRetries) {
                            Sleep(15);
                            mbc = PrepareMeshBoneContext(meshForBones, target.worldLoc);
                        }
                        if (!mbc.valid)
                            ++missStreak;
                        else
                            missStreak = 0;
                    }
                    else if (mbc.valid) {
                        s_boneRetryMissStreak.erase(target.entity);
                    }
                    if (is_valid_pointer_optimized(meshForBones))
                        s_lastStableMesh[target.entity] = meshForBones;

                    Vector3 lFoot{}, rFoot{};

                    if (!mbc.valid) {
                        skipMbc++;
                        failReason = "mbc";
                    } else {
                    auto boneW = [&](int idx) -> Vector3 {
                        return GetBoneFromContext(mbc, idx);
                    };

                    Vector3 headPos = boneW(Head);
                    Vector3 neckProbe = boneW(neck_02);
                    Vector3 pelvisProbe = boneW(pelvis);
                    lFoot = boneW(foot_l);
                    rFoot = boneW(foot_r);

                    // Trust mesh Head when near neck; otherwise approx from neck later.
                    const bool headOk = is_valid_bone_position(headPos) &&
                        is_valid_bone_position(neckProbe) &&
                        headPos.distance(neckProbe) <= 70.0 &&
                        (target.worldLoc.IsZero() || headPos.distance(target.worldLoc) <= 280.0);
                    if (headOk) {
                        result.boneMap[Head] = headPos;
                        result.headFromBones = true;
                    }

                    auto footNearPawn = [&](const Vector3& f) -> bool {
                        if (!is_valid_bone_position(f)) return false;
                        if (target.worldLoc.IsZero()) return true;
                        return f.distance(target.worldLoc) <= 260.0;
                    };
                    if (footNearPawn(lFoot) || footNearPawn(rFoot)) {
                        Vector3 footMid = target.worldLoc;
                        if (footNearPawn(lFoot) && footNearPawn(rFoot)) {
                            footMid.x = (lFoot.x + rFoot.x) * 0.5f;
                            footMid.y = (lFoot.y + rFoot.y) * 0.5f;
                            footMid.z = (std::min)(lFoot.z, rFoot.z);
                        } else if (footNearPawn(lFoot)) {
                            footMid = lFoot;
                        } else {
                            footMid = rFoot;
                        }
                        if (is_valid_bone_position(footMid)) {
                            result.boneMap[Root] = footMid;
                            result.rootFromBones = true;
                        }
                    }

                    // Follows the ESP max distance with hysteresis so border players don't pop,
                    // but capped on its own: distance_vis now runs to "unlimited" and a full
                    // bone read per player is the expensive half of the pipeline. Past this cap
                    // the player still draws - Loop.hpp's position-only path gives them a box
                    // without a pose - so raising the slider never costs bone work out to 1.5km.
                    // 450 is what the old slider ceiling gave, and that was affordable in
                    // practice - dropping it to 300 cost skeletons on everyone in the
                    // 340-490m band who used to have them. It exists only to stop
                    // "unlimited" draw distance from pulling bone reads out to 1.5km.
                    constexpr float kBoneResolveMaxM = 450.0f;
                    const float skelMaxM =
                        (std::min)((std::max)(250.0f, distance_vis), kBoneResolveMaxM) + 40.0f;
                    float distM = localPos.IsZero() ? 0.f
                        : (float)(localPos.distance(target.worldLoc) / 100.0f);
                    bool doFullSkeleton = (visuals5 || drawChams)
                        && !target.npc
                        && !localPos.IsZero()
                        && distM <= skelMaxM;

                    if (!doFullSkeleton) {
                        failRange++;
                        failReason = (!visuals5 && !drawChams) ? "skel_off" : "range";
                    } else {
                        Vector3 neckPos = neckProbe;
                        Vector3 chestPos = boneW(spine_02);
                        Vector3 pelvisPos = pelvisProbe;
                        // Fallback spine bones when primary indices miss on some meshes/LODs.
                        // is_valid_bone_position() only checks for a non-zero/finite read (a single
                        // transform read that failed on this frame), so a single-bone miss is common
                        // and transient — widen the fallback chain rather than treating it as fatal.
                        if (!is_valid_bone_position(chestPos))
                            chestPos = boneW(spine_01);
                        if (!is_valid_bone_position(chestPos))
                            chestPos = boneW(spine_03);
                        if (!is_valid_bone_position(neckPos))
                            neckPos = boneW(neck_01);
                        if (!is_valid_bone_position(neckPos))
                            neckPos = boneW(spine_03);
                        if (!is_valid_bone_position(pelvisPos))
                            pelvisPos = boneW(spine_01);

                        // Last resort: rebuild the torso from the head and the feet.
                        //
                        // Some meshes return exactly (0,0,0) for the entire spine chain while
                        // the head and both feet read fine - Shaak did it 171 times in one
                        // match, every spine bone zero, and that single gap was the largest
                        // source of missing skeletons in the log. Nothing in the chain above
                        // helps there, because every link it tries is part of the same dead
                        // run of indices.
                        //
                        // Head and foot are independent reads that did succeed, so the segment
                        // between them is real data; only the interior split is assumed, using
                        // proportions that hold for any upright stance. This is deliberately
                        // the last branch - a mesh that reports even one true spine bone keeps
                        // using it. The pelvis fallback that used to sit here took Root
                        // instead, which put the pelvis down at the soles and then failed
                        // coherence on span anyway.
                        bool haveTorsoTop = is_valid_bone_position(headPos) &&
                            (target.worldLoc.IsZero() || headPos.distance(target.worldLoc) <= 280.0);
                        Vector3 torsoFoot{};
                        bool haveTorsoFoot = false;
                        if (footNearPawn(lFoot) && footNearPawn(rFoot)) {
                            torsoFoot.x = (lFoot.x + rFoot.x) * 0.5f;
                            torsoFoot.y = (lFoot.y + rFoot.y) * 0.5f;
                            torsoFoot.z = (std::min)(lFoot.z, rFoot.z);
                            haveTorsoFoot = true;
                        } else if (footNearPawn(lFoot)) {
                            torsoFoot = lFoot;
                            haveTorsoFoot = true;
                        } else if (footNearPawn(rFoot)) {
                            torsoFoot = rFoot;
                            haveTorsoFoot = true;
                        }

                        // The head/foot pair has to look like a body before it is allowed to
                        // define one. headOk cannot stand in for this - it needs a valid neck,
                        // and a missing neck is the usual reason this branch runs at all - so
                        // the pair is checked against its own geometry instead: the head has
                        // to sit a plausible standing height above the soles and roughly over
                        // them. Without this the branch interpolates a torso out of a head
                        // that is already wrong, and the result surfaces downstream as
                        // coherent:spanShort, which reads as a collapsed skeleton rather than
                        // as the bad head read it actually is.
                        if (haveTorsoTop && haveTorsoFoot) {
                            const double rise = (double)headPos.z - (double)torsoFoot.z;
                            const double leanX = (double)headPos.x - (double)torsoFoot.x;
                            const double leanY = (double)headPos.y - (double)torsoFoot.y;
                            const double lean = std::sqrt(leanX * leanX + leanY * leanY);
                            if (rise < kTorsoSynthMinRise || rise > kTorsoSynthMaxRise ||
                                lean > kTorsoSynthMaxLean) {
                                haveTorsoTop = false;
                            }
                        }

                        if (haveTorsoTop && haveTorsoFoot) {
                            auto alongHeadToFoot = [&](float t) {
                                Vector3 p;
                                p.x = headPos.x + (torsoFoot.x - headPos.x) * t;
                                p.y = headPos.y + (torsoFoot.y - headPos.y) * t;
                                p.z = headPos.z + (torsoFoot.z - headPos.z) * t;
                                return p;
                            };
                            if (!is_valid_bone_position(neckPos))
                                neckPos = alongHeadToFoot(0.12f);
                            if (!is_valid_bone_position(chestPos))
                                chestPos = alongHeadToFoot(0.32f);
                            if (!is_valid_bone_position(pelvisPos))
                                pelvisPos = alongHeadToFoot(0.55f);
                        }

                        if (!(is_valid_bone_position(neckPos) && is_valid_bone_position(chestPos) && is_valid_bone_position(pelvisPos))) {
                            failNoSpine++;
                            failReason = "spine";

                            // One-shot per entity name: dump the raw spine chain to tell a
                            // bone-index shift apart from genuinely invalid geometry. Not a
                            // fix - just visibility, per the observed FATMOIST44-style case
                            // (spine consistently rejects across the whole run regardless of
                            // distance/movement - looks specific to that bone chain, not
                            // double-buffer/ragdoll/freshness related).
                            if (debugConsoleEnabled) {
                                static std::unordered_set<std::string> s_spineDiagLogged;
                                if (s_spineDiagLogged.size() > 256)
                                    s_spineDiagLogged.clear();
                                const std::string diagKey = target.name.empty() ? std::string("<empty>") : target.name;
                                if (s_spineDiagLogged.find(diagKey) == s_spineDiagLogged.end()) {
                                    s_spineDiagLogged.insert(diagKey);

                                    Vector3 rawPelvis = boneW(pelvis);
                                    Vector3 rawSpine01 = boneW(spine_01);
                                    Vector3 rawSpine02 = boneW(spine_02);
                                    Vector3 rawSpine03 = boneW(spine_03);

                                    char line1[384];
                                    snprintf(line1, sizeof(line1),
                                        "[SpineDiag] name=%s pelvis=(%.0f,%.0f,%.0f)%s spine01=(%.0f,%.0f,%.0f)%s "
                                        "spine02=(%.0f,%.0f,%.0f)%s spine03=(%.0f,%.0f,%.0f)%s chest(resolved)=(%.0f,%.0f,%.0f)%s",
                                        diagKey.c_str(),
                                        rawPelvis.x, rawPelvis.y, rawPelvis.z, is_valid_bone_position(rawPelvis) ? "" : "[INVALID]",
                                        rawSpine01.x, rawSpine01.y, rawSpine01.z, is_valid_bone_position(rawSpine01) ? "" : "[INVALID]",
                                        rawSpine02.x, rawSpine02.y, rawSpine02.z, is_valid_bone_position(rawSpine02) ? "" : "[INVALID]",
                                        rawSpine03.x, rawSpine03.y, rawSpine03.z, is_valid_bone_position(rawSpine03) ? "" : "[INVALID]",
                                        chestPos.x, chestPos.y, chestPos.z, is_valid_bone_position(chestPos) ? "" : "[INVALID]");
                                    DebugSystem::LogConsole(line1);

                                    double dPelvisS1 = (is_valid_bone_position(rawPelvis) && is_valid_bone_position(rawSpine01))
                                        ? rawPelvis.distance(rawSpine01) : -1.0;
                                    double dS1S2 = (is_valid_bone_position(rawSpine01) && is_valid_bone_position(rawSpine02))
                                        ? rawSpine01.distance(rawSpine02) : -1.0;
                                    double dS2S3 = (is_valid_bone_position(rawSpine02) && is_valid_bone_position(rawSpine03))
                                        ? rawSpine02.distance(rawSpine03) : -1.0;
                                    double dPelvisNeck = (is_valid_bone_position(rawPelvis) && is_valid_bone_position(neckPos))
                                        ? rawPelvis.distance(neckPos) : -1.0;
                                    char line2[256];
                                    snprintf(line2, sizeof(line2),
                                        "[SpineDiag] name=%s dist(pelvis-spine01)=%.0f dist(spine01-spine02)=%.0f "
                                        "dist(spine02-spine03)=%.0f dist(pelvis-neck)=%.0f",
                                        diagKey.c_str(), dPelvisS1, dS1S2, dS2S3, dPelvisNeck);
                                    DebugSystem::LogConsole(line2);
                                }
                            }
                        } else {
                            result.bones.neck = neckPos;
                            result.bones.chest = chestPos;
                            result.bones.pelvis = pelvisPos;
                            result.bones.l_shoulder = boneW(upperarm_l);
                            result.bones.l_elbow = boneW(lowerarm_l);
                            result.bones.l_hand = boneW(hand_l);
                            result.bones.r_shoulder = boneW(upperarm_r);
                            result.bones.r_elbow = boneW(lowerarm_r);
                            result.bones.r_hand = boneW(hand_r);
                            // Reject swapped/bad right-hand samples (common while downed).
                            if (is_valid_bone_position(result.bones.r_elbow) &&
                                is_valid_bone_position(result.bones.r_hand))
                            {
                                const float rhDist = result.bones.r_elbow.distance(result.bones.r_hand);
                                const float lhDist = is_valid_bone_position(result.bones.l_elbow)
                                    ? result.bones.l_elbow.distance(result.bones.r_hand)
                                    : 1.0e9f;
                                if (rhDist > 120.0f || lhDist + 15.0f < rhDist)
                                {
                                    Vector3 ikHand = boneW(ik_hand_r);
                                    if (is_valid_bone_position(ikHand) &&
                                        result.bones.r_elbow.distance(ikHand) <= 120.0f)
                                        result.bones.r_hand = ikHand;
                                    else
                                        result.bones.r_hand = result.bones.r_elbow;
                                }
                            }
                            result.bones.l_thigh = boneW(thigh_l);
                            result.bones.l_knee = boneW(calf_l);
                            result.bones.l_foot = lFoot;
                            result.bones.r_thigh = boneW(thigh_r);
                            result.bones.r_knee = boneW(calf_r);
                            result.bones.r_foot = rFoot;

                            // Alternate feet (ball_/ik_) when primary foot bones miss.
                            if (!is_valid_bone_position(result.bones.l_foot)) {
                                Vector3 alt = boneW(ball_l);
                                if (!is_valid_bone_position(alt)) alt = boneW(ik_foot_l);
                                if (is_valid_bone_position(alt)) result.bones.l_foot = alt;
                            }
                            if (!is_valid_bone_position(result.bones.r_foot)) {
                                Vector3 alt = boneW(ball_r);
                                if (!is_valid_bone_position(alt)) alt = boneW(ik_foot_r);
                                if (is_valid_bone_position(alt)) result.bones.r_foot = alt;
                            }

                            const bool hasShoulders =
                                is_valid_bone_position(result.bones.l_shoulder) ||
                                is_valid_bone_position(result.bones.r_shoulder);
                            const bool hasFeet =
                                is_valid_bone_position(result.bones.l_foot) ||
                                is_valid_bone_position(result.bones.r_foot);
                            const char* coherentWhy = "ok";
                            const bool coherent = IsCoherentPlayerSkeleton(
                                headOk ? headPos : Vector3{},
                                neckPos, pelvisPos,
                                result.bones.l_foot, result.bones.r_foot, target.worldLoc,
                                &coherentWhy);

                            // Accept partial skeletons (one side / no head) as long as spine + limbs attach.
                            if (!hasShoulders || !hasFeet) {
                                failLimbs++;
                                failReason = !hasShoulders ? "shoulders" : "feet";
                            } else if (!coherent) {
                                failCoherent++;
                                // Carries the specific rule that rejected the pose, so a
                                // player who never gets a skeleton can be traced to one
                                // check instead of all seven.
                                static thread_local char coherentLabel[48];
                                snprintf(coherentLabel, sizeof(coherentLabel), "coherent:%s", coherentWhy);
                                failReason = coherentLabel;
                                // The bones were read; they just do not describe a body.
                                // Marked so the hold in ApplyToCache cannot keep presenting
                                // this pose as a good one.
                                result.rejectedGeometry = true;

                                // One shot per name+rule: print the coordinates the rejected
                                // rule actually saw. spanShort in particular only reports that
                                // crown-to-sole came out too short, which cannot distinguish a
                                // genuinely collapsed read from a torso this code synthesised
                                // out of a bad head - the raw values separate the two, and the
                                // synth flags say whether the pose is measured or interpolated.
                                if (debugConsoleEnabled) {
                                    static std::unordered_set<std::string> s_coherentDiagLogged;
                                    if (s_coherentDiagLogged.size() > 256)
                                        s_coherentDiagLogged.clear();
                                    const std::string diagKey =
                                        (target.name.empty() ? std::string("<empty>") : target.name) +
                                        "|" + coherentWhy;
                                    if (s_coherentDiagLogged.find(diagKey) == s_coherentDiagLogged.end()) {
                                        s_coherentDiagLogged.insert(diagKey);

                                        const Vector3 rawHead = boneW(Head);
                                        const Vector3 coherHead = headOk ? headPos : Vector3{};
                                        const float footZ = (std::min)(
                                            is_valid_bone_position(result.bones.l_foot) ? result.bones.l_foot.z : 1e9f,
                                            is_valid_bone_position(result.bones.r_foot) ? result.bones.r_foot.z : 1e9f);
                                        const double span = (is_valid_bone_position(coherHead) && footZ < 1e8f)
                                            ? (double)coherHead.z - (double)footZ : -1.0;

                                        char cline[448];
                                        snprintf(cline, sizeof(cline),
                                            "[CoherDiag] name=%s why=%s headOk=%d rawHead=(%.0f,%.0f,%.0f) "
                                            "head=(%.0f,%.0f,%.0f) neck=(%.0f,%.0f,%.0f) pelvis=(%.0f,%.0f,%.0f) "
                                            "footL=(%.0f,%.0f,%.0f) footR=(%.0f,%.0f,%.0f) worldLoc=(%.0f,%.0f,%.0f) "
                                            "span=%.0f synthTop=%d synthFoot=%d",
                                            diagKey.c_str(), coherentWhy, headOk ? 1 : 0,
                                            rawHead.x, rawHead.y, rawHead.z,
                                            coherHead.x, coherHead.y, coherHead.z,
                                            neckPos.x, neckPos.y, neckPos.z,
                                            pelvisPos.x, pelvisPos.y, pelvisPos.z,
                                            result.bones.l_foot.x, result.bones.l_foot.y, result.bones.l_foot.z,
                                            result.bones.r_foot.x, result.bones.r_foot.y, result.bones.r_foot.z,
                                            target.worldLoc.x, target.worldLoc.y, target.worldLoc.z,
                                            span, haveTorsoTop ? 1 : 0, haveTorsoFoot ? 1 : 0);
                                        DebugSystem::LogConsole(cline);
                                    }
                                }
                            } else {
                                if (headOk)
                                    result.boneMap[Head] = headPos;
                                else {
                                    Vector3 approxHead = neckPos;
                                    approxHead.z += 12.0f;
                                    result.boneMap[Head] = approxHead;
                                }
                                result.boneMap[neck_02] = neckPos;
                                result.boneMap[spine_02] = chestPos;
                                result.boneMap[pelvis] = pelvisPos;
                                if (is_valid_bone_position(result.bones.l_shoulder))
                                    result.boneMap[upperarm_l] = result.bones.l_shoulder;
                                if (is_valid_bone_position(result.bones.l_elbow))
                                    result.boneMap[lowerarm_l] = result.bones.l_elbow;
                                if (is_valid_bone_position(result.bones.l_hand))
                                    result.boneMap[hand_l] = result.bones.l_hand;
                                if (is_valid_bone_position(result.bones.r_shoulder))
                                    result.boneMap[upperarm_r] = result.bones.r_shoulder;
                                if (is_valid_bone_position(result.bones.r_elbow))
                                    result.boneMap[lowerarm_r] = result.bones.r_elbow;
                                if (is_valid_bone_position(result.bones.r_hand))
                                    result.boneMap[hand_r] = result.bones.r_hand;
                                if (is_valid_bone_position(result.bones.l_thigh))
                                    result.boneMap[thigh_l] = result.bones.l_thigh;
                                if (is_valid_bone_position(result.bones.l_knee))
                                    result.boneMap[calf_l] = result.bones.l_knee;
                                if (is_valid_bone_position(result.bones.l_foot))
                                    result.boneMap[foot_l] = result.bones.l_foot;
                                if (is_valid_bone_position(result.bones.r_thigh))
                                    result.boneMap[thigh_r] = result.bones.r_thigh;
                                if (is_valid_bone_position(result.bones.r_knee))
                                    result.boneMap[calf_r] = result.bones.r_knee;
                                if (is_valid_bone_position(result.bones.r_foot))
                                    result.boneMap[foot_r] = result.bones.r_foot;

                                // Intermediate spine/neck links and the toe bones. Not
                                // part of any validity check - the skeleton is already
                                // accepted by this point - they only refine the drawn
                                // pose, so each is written independently and a mesh that
                                // does not resolve one simply keeps the coarser chain.
                                // Range-gated against the pawn origin like everything
                                // else here so a bad decrypt cannot stretch a limb.
                                {
                                    static const Bones kDetailBones[] = {
                                        spine_01, spine_03, neck_01, ball_l, ball_r
                                    };
                                    for (Bones db : kDetailBones) {
                                        Vector3 dp = boneW(db);
                                        if (is_valid_bone_position(dp) &&
                                            (target.worldLoc.IsZero() ||
                                             dp.distance(target.worldLoc) <= 260.0))
                                            result.boneMap[db] = dp;
                                    }
                                }
                                result.valid = true;
                                okFull++;
                                if (!haveOkSample) {
                                    okSample.name = target.name.empty() ? "?" : target.name;
                                    okSample.reason = "ok";
                                    okSample.mesh = meshForBones;
                                    okSample.boneArr = mbc.boneArray;
                                    okSample.c2w = mbc.componentToWorld.translation;
                                    haveOkSample = true;
                                }
                            }
                        }
                    }
                    } // mbc.valid

                    if (!result.valid && failReason && failSamples.size() < 4) {
                        FailSample fs;
                        fs.name = target.name.empty() ? "?" : target.name;
                        fs.reason = failReason;
                        fs.mesh = meshForBones;
                        fs.boneArr = mbc.boneArray;
                        fs.c2w = mbc.componentToWorld.translation;
                        fs.distM = localPos.IsZero() ? -1.f
                            : (float)(localPos.distance(target.worldLoc) / 100.0f);
                        // range and skel_off are policy gates, not read failures, so they
                        // stay out - dumping slot state for a player who is simply too far
                        // away would bury the cases that matter.
                        const bool boneDataFailure =
                            fs.reason == "mbc" || fs.reason == "spine" ||
                            fs.reason == "shoulders" || fs.reason == "feet" ||
                            fs.reason.rfind("coherent:", 0) == 0;
                        if (boneDataFailure && is_valid_pointer_optimized(meshForBones)) {
                            fs.dbg = NameResolver::GetBoneArrayDebugInfo(meshForBones);
                            fs.haveDbg = true;
                            fs.mbcScore = mbc.chosenScore;
                            fs.slotsPresent = mbc.slotsPresent;
                            fs.rawPelvis = ReadRawBoneDump(mbc, (int)Bones::pelvis);
                            fs.rawHead = ReadRawBoneDump(mbc, (int)Bones::Head);
                        }
                        failSamples.push_back(std::move(fs));
                    }

                    // Mesh/MBC can miss while root worldLoc is still good — keep ESP alive.
                    if (is_valid_bone_position(target.worldLoc)) {
                        if (result.boneMap.find(Head) == result.boneMap.end()) {
                            Vector3 headApprox = target.worldLoc;
                            headApprox.z += 90.0f;
                            result.boneMap[Head] = headApprox;
                        }
                        if (result.boneMap.find(Root) == result.boneMap.end())
                            result.boneMap[Root] = target.worldLoc;
                    }

                    if (!result.boneMap.empty()) {
                        okResults++;
                        results.push_back(std::move(result));
                    }
                }
                catch (...) {
                    continue;
                }
            }

            if (debugConsoleEnabled) {
                static uint64_t s_lastBoneLog = 0;
                uint64_t nowLog = GetTickCount64();
                if ((nowLog - s_lastBoneLog) > 2000) {
                    s_lastBoneLog = nowLog;
                    char line[384];
                    snprintf(line, sizeof(line),
                        "[SlowCache] targets=%d results=%d fullOk=%d skipMbc=%d skipNpc=%d skipMesh=%d "
                        "noMesh=%d skipGhost=%d skipRagdoll=%d skipFrozen=%d failSpine=%d failLimbs=%d failCoh=%d failRange=%d",
                        (int)allTargets.size(), okResults, okFull, skipMbc, skipNpc, skipMesh,
                        droppedNoMesh, skipGhost, skipRagdoll, skipFrozen, failNoSpine, failLimbs, failCoherent, failRange);
                    DebugSystem::LogConsole(line);

                    if (haveOkSample) {
                        snprintf(line, sizeof(line),
                            "[BoneDiag] OK name=%s mesh=0x%llX boneArr=0x%llX c2w=(%.0f,%.0f,%.0f)",
                            okSample.name.c_str(),
                            (unsigned long long)okSample.mesh,
                            (unsigned long long)okSample.boneArr,
                            okSample.c2w.x, okSample.c2w.y, okSample.c2w.z);
                        DebugSystem::LogConsole(line);
                    }
                    for (const auto& fs : failSamples) {
                        snprintf(line, sizeof(line),
                            "[BoneDiag] FAIL name=%s reason=%s dist=%.0fm mesh=0x%llX boneArr=0x%llX c2w=(%.0f,%.0f,%.0f)",
                            fs.name.c_str(), fs.reason.c_str(), fs.distM,
                            (unsigned long long)fs.mesh,
                            (unsigned long long)fs.boneArr,
                            fs.c2w.x, fs.c2w.y, fs.c2w.z);
                        DebugSystem::LogConsole(line);

                        if (fs.haveDbg) {
                            // Throttled per name. The widened trigger fires every pass for a
                            // persistently broken mesh, and three such players turned this
                            // one line into the bulk of a 637KB log, burying everything else.
                            // The slot state barely moves between passes - the selector
                            // alternates and the two buffers swap roles, nothing more - so a
                            // line every few seconds carries the same information.
                            static std::unordered_map<std::string, uint64_t> s_boneSlotLogMs;
                            if (s_boneSlotLogMs.size() > 256)
                                s_boneSlotLogMs.clear();
                            uint64_t& lastSlotLog = s_boneSlotLogMs[fs.name];
                            const uint64_t nowSlot = GetTickCount64();
                            if ((nowSlot - lastSlotLog) > 5000) {
                                lastSlotLog = nowSlot;
                                // chose= says which of the two buffers the geometry scoring
                                // actually settled on. If failing meshes consistently land
                                // on "edit" while healthy ones land on "read", the selector
                                // maths in Steam.hpp is inverted for this build and the
                                // constant can be corrected rather than scored around.
                                const char* chose =
                                    (fs.boneArr != 0 && fs.boneArr == fs.dbg.readArrayPtr) ? "read" :
                                    (fs.boneArr != 0 && fs.boneArr == fs.dbg.editArrayPtr) ? "edit" :
                                    (fs.boneArr != 0 && fs.boneArr == fs.dbg.readArrayPtr + kBoneArrayBaseOff) ? "read+base" :
                                    (fs.boneArr != 0 && fs.boneArr == fs.dbg.editArrayPtr + kBoneArrayBaseOff) ? "edit+base" : "other";
                                snprintf(line, sizeof(line),
                                    "[BoneSlot] name=%s dbgValid=%d selector=%u slot=0x%X "
                                    "readArray=0x%llX editArray=0x%llX numBones=%u readSane=%d editSane=%d "
                                    "chose=%s score=%.0f slotsPresent=%d",
                                    fs.name.c_str(), fs.dbg.valid ? 1 : 0,
                                    fs.dbg.selectorField, fs.dbg.selectedSlot,
                                    (unsigned long long)fs.dbg.readArrayPtr,
                                    (unsigned long long)fs.dbg.editArrayPtr,
                                    fs.dbg.numBones,
                                    is_sane_bone_array_pointer(fs.dbg.readArrayPtr) ? 1 : 0,
                                    is_sane_bone_array_pointer(fs.dbg.editArrayPtr) ? 1 : 0,
                                    chose, fs.mbcScore, fs.slotsPresent);
                                DebugSystem::LogConsole(line);

                                // Pelvis is the reference: in a rest pose its component
                                // space translation is small and its quaternion is unit. A
                                // non-unit quat means the field layout is wrong; a sane quat
                                // with an oversized translation or scale means it is units.
                                const RawBoneDump& rb = fs.rawPelvis;
                                const RawBoneDump& rh = fs.rawHead;
                                if (rb.ok) {
                                    snprintf(line, sizeof(line),
                                        "[BoneRaw] name=%s pelvis q=(%.3f,%.3f,%.3f,%.3f) qn=%.4f "
                                        "t=(%.2f,%.2f,%.2f) s=(%.3f,%.3f,%.3f)",
                                        fs.name.c_str(), rb.qx, rb.qy, rb.qz, rb.qw, rb.qnorm,
                                        rb.tx, rb.ty, rb.tz, rb.sx, rb.sy, rb.sz);
                                    DebugSystem::LogConsole(line);
                                    snprintf(line, sizeof(line),
                                        "[BoneRaw] name=%s head qn=%.4f t=(%.2f,%.2f,%.2f) "
                                        "s=(%.3f,%.3f,%.3f) c2wT=(%.1f,%.1f,%.1f) "
                                        "c2wS=(%.3f,%.3f,%.3f) c2wQn=%.4f",
                                        fs.name.c_str(), rh.qnorm, rh.tx, rh.ty, rh.tz,
                                        rh.sx, rh.sy, rh.sz,
                                        rb.c2wx, rb.c2wy, rb.c2wz,
                                        rb.c2wsx, rb.c2wsy, rb.c2wsz, rb.c2wqnorm);
                                    DebugSystem::LogConsole(line);
                                }
                            }
                        }
                    }
                }
            }

            {
                std::unique_lock<std::shared_mutex> lk(m_mtx);
                m_results = std::move(results);
                // Merge, do not replace. Replacing with this-pass-only froze anyone who
                // was dropped from the player buffer (the old WorldCache skip) and then
                // immediately unfroze them next pass — ESP on/off every bone cycle.
                for (uint64_t e : frozenThisPass)
                    m_frozenGhosts.insert(e);
                for (uint64_t e : thawedThisPass)
                    m_frozenGhosts.erase(e);
            }
        }

    public:
        void ApplyToCache(std::vector<PlayerDynamicData>& players) {
            SPOOF_FUNC;
            std::shared_lock<std::shared_mutex> lk(m_mtx);
            if (m_results.empty())
                return;

            std::unordered_map<uint64_t, size_t> indexMap;
            indexMap.reserve(players.size());
            for (size_t i = 0; i < players.size(); ++i) {
                indexMap[players[i].entity] = i;
            }

            // Un-pin state for the 900u outlier clamp below, keyed by entity and bone. Only
            // ever touched from IndexPlayers()'s single producer thread, same as the worldLoc
            // recovery map in WorldCache.hpp.
            struct BoneJumpRecovery {
                Vector3 candidate{};
                int agree = 0;
            };
            static std::unordered_map<uint64_t, BoneJumpRecovery> s_boneJumpRecovery;
            if (s_boneJumpRecovery.size() > 1024)
                s_boneJumpRecovery.clear();
            constexpr int kBoneAdoptSamples = 3;

            for (const auto& res : m_results) {
                auto it = indexMap.find(res.entity);
                if (it == indexMap.end()) continue;
                auto& pd = players[it->second];

                auto headIt = pd.cachedBones.find(Head);
                if (headIt != pd.cachedBones.end()) {
                    pd.previous_head_pos = headIt->second;
                }

                const uint64_t nowMs = GetTickCount64();
                for (const auto& [bone, pos] : res.boneMap) {
                    const bool isAnchor = (bone == Head || bone == Root);
                    auto prevIt = pd.cachedBones.find(bone);
                    const bool havePrev = prevIt != pd.cachedBones.end() &&
                        is_valid_bone_position(prevIt->second);
                    const bool posOk = is_valid_bone_position(pos);
                    const uint64_t jumpKey =
                        res.entity ^ ((static_cast<uint64_t>(bone) & 0xFFull) << 56);

                    // Outlier clamp, with the same confirm-then-adopt recovery the worldLoc
                    // clamp in WorldCache.hpp uses, and for the same reason: the jump is
                    // measured against the cached value, so a plain reject is one-way. A bone
                    // that genuinely moves more than 9m between passes - a fall, a zipline, a
                    // respawn - would pin its cached position permanently, and nothing after
                    // that could ever agree with the pin. K_HyuGa___3B held a Root 43m above
                    // his own mesh for an entire match that way, and the render path happily
                    // drew a full-size box on the empty rooftop where that stale point sat.
                    // A single bad read does not land in the same wrong place three times; a
                    // real move keeps reporting from where the player actually went.
                    if (havePrev && posOk && prevIt->second.distance(pos) >= 900.0f) {
                        auto& rec = s_boneJumpRecovery[jumpKey];
                        if (rec.agree > 0 && rec.candidate.distance(pos) <= 900.0) {
                            ++rec.agree;
                        } else {
                            rec.candidate = pos;
                            rec.agree = 1;
                        }

                        if (rec.agree < kBoneAdoptSamples) {
                            // No headRootMs stamp on this path on purpose. The cached
                            // position and the live sample disagree and we have not decided
                            // which one is right yet, so the point the render path would draw
                            // is precisely the one being contradicted. Withholding the stamp
                            // lets the 2-second gate in Loop.hpp cull the entity if the
                            // disagreement outlasts the adopt window, which is what stops a
                            // stale anchor from being drawn as a ghost.
                            continue;
                        }

                        rec.agree = 0;
                        pd.cachedBones[bone] = pos;
                        if (isAnchor)
                            pd.headRootMs = nowMs;
                        continue;
                    }
                    s_boneJumpRecovery.erase(jumpKey);

                    // Head/Root are never erased below (kLimbBones excludes them), so this
                    // timestamp is the render path's only way to tell a fresh position from
                    // one that has been sitting untouched for minutes. Stamped ahead of the
                    // remaining guards: those decide whether the cached position is worth
                    // replacing, which is a different question from whether the entity is
                    // still reporting, and both of them exit by continue.
                    if (isAnchor && posOk)
                        pd.headRootMs = nowMs;

                    // Approx Head/Root fallback must not wipe a real skeleton sample. Keyed on
                    // where this specific sample came from, not on res.valid: Head and Root are
                    // read off the bone array before the spine/coherent/range checks run, so a
                    // result can be invalid while still carrying a perfectly good Head and Root.
                    // Head+Root alone are what the ESP box needs; spine/coherent decide whether
                    // the skeleton draws, nothing more.
                    const bool approxSample = (bone == Head && !res.headFromBones) ||
                        (bone == Root && !res.rootFromBones);
                    if (approxSample && havePrev && isAnchor)
                    {
                        const float jump = prevIt->second.distance(pos);
                        if (jump < 250.0f)
                            continue;
                    }
                    if (havePrev && posOk)
                    {
                        const float jump = prevIt->second.distance(pos);
                        // Light blend only on tiny jitter — heavy blending lagged ESP and flickered.
                        if (jump > 0.5f && jump < 40.0f)
                        {
                            prevIt->second.x = prevIt->second.x * 0.20f + pos.x * 0.80f;
                            prevIt->second.y = prevIt->second.y * 0.20f + pos.y * 0.80f;
                            prevIt->second.z = prevIt->second.z * 0.20f + pos.z * 0.80f;
                            continue;
                        }
                    }
                    pd.cachedBones[bone] = pos;
                }

                if (res.valid) {
                    pd.fullSkeleton = true;
                    pd.fullSkeletonMs = nowMs;
                    pd.skeletonBones = res.bones;
                    pd.bonesPredicted = false;
                } else {
                    // Keep last good skeleton briefly — bone decrypt/LOD misses are common and
                    // usually resolve next frame. Use kBoneHoldMaxAgeMs as the freshness window and
                    // kCoherentPelvisWorldLocTolerance (SDK.hpp) so this doesn't reject cached data
                    // any tighter than the coherence check that decided res was invalid in the first place.
                    //
                    // A pose rejected on its geometry is explicitly not held. The hold cannot
                    // actually preserve the last good skeleton the way its name suggests: the
                    // loop above has already written this result's Head and Root into
                    // cachedBones (deliberately - the ESP box needs them even when the skeleton
                    // is unusable), so all the hold keeps is the fullSkeleton flag, and what
                    // then renders is the new bad anchor wearing the previous frame's limbs.
                    // For a mesh that alternates between good and collapsed reads the hold
                    // never lapses, so that hybrid is on screen continuously - observed as
                    // Dang3rDoom's skeleton lying flat at the feet, every bone sharing the
                    // mesh origin's Z while the checker rejected the pose 94 times running.
                    // A read that never landed leaves the old pose intact and is still worth
                    // riding out; a read that landed and came back the wrong shape is not.
                    static constexpr uint64_t kBoneHoldMaxAgeMs = 500;
                    const bool holdPrev = !res.rejectedGeometry &&
                        pd.fullSkeleton &&
                        pd.fullSkeletonMs != 0 &&
                        (nowMs - pd.fullSkeletonMs) <= kBoneHoldMaxAgeMs &&
                        is_valid_bone_position(pd.worldLoc) &&
                        pd.cachedBones.count(pelvis) != 0 &&
                        pd.cachedBones[pelvis].distance(pd.worldLoc) <= kCoherentPelvisWorldLocTolerance;
                    if (!holdPrev) {
                        static const Bones kLimbBones[] = {
                            neck_01, neck_02, spine_01, spine_02, spine_03, pelvis,
                            upperarm_l, lowerarm_l, hand_l,
                            upperarm_r, lowerarm_r, hand_r,
                            thigh_l, calf_l, foot_l, ball_l,
                            thigh_r, calf_r, foot_r, ball_r
                        };
                        for (Bones b : kLimbBones)
                            pd.cachedBones.erase(b);
                        pd.skeletonBones = {};
                        pd.fullSkeleton = false;
                        pd.fullSkeletonMs = 0;
                        pd.bonesPredicted = false;
                    } else {
                        // Stale-but-fresh sample is being reused for rendering this frame.
                        pd.bonesPredicted = true;
                    }
                }
                // Head+Root is enough for ESP/aim box; skeleton draws only when fullSkeleton.
                pd.bones_valid = (pd.cachedBones.count(Head) != 0 && pd.cachedBones.count(Root) != 0);
            }
        }

        void Clear() {
            std::unique_lock<std::shared_mutex> lk(m_mtx);
            m_results.clear();
            m_frozenGhosts.clear();
        }

        void CacheWithSEH() {
            Cache();
        }

    };

    inline CAsyncPlayerSync* AsyncPlayer = nullptr;
    inline CAsyncWorldSync* AsyncWorld = nullptr;
    inline CBoneSync* BoneSyncer = nullptr;

}
