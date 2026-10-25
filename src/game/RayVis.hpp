#pragma once

#include "EntityCache.hpp"
#include "dgb/Spoof.h"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <mutex>
#include <utility>
#include <shared_mutex>
#include <unordered_map>
#include <unordered_set>
#include <vector>

// External CPU vischeck: camera -> head vs UBodySetup AggGeom copied out of the game.
// Fail open while the cache is empty. Fail closed on a hit once a snapshot is published.
// Landscape / complex TriMesh is not in AggGeom; PhysX is not wired (no FPhysScene offset).

static_assert(sizeof(Vector3) == 24, "AggGeom FVector is LWC (3 doubles); Offsets UBodySetup.AggGeom is the field, not the vector size");
static_assert(sizeof(TArray) == 16, "FKAggregateGeom is four inline TArrays (64 bytes)");
static_assert(sizeof(FQuat) == 32, "ComponentToWorld FTransform uses a four-double quat");
static_assert(sizeof(FTransform) == 96, "FQuat 32 + translation 24 + pad 8 + scale 24 + pad 8");

namespace RayVis {

    constexpr double kOriginBiasUu = 12.0;
    constexpr double kSelfHitUu = 10.0;
    constexpr double kSkinUu = 8.0;
    constexpr double kCacheRadiusUu = 40000.0;
    constexpr double kTinyExtentUu = 8.0;
    constexpr double kInsaneExtentUu = 80000.0;
    constexpr int kMaxInstances = 8192;
    constexpr int kMinReadyInstances = 16;
    constexpr int kMaxElems = 64;
    constexpr int kMaxConvexVerts = 48;
    constexpr DWORD kRebuildSleepMs = 160;

    struct SpherePrim { Vector3 center; double radius = 0.0; };
    struct BoxPrim {
        Vector3 center;
        FQuat rot{};
        Vector3 half{}; // half-extents after /2 of authored full width
    };
    struct CapsulePrim {
        Vector3 p0, p1;
        double radius = 0.0;
    };
    struct ConvexPrim {
        std::vector<Vector3> verts;
        std::vector<int> indices; // empty -> triangle fan
    };

    struct SharedGeom {
        std::vector<SpherePrim> spheres;
        std::vector<BoxPrim> boxes;
        std::vector<CapsulePrim> capsules;
        std::vector<ConvexPrim> convex;
        bool boundsObb = false;
        Vector3 boundsOrigin{};
        Vector3 boundsExtent{};
    };

    struct Instance {
        uint64_t actor = 0;
        uint64_t geomKey = 0;
        FTransform c2w{};
        Vector3 aabbMin{};
        Vector3 aabbMax{};
    };

    struct Snapshot {
        std::unordered_map<uint64_t, SharedGeom> geoms;
        std::vector<Instance> instances;
        std::unordered_set<uint64_t> actorSet;
        bool ready = false;
        uint32_t bodies = 0;
        uint32_t spheres = 0;
        uint32_t boxes = 0;
        uint32_t sphyls = 0;
        uint32_t convex = 0;
        uint32_t bounds = 0;
        uint32_t primBounds = 0;
        uint32_t primFail = 0;
        uint32_t rebuildMs = 0;
        uint32_t scanned = 0;
        uint32_t noMesh = 0;
        uint32_t distSkip = 0;
        uint32_t noGeom = 0;
        uint32_t aabbReject = 0;
        uint32_t primHit = 0;
    };

    struct AggLayout {
        bool valid = false;
        int sphereOff = 0x18;
        int sphereRadius = 0x30;
        int sphereStride = 0x38;
        int boxOff = 0x18;
        int boxRot = 0x30;
        int boxExtent = 0x48;
        int boxStride = 0x58;
        int sphylOff = 0x18;
        int sphylRot = 0x30;
        int sphylRadius = 0x48;
        int sphylLength = 0x4C;
        int sphylStride = 0x50;
        int convexVerts = 0x18;
        int convexStride = 0xC0;
    };

    inline Snapshot g_buf[2]{};
    inline std::atomic<int> g_pub{ 0 };
    inline std::atomic<bool> g_ready{ false };
    inline std::shared_mutex g_snapMtx;
    inline std::atomic<bool> g_started{ false };
    inline std::atomic<uint64_t> g_lastUWorld{ 0 };
    inline AggLayout g_layout{};
    inline bool g_layoutLogged = false;
    inline bool g_physxLogged = false;
    inline bool g_boundsLogged = false;
    inline int g_geomFailLogs = 0;
    inline int g_emptyWraps = 0;
    inline uintptr_t g_loggedAggOff = 0;
    inline uintptr_t g_extBoundsOff = 0;
    inline uintptr_t g_smaCompOff = 0;
    inline int g_smaProbeTries = 0;
    inline Snapshot g_accum{};
    inline std::mutex g_accumMtx;
    inline int g_scanLevel = 0;
    inline int g_scanActor = 0;
    inline uint64_t g_scanWorld = 0;
    inline Vector3 g_radiusOrigin{};
    inline uint64_t g_skipPawn = 0;
    constexpr DWORD kBudgetMs = 80;

    inline FQuat QuatConjugate(const FQuat& q)
    {
        FQuat o;
        o.x = -q.x; o.y = -q.y; o.z = -q.z; o.w = q.w;
        return o;
    }

    inline Vector3 CameraForward(const FRotator& rot)
    {
        constexpr double kDeg = 3.14159265358979323846 / 180.0;
        const double pitchRad = rot.Pitch * kDeg;
        const double yawRad = rot.Yaw * kDeg;
        const double cp = cos(pitchRad), sp = sin(pitchRad);
        const double cy = cos(yawRad), sy = sin(yawRad);
        return Vector3(cp * cy, cp * sy, sp);
    }

    inline Vector3 MulScale(const Vector3& v, const Vector3& s)
    {
        return Vector3(v.x * s.x, v.y * s.y, v.z * s.z);
    }

    inline Vector3 TransformPoint(const FTransform& t, const Vector3& local)
    {
        Vector3 s = t.scale;
        if (s.x == 0.0 && s.y == 0.0 && s.z == 0.0)
            s = Vector3(1, 1, 1);
        const Vector3 scaled = MulScale(local, s);
        const Vector3 r = QuatRotate(scaled, t.rot);
        return Vector3(t.translation.x + r.x, t.translation.y + r.y, t.translation.z + r.z);
    }

    inline Vector3 InversePoint(const FTransform& t, const Vector3& world)
    {
        Vector3 s = t.scale;
        if (fabs(s.x) < 0.001) s.x = 1.0;
        if (fabs(s.y) < 0.001) s.y = 1.0;
        if (fabs(s.z) < 0.001) s.z = 1.0;
        Vector3 p(world.x - t.translation.x, world.y - t.translation.y, world.z - t.translation.z);
        p = QuatRotate(p, QuatConjugate(t.rot));
        return Vector3(p.x / s.x, p.y / s.y, p.z / s.z);
    }

    inline double MaxAbsScale(const FTransform& t)
    {
        const double ax = fabs(t.scale.x), ay = fabs(t.scale.y), az = fabs(t.scale.z);
        double m = ax;
        if (ay > m) m = ay;
        if (az > m) m = az;
        if (m < 0.001) m = 1.0;
        return m;
    }

    inline bool SaneLocal(const Vector3& v)
    {
        return std::isfinite(v.x) && std::isfinite(v.y) && std::isfinite(v.z)
            && fabs(v.x) < 200000.0 && fabs(v.y) < 200000.0 && fabs(v.z) < 200000.0;
    }

    inline bool SaneRadius(double r)
    {
        return std::isfinite(r) && r > 1.0 && r < 2500.0;
    }

    inline bool SaneExtent(double x, double y, double z)
    {
        if (!std::isfinite(x) || !std::isfinite(y) || !std::isfinite(z))
            return false;
        if (x < 2.0 || y < 2.0 || z < 2.0)
            return false;
        if (x > kInsaneExtentUu || y > kInsaneExtentUu || z > kInsaneExtentUu)
            return false;
        return true;
    }

    inline Vector3 ReadVec(const uint8_t* p)
    {
        Vector3 v;
        memcpy(&v, p, sizeof(Vector3));
        return v;
    }

    inline FRotator ReadRot(const uint8_t* p)
    {
        FRotator r;
        memcpy(&r, p, sizeof(FRotator));
        return r;
    }

    inline float ReadF32(const uint8_t* p)
    {
        float f;
        memcpy(&f, p, 4);
        return f;
    }

    inline bool ScoreSphereElem(const uint8_t* elem, int off, int radiusOff)
    {
        if (radiusOff + 4 > 240 || off + (int)sizeof(Vector3) > 240)
            return false;
        const Vector3 c = ReadVec(elem + off);
        const double r = (double)ReadF32(elem + radiusOff);
        return SaneLocal(c) && SaneRadius(r);
    }

    inline bool ScoreBoxElem(const uint8_t* elem, int off, int rotOff, int extOff)
    {
        if (extOff + 12 > 240)
            return false;
        const Vector3 c = ReadVec(elem + off);
        const FRotator rot = ReadRot(elem + rotOff);
        const float x = ReadF32(elem + extOff);
        const float y = ReadF32(elem + extOff + 4);
        const float z = ReadF32(elem + extOff + 8);
        if (!SaneLocal(c) || !SaneExtent(x, y, z))
            return false;
        return std::isfinite(rot.Pitch) && std::isfinite(rot.Yaw) && std::isfinite(rot.Roll)
            && fabs(rot.Pitch) < 720.0 && fabs(rot.Yaw) < 720.0 && fabs(rot.Roll) < 720.0;
    }

    inline bool ReadElemBlob(uintptr_t data, int index, int stride, uint8_t* out, int bytes)
    {
        if (!is_valid_pointer_optimized(data) || stride <= 0 || bytes <= 0 || bytes > 256)
            return false;
        return ctx->read_array(data + (uint64_t)index * (uint64_t)stride, out, (size_t)bytes);
    }

    inline void InferLayout(uintptr_t sphereData, int sphereCount, uintptr_t boxData, int boxCount)
    {
        if (g_layout.valid)
            return;

        AggLayout L;
        // UE5 LWC shipping: FKShapeElem ~0x18, FVector 24, FRotator 24.
        L.sphereOff = 0x18; L.sphereRadius = 0x30; L.sphereStride = 0x38;
        L.boxOff = 0x18; L.boxRot = 0x30; L.boxExtent = 0x48; L.boxStride = 0x58;
        L.sphylOff = 0x18; L.sphylRot = 0x30; L.sphylRadius = 0x48; L.sphylLength = 0x4C; L.sphylStride = 0x50;
        L.convexVerts = 0x18; L.convexStride = 0xC0;

        uint8_t e0[256]{}, e1[256]{};
        bool sphereOk = false;
        if (sphereCount > 0 && is_valid_pointer_optimized(sphereData)) {
            static const int kBase[] = { 0x18, 0x20, 0x10, 0x30, 0x28 };
            static const int kStride[] = { 0x38, 0x40, 0x30, 0x48, 0x50, 0x80 };
            for (int b : kBase) {
                for (int st : kStride) {
                    if (!ReadElemBlob(sphereData, 0, st, e0, 128))
                        continue;
                    const int rOff = b + (int)sizeof(Vector3);
                    if (!ScoreSphereElem(e0, b, rOff))
                        continue;
                    if (sphereCount >= 2) {
                        if (!ReadElemBlob(sphereData, 1, st, e1, 128) || !ScoreSphereElem(e1, b, rOff))
                            continue;
                    }
                    L.sphereOff = b;
                    L.sphereRadius = rOff;
                    L.sphereStride = st;
                    sphereOk = true;
                    break;
                }
                if (sphereOk)
                    break;
            }
        }

        bool boxOk = false;
        if (boxCount > 0 && is_valid_pointer_optimized(boxData)) {
            static const int kBase[] = { 0x18, 0x20, 0x10, 0x30 };
            static const int kStride[] = { 0x58, 0x60, 0x50, 0x70, 0x48, 0x80 };
            for (int b : kBase) {
                const int rotOff = b + (int)sizeof(Vector3);
                const int extOff = rotOff + (int)sizeof(FRotator);
                for (int st : kStride) {
                    if (st < extOff + 12)
                        continue;
                    if (!ReadElemBlob(boxData, 0, st, e0, 160))
                        continue;
                    if (!ScoreBoxElem(e0, b, rotOff, extOff))
                        continue;
                    if (boxCount >= 2) {
                        if (!ReadElemBlob(boxData, 1, st, e1, 160) || !ScoreBoxElem(e1, b, rotOff, extOff))
                            continue;
                    }
                    L.boxOff = b;
                    L.boxRot = rotOff;
                    L.boxExtent = extOff;
                    L.boxStride = st;
                    boxOk = true;
                    break;
                }
                if (boxOk)
                    break;
            }
        }

        L.sphylOff = L.boxOff;
        L.sphylRot = L.boxRot;
        L.sphylRadius = L.boxExtent;
        L.sphylLength = L.boxExtent + 4;
        L.sphylStride = (L.boxStride >= 0x50) ? 0x50 : L.boxStride;
        if (L.sphylStride < L.sphylLength + 4)
            L.sphylStride = L.sphylLength + 8;

        L.valid = sphereOk || boxOk;
        if (!L.valid) {
            // Keep UE5 LWC defaults so a one-element array can still parse.
            L.valid = true;
        }
        g_layout = L;
    }

    inline bool PointInAabb(const Vector3& p, const Vector3& bmin, const Vector3& bmax)
    {
        return p.x >= bmin.x && p.x <= bmax.x
            && p.y >= bmin.y && p.y <= bmax.y
            && p.z >= bmin.z && p.z <= bmax.z;
    }

    // Overlap: origin inside still counts (used to cull instances before prim tests).
    inline bool RayAabbOverlap(const Vector3& orig, const Vector3& dir, double tMax,
        const Vector3& bmin, const Vector3& bmax)
    {
        double tmin = -1e18;
        double tmax = tMax;
        const double o[3] = { orig.x, orig.y, orig.z };
        const double d[3] = { dir.x, dir.y, dir.z };
        const double mn[3] = { bmin.x, bmin.y, bmin.z };
        const double mx[3] = { bmax.x, bmax.y, bmax.z };
        for (int i = 0; i < 3; i++) {
            if (fabs(d[i]) < 1e-12) {
                if (o[i] < mn[i] || o[i] > mx[i])
                    return false;
                continue;
            }
            double inv = 1.0 / d[i];
            double t0 = (mn[i] - o[i]) * inv;
            double t1 = (mx[i] - o[i]) * inv;
            if (t0 > t1) { const double tmp = t0; t0 = t1; t1 = tmp; }
            if (t0 > tmin) tmin = t0;
            if (t1 < tmax) tmax = t1;
            if (tmax < tmin)
                return false;
        }
        return tmax >= 0.0 && tmin <= tMax;
    }

    // Surface entry only. Origin inside does not count - ExtendedBounds of the
    // building you are standing in would otherwise occlude every ray.
    inline bool RayAabbEnter(const Vector3& orig, const Vector3& dir, double tMin, double tMax,
        const Vector3& bmin, const Vector3& bmax)
    {
        double tmin = -1e18;
        double tmax = tMax;
        const double o[3] = { orig.x, orig.y, orig.z };
        const double d[3] = { dir.x, dir.y, dir.z };
        const double mn[3] = { bmin.x, bmin.y, bmin.z };
        const double mx[3] = { bmax.x, bmax.y, bmax.z };
        for (int i = 0; i < 3; i++) {
            if (fabs(d[i]) < 1e-12) {
                if (o[i] < mn[i] || o[i] > mx[i])
                    return false;
                continue;
            }
            double inv = 1.0 / d[i];
            double t0 = (mn[i] - o[i]) * inv;
            double t1 = (mx[i] - o[i]) * inv;
            if (t0 > t1) { const double tmp = t0; t0 = t1; t1 = tmp; }
            if (t0 > tmin) tmin = t0;
            if (t1 < tmax) tmax = t1;
            if (tmax < tmin)
                return false;
        }
        return tmin >= tMin && tmin <= tMax;
    }

    inline bool RayAabb(const Vector3& orig, const Vector3& dir, double tMax,
        const Vector3& bmin, const Vector3& bmax, double tMinWorld)
    {
        (void)tMinWorld;
        return RayAabbOverlap(orig, dir, tMax, bmin, bmax);
    }

    inline bool RaySphere(const Vector3& orig, const Vector3& dir, double tMin, double tMax,
        const Vector3& c, double r)
    {
        const Vector3 oc(orig.x - c.x, orig.y - c.y, orig.z - c.z);
        const double a = dir.Dot(dir);
        const double b = 2.0 * oc.Dot(dir);
        const double cc = oc.Dot(oc) - r * r;
        const double disc = b * b - 4.0 * a * cc;
        if (disc < 0.0 || a < 1e-12)
            return false;
        const double s = sqrt(disc);
        double t = (-b - s) / (2.0 * a);
        if (t >= tMin && t <= tMax)
            return true;
        t = (-b + s) / (2.0 * a);
        return t >= tMin && t <= tMax;
    }

    inline bool RayCapsule(const Vector3& orig, const Vector3& dir, double tMin, double tMax,
        const Vector3& a, const Vector3& b, double radius)
    {
        const Vector3 ba = b - a;
        const Vector3 oa = orig - a;
        const double baba = ba.Dot(ba);
        if (baba < 1e-8)
            return RaySphere(orig, dir, tMin, tMax, a, radius);
        const double bard = ba.Dot(dir);
        const double baoa = ba.Dot(oa);
        const double rdoa = dir.Dot(oa);
        const double oaoa = oa.Dot(oa);
        const double a2 = baba - bard * bard;
        const double b2 = baba * rdoa - baoa * bard;
        const double c2 = baba * oaoa - baoa * baoa - radius * radius * baba;
        const double h = b2 * b2 - a2 * c2;
        if (h >= 0.0 && fabs(a2) > 1e-12) {
            const double t = (-b2 - sqrt(h)) / a2;
            const double y = baoa + t * bard;
            if (y > 0.0 && y < baba && t >= tMin && t <= tMax)
                return true;
        }
        return RaySphere(orig, dir, tMin, tMax, a, radius)
            || RaySphere(orig, dir, tMin, tMax, b, radius);
    }

    inline bool RayAabbLocal(const Vector3& localO, const Vector3& localD, double tMin, double tMax,
        const Vector3& half)
    {
        const Vector3 bmin(-half.x, -half.y, -half.z);
        const Vector3 bmax(half.x, half.y, half.z);
        return RayAabbEnter(localO, localD, tMin, tMax, bmin, bmax);
    }

    inline bool RayTriangle(const Vector3& orig, const Vector3& dir, double tMin, double tMax,
        const Vector3& v0, const Vector3& v1, const Vector3& v2)
    {
        const Vector3 e1 = v1 - v0;
        const Vector3 e2 = v2 - v0;
        const Vector3 p(
            dir.y * e2.z - dir.z * e2.y,
            dir.z * e2.x - dir.x * e2.z,
            dir.x * e2.y - dir.y * e2.x);
        const double det = e1.Dot(p);
        if (fabs(det) < 1e-12)
            return false;
        const double inv = 1.0 / det;
        const Vector3 tvec = orig - v0;
        const double u = tvec.Dot(p) * inv;
        if (u < 0.0 || u > 1.0)
            return false;
        const Vector3 q(
            tvec.y * e1.z - tvec.z * e1.y,
            tvec.z * e1.x - tvec.x * e1.z,
            tvec.x * e1.y - tvec.y * e1.x);
        const double v = dir.Dot(q) * inv;
        if (v < 0.0 || u + v > 1.0)
            return false;
        const double t = e2.Dot(q) * inv;
        return t >= tMin && t <= tMax;
    }

    inline void ExpandAabb(Vector3& mn, Vector3& mx, const Vector3& p)
    {
        if (p.x < mn.x) mn.x = p.x;
        if (p.y < mn.y) mn.y = p.y;
        if (p.z < mn.z) mn.z = p.z;
        if (p.x > mx.x) mx.x = p.x;
        if (p.y > mx.y) mx.y = p.y;
        if (p.z > mx.z) mx.z = p.z;
    }

    inline bool BoundsToWorldAabb(const FTransform& c2w, const Vector3& origin, const Vector3& extent,
        Vector3& aabbMin, Vector3& aabbMax)
    {
        if (!std::isfinite(origin.x) || !std::isfinite(origin.y) || !std::isfinite(origin.z))
            return false;
        if (!std::isfinite(extent.x) || !std::isfinite(extent.y) || !std::isfinite(extent.z))
            return false;
        if (extent.x < 1.0 && extent.y < 1.0 && extent.z < 1.0)
            return false;
        if (extent.x > kInsaneExtentUu || extent.y > kInsaneExtentUu || extent.z > kInsaneExtentUu)
            return false;
        Vector3 s = c2w.scale;
        if (s.x == 0.0 && s.y == 0.0 && s.z == 0.0)
            s = Vector3(1, 1, 1);
        const double sx = fabs(extent.x * s.x);
        const double sy = fabs(extent.y * s.y);
        const double sz = fabs(extent.z * s.z);
        if (sx < kTinyExtentUu && sy < kTinyExtentUu && sz < kTinyExtentUu)
            return false;
        if (sx > 28000.0 || sy > 28000.0 || sz > 28000.0)
            return false;

        const Vector3 scaledO = MulScale(origin, s);
        const Vector3 rotO = QuatRotate(scaledO, c2w.rot);
        const Vector3 wC(c2w.translation.x + rotO.x, c2w.translation.y + rotO.y, c2w.translation.z + rotO.z);

        const Vector3 corners[8] = {
            Vector3(-sx, -sy, -sz), Vector3(sx, -sy, -sz),
            Vector3(sx, sy, -sz), Vector3(-sx, sy, -sz),
            Vector3(-sx, -sy, sz), Vector3(sx, -sy, sz),
            Vector3(sx, sy, sz), Vector3(-sx, sy, sz)
        };
        aabbMin = Vector3(1e18, 1e18, 1e18);
        aabbMax = Vector3(-1e18, -1e18, -1e18);
        for (int i = 0; i < 8; i++) {
            const Vector3 r = QuatRotate(corners[i], c2w.rot);
            ExpandAabb(aabbMin, aabbMax, Vector3(wC.x + r.x, wC.y + r.y, wC.z + r.z));
        }
        return true;
    }

    inline bool LooksLikeStaticMeshAsset(uintptr_t sm);
    inline bool LooksLikeMeshComp(uintptr_t comp);

    inline uintptr_t ProbeSmaMeshComp(uintptr_t actor)
    {
        if (g_smaCompOff) {
            const uintptr_t c = ctx->read<uintptr_t>(actor + g_smaCompOff);
            if (LooksLikeMeshComp(c))
                return c;
        }
        const uintptr_t known = Offsets::Get().UStaticMeshActor.StaticMeshComponent;
        const uintptr_t cKnown = ctx->read<uintptr_t>(actor + known);
        if (LooksLikeMeshComp(cKnown)) {
            g_smaCompOff = known;
            return cKnown;
        }
        if (g_smaCompOff)
            return 0;
        if (g_smaProbeTries >= 64)
            return 0;
        ++g_smaProbeTries;
        const uintptr_t begin = (known > 0x80) ? known - 0x80 : 0x300;
        const uintptr_t end = known + 0x80;
        for (uintptr_t off = begin; off <= end; off += 8) {
            if (off == known)
                continue;
            const uintptr_t c = ctx->read<uintptr_t>(actor + off);
            if (!LooksLikeMeshComp(c))
                continue;
            g_smaCompOff = off;
            if (debugConsoleEnabled) {
                char line[160];
                snprintf(line, sizeof(line),
                    "[RayVis] SMA StaticMeshComponent off=0x%llX (cfg=0x%llX)",
                    (unsigned long long)off, (unsigned long long)known);
                DebugSystem::LogConsole(line);
            }
            return c;
        }
        return 0;
    }

    inline uintptr_t FindStaticMeshComp(uintptr_t actor, uintptr_t root)
    {
        if (LooksLikeMeshComp(root))
            return root;
        if (const uintptr_t sma = ProbeSmaMeshComp(actor))
            return sma;
        if (!is_valid_pointer_optimized(root))
            return 0;
        const auto& off = Offsets::Get();
        TArray childArr{};
        if (!ctx->read_array(root + off.USceneComponent.AttachChildren, &childArr, sizeof(TArray)))
            return 0;
        if (!is_valid_pointer_optimized(childArr.Data) || childArr.Count <= 0)
            return 0;
        int cnt = childArr.Count;
        if (cnt > 48)
            cnt = 48;
        uint64_t kids[48]{};
        if (!ctx->read_array(childArr.Data, kids, (size_t)cnt * sizeof(uint64_t)))
            return 0;
        for (int i = 0; i < cnt; i++) {
            if (LooksLikeMeshComp(kids[i]))
                return kids[i];
        }
        // One extra hop: Actor -> Scene -> StaticMeshComponent.
        int grandCap = cnt;
        if (grandCap > 16)
            grandCap = 16;
        for (int i = 0; i < grandCap; i++) {
            if (!is_valid_pointer_optimized(kids[i]))
                continue;
            TArray grand{};
            if (!ctx->read_array(kids[i] + off.USceneComponent.AttachChildren, &grand, sizeof(TArray)))
                continue;
            if (!is_valid_pointer_optimized(grand.Data) || grand.Count <= 0)
                continue;
            int gn = grand.Count;
            if (gn > 8)
                gn = 8;
            uint64_t gkids[8]{};
            if (!ctx->read_array(grand.Data, gkids, (size_t)gn * sizeof(uint64_t)))
                continue;
            for (int k = 0; k < gn; k++) {
                if (LooksLikeMeshComp(gkids[k]))
                    return gkids[k];
            }
        }
        return 0;
    }

    inline bool ParseSphereArray(uintptr_t data, int count, SharedGeom& g)
    {
        const AggLayout& L = g_layout;
        if (count > kMaxElems)
            count = kMaxElems;
        uint8_t blob[128];
        for (int i = 0; i < count; i++) {
            if (!ReadElemBlob(data, i, L.sphereStride, blob, 80))
                break;
            const Vector3 c = ReadVec(blob + L.sphereOff);
            const double r = (double)ReadF32(blob + L.sphereRadius);
            if (!SaneLocal(c) || !SaneRadius(r))
                continue;
            g.spheres.push_back({ c, r });
        }
        return !g.spheres.empty();
    }

    inline bool ParseBoxArray(uintptr_t data, int count, SharedGeom& g)
    {
        const AggLayout& L = g_layout;
        if (count > kMaxElems)
            count = kMaxElems;
        uint8_t blob[160];
        for (int i = 0; i < count; i++) {
            if (!ReadElemBlob(data, i, L.boxStride, blob, 160))
                break;
            const Vector3 c = ReadVec(blob + L.boxOff);
            const FRotator rot = ReadRot(blob + L.boxRot);
            const double x = (double)ReadF32(blob + L.boxExtent);
            const double y = (double)ReadF32(blob + L.boxExtent + 4);
            const double z = (double)ReadF32(blob + L.boxExtent + 8);
            if (!SaneLocal(c) || !SaneExtent(x, y, z))
                continue;
            BoxPrim b;
            b.center = c;
            b.rot = RotatorToQuat(rot);
            // Authored X/Y/Z are full width (zarboz 2x trap).
            b.half = Vector3(x * 0.5, y * 0.5, z * 0.5);
            g.boxes.push_back(b);
        }
        return !g.boxes.empty();
    }

    inline bool ParseSphylArray(uintptr_t data, int count, SharedGeom& g)
    {
        const AggLayout& L = g_layout;
        if (count > kMaxElems)
            count = kMaxElems;
        uint8_t blob[160];
        for (int i = 0; i < count; i++) {
            if (!ReadElemBlob(data, i, L.sphylStride, blob, 160))
                break;
            const Vector3 c = ReadVec(blob + L.sphylOff);
            const FRotator rot = ReadRot(blob + L.sphylRot);
            const double radius = (double)ReadF32(blob + L.sphylRadius);
            const double length = (double)ReadF32(blob + L.sphylLength);
            if (!SaneLocal(c) || !SaneRadius(radius) || !std::isfinite(length) || length < 0.0 || length > kInsaneExtentUu)
                continue;
            const FQuat q = RotatorToQuat(rot);
            const Vector3 axis = QuatRotate(Vector3(0, 0, 1), q);
            const Vector3 half = axis * (length * 0.5);
            CapsulePrim cap;
            cap.p0 = Vector3(c.x - half.x, c.y - half.y, c.z - half.z);
            cap.p1 = Vector3(c.x + half.x, c.y + half.y, c.z + half.z);
            cap.radius = radius;
            g.capsules.push_back(cap);
        }
        return !g.capsules.empty();
    }

    inline bool ParseConvexArray(uintptr_t data, int count, SharedGeom& g)
    {
        const AggLayout& L = g_layout;
        if (count > 16)
            count = 16;
        for (int i = 0; i < count; i++) {
            TArray verts{};
            const uintptr_t elem = data + (uint64_t)i * (uint64_t)L.convexStride;
            if (!ctx->read_array(elem + (uintptr_t)L.convexVerts, &verts, sizeof(TArray)))
                continue;
            if (!is_valid_tarray(verts, kMaxConvexVerts) && !(is_valid_pointer_optimized(verts.Data)
                && verts.Count >= 4 && verts.Count <= kMaxConvexVerts))
                continue;
            int n = verts.Count;
            if (n > kMaxConvexVerts)
                n = kMaxConvexVerts;
            std::vector<Vector3> pts((size_t)n);
            if (!ctx->read_array(verts.Data, pts.data(), (size_t)n * sizeof(Vector3)))
                continue;
            bool ok = true;
            for (int v = 0; v < n; v++) {
                if (!SaneLocal(pts[v])) { ok = false; break; }
            }
            if (!ok)
                continue;
            ConvexPrim cv;
            cv.verts = std::move(pts);
            g.convex.push_back(std::move(cv));
        }
        return !g.convex.empty();
    }

    inline void LogFirstAggGeom(uintptr_t body, uintptr_t agg, const TArray& sp, const TArray& bx,
        const TArray& sy, const TArray& cv)
    {
        if (g_layoutLogged || !debugConsoleEnabled)
            return;
        g_layoutLogged = true;
        char line[384];
        snprintf(line, sizeof(line),
            "[RayVis] AggGeom body=0x%llX off=0x%llX FVector=%zu LWC=1 "
            "sphere=%d/%d box=%d/%d sphyl=%d/%d convex=%d/%d "
            "layout sphStride=0x%X boxStride=0x%X sphOff=0x%X boxExt=0x%X",
            (unsigned long long)body, (unsigned long long)agg, sizeof(Vector3),
            sp.Count, sp.Max, bx.Count, bx.Max, sy.Count, sy.Max, cv.Count, cv.Max,
            g_layout.sphereStride, g_layout.boxStride, g_layout.sphereOff, g_layout.boxExtent);
        DebugSystem::LogConsole(line);
        if (sp.Count > 0 && is_valid_pointer_optimized(sp.Data)) {
            uint8_t blob[80]{};
            if (ReadElemBlob(sp.Data, 0, g_layout.sphereStride, blob, 80)) {
                const Vector3 c = ReadVec(blob + g_layout.sphereOff);
                const float r = ReadF32(blob + g_layout.sphereRadius);
                snprintf(line, sizeof(line),
                    "[RayVis] first sphere center=(%.1f,%.1f,%.1f) radius=%.1f",
                    c.x, c.y, c.z, r);
                DebugSystem::LogConsole(line);
            }
        }
        if (bx.Count > 0 && is_valid_pointer_optimized(bx.Data)) {
            uint8_t blob[160]{};
            if (ReadElemBlob(bx.Data, 0, g_layout.boxStride, blob, 160)) {
                const Vector3 c = ReadVec(blob + g_layout.boxOff);
                const float x = ReadF32(blob + g_layout.boxExtent);
                const float y = ReadF32(blob + g_layout.boxExtent + 4);
                const float z = ReadF32(blob + g_layout.boxExtent + 8);
                snprintf(line, sizeof(line),
                    "[RayVis] first box center=(%.1f,%.1f,%.1f) fullWidth=(%.1f,%.1f,%.1f)",
                    c.x, c.y, c.z, x, y, z);
                DebugSystem::LogConsole(line);
            }
        }
    }

    inline void LogPhysXFollowup(uintptr_t uworld)
    {
        if (g_physxLogged || !debugConsoleEnabled || !is_valid_pointer_optimized(uworld))
            return;
        g_physxLogged = true;
        // Landscape heightfields and complex TriMeshes live in FPhysScene / NpScene.
        // No public Arc Raiders FPhysScene offset for this CL - do not guess stock
        // PhysX 3.4 (NpScene 0x23D8). AggGeom vischeck ships without that layer.
        char line[256];
        snprintf(line, sizeof(line),
            "[RayVis] PhysX unused uworld=0x%llX - no FPhysScene offset this CL; "
            "landscape/TriMesh not in AggGeom",
            (unsigned long long)uworld);
        DebugSystem::LogConsole(line);
    }

    inline bool AggHeaderSane(const TArray hdr[4])
    {
        for (int i = 0; i < 4; i++) {
            if (hdr[i].Count < 0 || hdr[i].Count > 256)
                return false;
            if (hdr[i].Count == 0)
                continue;
            if (!is_valid_pointer_optimized(hdr[i].Data))
                return false;
            if (hdr[i].Max != 0 && (hdr[i].Max < hdr[i].Count || hdr[i].Max > 4096))
                return false;
        }
        return true;
    }

    inline bool ReadAggHeader(uintptr_t body, uintptr_t aggOff, TArray hdr[4])
    {
        if (!is_valid_pointer_optimized(body))
            return false;
        if (!ctx->read_array(body + aggOff, hdr, sizeof(TArray) * 4))
            return false;
        return AggHeaderSane(hdr);
    }

    inline FTransform IdentityC2W()
    {
        FTransform t{};
        t.rot.w = 1.0;
        t.scale = Vector3(1, 1, 1);
        return t;
    }

    inline bool SaneWorldExtent(const Vector3& e)
    {
        if (!std::isfinite(e.x) || !std::isfinite(e.y) || !std::isfinite(e.z))
            return false;
        const double ax = fabs(e.x), ay = fabs(e.y), az = fabs(e.z);
        const double mx = fmax(ax, fmax(ay, az));
        const double mn = fmin(ax, fmin(ay, az));
        if (mx < 8.0 || mx > 25000.0)
            return false;
        // Walls are thin on one axis. mn<1 dropped every sheet mesh.
        if (mn < 0.25)
            return false;
        if (mn > 12000.0)
            return false;
        if (ax * ay * az > 2e10)
            return false;
        return true;
    }

    inline bool ReadExtendedBounds(uintptr_t sm, Vector3& origin, Vector3& extent)
    {
        const uintptr_t bOff = Offsets::Get().UStaticMesh.ExtendedBounds;
        Vector3 o{}, e{};
        if (!ctx->read_array(sm + bOff, &o, sizeof(Vector3)))
            return false;
        if (!ctx->read_array(sm + bOff + sizeof(Vector3), &e, sizeof(Vector3)))
            return false;
        if (!SaneWorldExtent(e))
            return false;
        origin = o;
        extent = Vector3(fabs(e.x), fabs(e.y), fabs(e.z));
        if (!g_extBoundsOff) {
            g_extBoundsOff = bOff;
            if (debugConsoleEnabled) {
                char line[192];
                snprintf(line, sizeof(line),
                    "[RayVis] ExtendedBounds off=0x%llX e=(%.1f,%.1f,%.1f)",
                    (unsigned long long)bOff, extent.x, extent.y, extent.z);
                DebugSystem::LogConsole(line);
            }
        }
        return true;
    }

    inline bool LooksLikeStaticMeshAsset(uintptr_t sm)
    {
        if (!is_valid_pointer_optimized(sm) || sm < 0x10000000ULL)
            return false;
        Vector3 o{}, e{};
        return ReadExtendedBounds(sm, o, e);
    }

    inline bool LooksLikeMeshComp(uintptr_t comp)
    {
        if (!is_valid_pointer_optimized(comp) || comp < 0x10000000ULL)
            return false;
        const uintptr_t sm = ctx->read<uintptr_t>(
            comp + Offsets::Get().UStaticMeshComponent.StaticMesh);
        if (!is_valid_pointer_optimized(sm) || sm < 0x10000000ULL)
            return false;
        const uintptr_t body = ctx->read<uintptr_t>(sm + Offsets::Get().UStaticMesh.BodySetup);
        if (is_valid_pointer_optimized(body))
            return true;
        return LooksLikeStaticMeshAsset(sm);
    }

    inline bool ReadComponentBounds(uintptr_t comp, const FTransform& c2w,
        Vector3& origin, Vector3& extent, bool& worldSpace)
    {
        const auto& off = Offsets::Get();
        const float boundsScale = ctx->read<float>(comp + off.USkeletalMeshComponent.BoundsScale);
        if (!std::isfinite(boundsScale) || boundsScale < 0.01f || boundsScale > 100.f)
            return false;
        // Only PrimitiveComponent Bounds (immediately before BoundsScale).
        // CTW± offsets overlap the transform and produced pancake AABBs
        // (live probe e=(8.9,88.7,0.3) at 0x338) that flickered vischeck.
        const uintptr_t cands[] = {
            off.USkeletalMeshComponent.BoundsScale - 0x38,
            off.USkeletalMeshComponent.BoundsScale - 0x40
        };
        if (debugConsoleEnabled && !g_boundsLogged) {
            g_boundsLogged = true;
            char line[384];
            snprintf(line, sizeof(line),
                "[RayVis] boundsProbe comp=0x%llX c2w=(%.0f,%.0f,%.0f) scaleOff=0x%llX",
                (unsigned long long)comp,
                c2w.translation.x, c2w.translation.y, c2w.translation.z,
                (unsigned long long)off.USkeletalMeshComponent.BoundsScale);
            DebugSystem::LogConsole(line);
            for (uintptr_t bOff : cands) {
                Vector3 o{}, e{};
                ctx->read_array(comp + bOff, &o, sizeof(Vector3));
                ctx->read_array(comp + bOff + sizeof(Vector3), &e, sizeof(Vector3));
                snprintf(line, sizeof(line),
                    "[RayVis] boundsCand off=0x%llX o=(%.1f,%.1f,%.1f) e=(%.1f,%.1f,%.1f) distC2w=%.0f saneE=%d",
                    (unsigned long long)bOff,
                    o.x, o.y, o.z, e.x, e.y, e.z,
                    o.distance(c2w.translation), SaneWorldExtent(e) ? 1 : 0);
                DebugSystem::LogConsole(line);
            }
        }
        for (uintptr_t bOff : cands) {
            Vector3 o{}, e{};
            if (!ctx->read_array(comp + bOff, &o, sizeof(Vector3)))
                continue;
            if (!ctx->read_array(comp + bOff + sizeof(Vector3), &e, sizeof(Vector3)))
                continue;
            if (!SaneWorldExtent(e))
                continue;
            if (!std::isfinite(o.x) || !std::isfinite(o.y) || !std::isfinite(o.z))
                continue;
            const double reach = sqrt(e.x * e.x + e.y * e.y + e.z * e.z) + 800.0;
            const double dist = o.distance(c2w.translation);
            if (dist <= reach) {
                origin = o;
                extent = Vector3(fabs(e.x), fabs(e.y), fabs(e.z));
                worldSpace = true;
                return true;
            }
            if (fabs(o.x) < 50000.0 && fabs(o.y) < 50000.0 && fabs(o.z) < 50000.0) {
                origin = o;
                extent = Vector3(fabs(e.x), fabs(e.y), fabs(e.z));
                worldSpace = false;
                return true;
            }
        }
        return false;
    }

    inline int AggHeaderScore(const TArray hdr[4])
    {
        int n = 0;
        for (int i = 0; i < 4; i++) {
            if (hdr[i].Count > 0)
                n += hdr[i].Count;
        }
        return n;
    }

    inline bool BuildGeom(uintptr_t sm, uintptr_t body, SharedGeom& g)
    {
        Vector3 origin{}, extent{};
        if (!ReadExtendedBounds(sm, origin, extent)) {
            if (debugConsoleEnabled && g_geomFailLogs < 8) {
                g_geomFailLogs++;
                char line[256];
                snprintf(line, sizeof(line),
                    "[RayVis] geomFail sm=0x%llX body=0x%llX",
                    (unsigned long long)sm, (unsigned long long)body);
                DebugSystem::LogConsole(line);
            }
            return false;
        }

        if (is_valid_pointer_optimized(body)) {
            const uintptr_t aggOff = Offsets::Get().UBodySetup.AggGeom;
            TArray hdr[4]{};
            if (ReadAggHeader(body, aggOff, hdr)) {
                InferLayout(hdr[0].Data, hdr[0].Count, hdr[1].Data, hdr[1].Count);
                LogFirstAggGeom(body, body + aggOff, hdr[0], hdr[1], hdr[2], hdr[3]);
                if (hdr[0].Count > 0)
                    ParseSphereArray(hdr[0].Data, hdr[0].Count, g);
                if (hdr[1].Count > 0)
                    ParseBoxArray(hdr[1].Data, hdr[1].Count, g);
                if (hdr[2].Count > 0)
                    ParseSphylArray(hdr[2].Data, hdr[2].Count, g);
                if (hdr[3].Count > 0)
                    ParseConvexArray(hdr[3].Data, hdr[3].Count, g);
            }
        }

        g.boundsObb = true;
        g.boundsOrigin = origin;
        g.boundsExtent = extent;
        return true;
    }

    inline bool HitGeom(const SharedGeom& g, const FTransform& c2w,
        const Vector3& orig, const Vector3& dir, double tMin, double tMax)
    {
        const double scale = MaxAbsScale(c2w);
        for (const auto& s : g.spheres) {
            const Vector3 wc = TransformPoint(c2w, s.center);
            const double r = s.radius * scale;
            if (orig.distance(wc) <= r + 8.0)
                continue;
            if (RaySphere(orig, dir, tMin, tMax, wc, r))
                return true;
        }
        for (const auto& cap : g.capsules) {
            const Vector3 wa = TransformPoint(c2w, cap.p0);
            const Vector3 wb = TransformPoint(c2w, cap.p1);
            if (RayCapsule(orig, dir, tMin, tMax, wa, wb, cap.radius * scale))
                return true;
        }
        for (const auto& b : g.boxes) {
            const Vector3 localO = InversePoint(c2w, orig);
            const Vector3 localH = InversePoint(c2w, orig + dir * tMax);
            Vector3 p = localO - b.center;
            p = QuatRotate(p, QuatConjugate(b.rot));
            Vector3 q = localH - b.center;
            q = QuatRotate(q, QuatConjugate(b.rot));
            const Vector3 localD = q - p;
            const double worldLen = tMax;
            const double localLen = localD.Length();
            if (localLen < 1e-8)
                continue;
            const double tScale = worldLen / localLen;
            const Vector3 dNorm = localD / localLen;
            if (RayAabbLocal(p, dNorm, tMin / tScale, worldLen / tScale, b.half))
                return true;
        }
        if (g.boundsObb) {
            Vector3 localO = InversePoint(c2w, orig);
            const Vector3 rel(
                localO.x - g.boundsOrigin.x,
                localO.y - g.boundsOrigin.y,
                localO.z - g.boundsOrigin.z);
            // Fat mesh AABB. Standing inside it (any POI) must not occlude the whole raid.
            if (fabs(rel.x) <= g.boundsExtent.x + 8.0
                && fabs(rel.y) <= g.boundsExtent.y + 8.0
                && fabs(rel.z) <= g.boundsExtent.z + 8.0)
            {
                ;
            } else {
                Vector3 localH = InversePoint(c2w, orig + dir * tMax);
                localO = rel;
                localH = Vector3(localH.x - g.boundsOrigin.x, localH.y - g.boundsOrigin.y, localH.z - g.boundsOrigin.z);
                const Vector3 localD = localH - localO;
                const double localLen = localD.Length();
                if (localLen > 1e-8) {
                    const double tScale = tMax / localLen;
                    if (RayAabbLocal(localO, localD / localLen, tMin / tScale, tMax / tScale, g.boundsExtent))
                        return true;
                }
            }
        }
        for (const auto& cv : g.convex) {
            const int n = (int)cv.verts.size();
            if (n < 3)
                continue;
            if (!cv.indices.empty() && cv.indices.size() >= 3) {
                for (size_t i = 0; i + 2 < cv.indices.size(); i += 3) {
                    const int i0 = cv.indices[i], i1 = cv.indices[i + 1], i2 = cv.indices[i + 2];
                    if (i0 < 0 || i1 < 0 || i2 < 0 || i0 >= n || i1 >= n || i2 >= n)
                        continue;
                    const Vector3 w0 = TransformPoint(c2w, cv.verts[i0]);
                    const Vector3 w1 = TransformPoint(c2w, cv.verts[i1]);
                    const Vector3 w2 = TransformPoint(c2w, cv.verts[i2]);
                    if (RayTriangle(orig, dir, tMin, tMax, w0, w1, w2))
                        return true;
                }
            } else {
                const Vector3 w0 = TransformPoint(c2w, cv.verts[0]);
                for (int i = 1; i + 1 < n; i++) {
                    const Vector3 w1 = TransformPoint(c2w, cv.verts[i]);
                    const Vector3 w2 = TransformPoint(c2w, cv.verts[i + 1]);
                    if (RayTriangle(orig, dir, tMin, tMax, w0, w1, w2))
                        return true;
                }
            }
        }
        return false;
    }

    inline bool CommitInstance(Snapshot& snap, uint64_t actor, uint64_t geomKey,
        const FTransform& c2w, SharedGeom* geom, const Vector3& bOrigin, const Vector3& bExtent)
    {
        (void)geom;
        Vector3 aabbMin, aabbMax;
        if (!BoundsToWorldAabb(c2w, bOrigin, bExtent, aabbMin, aabbMax))
            return false;
        const double dx = aabbMax.x - aabbMin.x;
        const double dy = aabbMax.y - aabbMin.y;
        const double dz = aabbMax.z - aabbMin.z;
        if (dx > 72000.0 || dy > 72000.0 || dz > 72000.0)
            return false;
        const double mn = fmin(dx, fmin(dy, dz));
        const double mx = fmax(dx, fmax(dy, dz));
        if (mn < 20.0 && mx > 16000.0)
            return false;
        Instance inst;
        inst.actor = actor;
        inst.geomKey = geomKey;
        inst.c2w = c2w;
        inst.aabbMin = aabbMin;
        inst.aabbMax = aabbMax;
        snap.instances.push_back(inst);
        snap.actorSet.insert(actor);
        return true;
    }

    inline bool TryAddPrimBounds(Snapshot& snap, uint64_t actor, uintptr_t comp, const Vector3& localPos)
    {
        if (!is_valid_pointer_optimized(comp))
            return false;
        if ((int)snap.instances.size() >= kMaxInstances)
            return false;
        const auto& off = Offsets::Get();
        FTransform c2w = ctx->read<FTransform>(comp + off.USceneComponent.ComponentToWorld);
        if (!std::isfinite(c2w.translation.x) || !std::isfinite(c2w.translation.y) || !std::isfinite(c2w.translation.z))
            return false;

        Vector3 origin{}, extent{};
        bool worldSpace = false;
        if (!ReadComponentBounds(comp, c2w, origin, extent, worldSpace)) {
            snap.primFail++;
            return false;
        }

        if (extent.x < 50.0 && extent.y < 50.0 && extent.z < 50.0)
            return false;

        const Vector3 worldCenter = worldSpace ? origin : TransformPoint(c2w, origin);
        if (!localPos.IsZero() && worldCenter.distance(localPos) > kCacheRadiusUu)
            return false;

        const uint64_t geomKey = comp;
        auto it = snap.geoms.find(geomKey);
        SharedGeom* geom = nullptr;
        if (it == snap.geoms.end()) {
            SharedGeom built{};
            built.boundsObb = true;
            built.boundsOrigin = origin;
            built.boundsExtent = extent;
            snap.bodies++;
            snap.bounds++;
            snap.primBounds++;
            auto ins = snap.geoms.emplace(geomKey, std::move(built));
            geom = &ins.first->second;
        } else {
            geom = &it->second;
        }
        const FTransform instC2w = worldSpace ? IdentityC2W() : c2w;
        return CommitInstance(snap, actor, geomKey, instC2w, geom, origin, extent);
    }

    inline bool TryAddActor(Snapshot& snap, uint64_t actor, const Vector3& localPos, uint64_t skipPawn)
    {
        if (!is_valid_pointer_optimized(actor) || actor == skipPawn)
            return false;
        if (snap.actorSet.find(actor) != snap.actorSet.end())
            return false;
        if ((int)snap.instances.size() >= kMaxInstances)
            return false;

        snap.scanned++;
        const auto& off = Offsets::Get();
        const uintptr_t root = ctx->read<uintptr_t>(actor + off.UObject.RootComponent);
        const uintptr_t meshComp = FindStaticMeshComp(actor, root);
        uintptr_t sm = 0;
        if (is_valid_pointer_optimized(meshComp))
            sm = ctx->read<uintptr_t>(meshComp + off.UStaticMeshComponent.StaticMesh);

        if (is_valid_pointer_optimized(meshComp) && LooksLikeStaticMeshAsset(sm)) {
            FTransform c2w = ctx->read<FTransform>(meshComp + off.USceneComponent.ComponentToWorld);
            if (std::isfinite(c2w.translation.x) && std::isfinite(c2w.translation.y) && std::isfinite(c2w.translation.z)
                && (localPos.IsZero() || c2w.translation.distance(localPos) <= kCacheRadiusUu))
            {
                const uintptr_t body = ctx->read<uintptr_t>(sm + off.UStaticMesh.BodySetup);
                const uint64_t geomKey = is_valid_pointer_optimized(body) ? body : sm;

                SharedGeom* geom = nullptr;
                auto it = snap.geoms.find(geomKey);
                if (it == snap.geoms.end()) {
                    SharedGeom built{};
                    if (BuildGeom(sm, body, built)) {
                        snap.bodies++;
                        snap.spheres += (uint32_t)built.spheres.size();
                        snap.boxes += (uint32_t)built.boxes.size();
                        snap.sphyls += (uint32_t)built.capsules.size();
                        snap.convex += (uint32_t)built.convex.size();
                        if (built.boundsObb)
                            snap.bounds++;
                        auto ins = snap.geoms.emplace(geomKey, std::move(built));
                        geom = &ins.first->second;
                    }
                } else {
                    geom = &it->second;
                }

                if (geom) {
                    Vector3 bOrigin = geom->boundsOrigin;
                    Vector3 bExtent = geom->boundsExtent;
                    if (!geom->boundsObb) {
                        if (!ReadExtendedBounds(sm, bOrigin, bExtent)) {
                            bOrigin = Vector3{};
                            bExtent = Vector3(50, 50, 50);
                        }
                    }
                    if (CommitInstance(snap, actor, geomKey, c2w, geom, bOrigin, bExtent))
                        return true;
                    snap.aabbReject++;
                }
            } else if (std::isfinite(c2w.translation.x) && !localPos.IsZero()
                && c2w.translation.distance(localPos) > kCacheRadiusUu)
            {
                snap.distSkip++;
                return false;
            }
        }

        if (is_valid_pointer_optimized(meshComp) && TryAddPrimBounds(snap, actor, meshComp, localPos)) {
            snap.primHit++;
            return true;
        }
        snap.noMesh++;
        return false;
    }

    inline void PublishAccum()
    {
        if ((int)g_accum.instances.size() < kMinReadyInstances)
            return;
        const int write = 1 - g_pub.load(std::memory_order_relaxed);
        Snapshot& dest = g_buf[write];
        dest = g_accum;
        dest.ready = true;
        {
            std::unique_lock<std::shared_mutex> lk(g_snapMtx);
            g_pub.store(write, std::memory_order_release);
            g_ready.store(true, std::memory_order_release);
        }
    }

    inline void Rebuild()
    {
        SPOOF_FUNC;
        if (!ctx || ctx->cr3 == 0 || !NT::Store::Entity)
            return;

        if (NT::Store::Entity->GetRaidState() != NT::Store::RaidState::InRaid) {
            {
                std::unique_lock<std::shared_mutex> lk(g_snapMtx);
                const int pub = g_pub.load(std::memory_order_relaxed);
                g_buf[pub].ready = false;
                g_ready.store(false, std::memory_order_release);
            }
            std::lock_guard<std::mutex> lk(g_accumMtx);
            g_accum = Snapshot{};
            g_scanWorld = 0;
            g_scanLevel = 0;
            g_scanActor = 0;
            g_physxLogged = false;
            g_layoutLogged = false;
            g_boundsLogged = false;
            g_geomFailLogs = 0;
            g_emptyWraps = 0;
            g_loggedAggOff = 0;
            g_extBoundsOff = 0;
            g_smaCompOff = 0;
            g_smaProbeTries = 0;
            return;
        }

        PointerCache ptrs = NT::Store::Entity->GetPointerSnapshot();
        const uintptr_t uworld = ptrs.UWorld;
        if (!is_valid_pointer_optimized(uworld))
            return;

        CachedCameraData cam = NT::Store::Entity->GetCameraSnapshot();
        Vector3 localPos = cam.valid ? cam.location : ptrs.RelativeLocation;
        if (localPos.IsZero() && is_valid_pointer_optimized(ptrs.LocalPawn)) {
            const uintptr_t localRoot = ctx->read<uintptr_t>(ptrs.LocalPawn + Offsets::Get().UObject.RootComponent);
            if (is_valid_pointer_optimized(localRoot))
                localPos = GetSceneComponentWorldLocation(localRoot);
        }
        g_radiusOrigin = localPos;
        g_skipPawn = ptrs.LocalPawn;

        if (g_scanWorld != uworld) {
            g_scanWorld = uworld;
            g_scanLevel = 0;
            g_scanActor = 0;
            g_layout.valid = false;
            g_layoutLogged = false;
            g_physxLogged = false;
            g_boundsLogged = false;
            g_geomFailLogs = 0;
            g_emptyWraps = 0;
            g_loggedAggOff = 0;
            g_extBoundsOff = 0;
            g_smaCompOff = 0;
            g_smaProbeTries = 0;
            {
                std::lock_guard<std::mutex> lk(g_accumMtx);
                g_accum = Snapshot{};
            }
            {
                std::unique_lock<std::shared_mutex> lk(g_snapMtx);
                const int pub = g_pub.load(std::memory_order_relaxed);
                g_buf[pub].ready = false;
                g_ready.store(false, std::memory_order_release);
            }
        }

        const auto& off = Offsets::Get();
        TArray levelsArr{};
        if (!ctx->read_array(uworld + off.UWorld.Levels, &levelsArr, sizeof(TArray)))
            return;
        if (!is_valid_pointer_optimized(levelsArr.Data) || levelsArr.Count <= 0 || levelsArr.Count > 1024)
            return;
        int levelCount = levelsArr.Count;
        if (levelCount > 1024)
            levelCount = 1024;
        std::vector<uintptr_t> levelPtrs((size_t)levelCount);
        if (!ctx->read_array(levelsArr.Data, levelPtrs.data(), (size_t)levelCount * sizeof(uintptr_t)))
            return;

        const uintptr_t persistent = ctx->read<uintptr_t>(uworld + off.UWorld.PersistentLevel);
        if (is_valid_pointer_optimized(persistent)) {
            bool havePersist = false;
            for (int i = 0; i < levelCount; i++) {
                if (levelPtrs[i] == persistent) { havePersist = true; break; }
            }
            if (!havePersist) {
                levelPtrs.insert(levelPtrs.begin(), persistent);
                levelCount++;
            } else if (levelPtrs[0] != persistent) {
                for (int i = 1; i < levelCount; i++) {
                    if (levelPtrs[i] == persistent) {
                        std::swap(levelPtrs[0], levelPtrs[i]);
                        break;
                    }
                }
            }
        }

        const ULONGLONG t0 = GetTickCount64();
        const ULONGLONG deadline = t0 + kBudgetMs;
        constexpr int kBatch = 512;
        std::vector<uint64_t> actorBatch(kBatch);

        std::lock_guard<std::mutex> accumLk(g_accumMtx);
        Snapshot& snap = g_accum;

        bool wrapped = false;
        while (g_scanLevel < levelCount) {
            if (GetTickCount64() >= deadline)
                break;
            if ((int)snap.instances.size() >= kMaxInstances) {
                wrapped = true;
                break;
            }
            if (NT::Store::Entity->GetRaidState() != NT::Store::RaidState::InRaid)
                break;

            const uintptr_t level = levelPtrs[g_scanLevel];
            if (!is_valid_pointer_optimized(level)) {
                g_scanLevel++;
                g_scanActor = 0;
                continue;
            }
            TArray actorArr{};
            if (!ctx->read_array(level + off.ULevel.ActorArray, &actorArr, sizeof(TArray))) {
                g_scanLevel++;
                g_scanActor = 0;
                continue;
            }
            if (!is_valid_pointer_optimized(actorArr.Data) || actorArr.Count <= 0) {
                g_scanLevel++;
                g_scanActor = 0;
                continue;
            }
            int cap = actorArr.Count;
            if (cap > 20000)
                cap = 20000;
            if (g_scanActor >= cap) {
                g_scanLevel++;
                g_scanActor = 0;
                continue;
            }

            while (g_scanActor < cap) {
                if (GetTickCount64() >= deadline)
                    break;
                if ((int)snap.instances.size() >= kMaxInstances)
                    break;
                int n = kBatch;
                if (g_scanActor + n > cap)
                    n = cap - g_scanActor;
                if (!ctx->read_array(actorArr.Data + (size_t)g_scanActor * sizeof(uint64_t),
                    actorBatch.data(), (size_t)n * sizeof(uint64_t))) {
                    g_scanActor += n;
                    continue;
                }
                for (int j = 0; j < n; j++)
                    TryAddActor(snap, actorBatch[j], localPos, ptrs.LocalPawn);
                g_scanActor += n;
            }

            if (g_scanActor >= cap) {
                g_scanLevel++;
                g_scanActor = 0;
                if (g_scanLevel >= levelCount)
                    wrapped = true;
            } else {
                break;
            }
        }

        if (g_scanLevel >= levelCount)
            wrapped = true;

        snap.rebuildMs = (uint32_t)(GetTickCount64() - t0);

        const bool enough = (int)snap.instances.size() >= kMinReadyInstances;
        // Never arm mid-scan on a handful of AABBs. A full wrap (or the instance cap)
        // is the first point the snapshot is a real occluder set.
        if (enough && (wrapped || (int)snap.instances.size() >= kMaxInstances))
            PublishAccum();

        LogPhysXFollowup(uworld);

        int pubN = 0;
        {
            const int pub = g_pub.load(std::memory_order_relaxed);
            if (g_ready.load(std::memory_order_relaxed) && g_buf[pub].ready)
                pubN = (int)g_buf[pub].instances.size();
        }

        if (debugConsoleEnabled) {
            static uint64_t s_lastLog = 0;
            const uint64_t now = GetTickCount64();
            if (now - s_lastLog > 4000) {
                s_lastLog = now;
                char line[448];
                snprintf(line, sizeof(line),
                    "[RayVis] ready=%d pub=%d accum=%zu bodies=%u bounds=%u sph=%u box=%u "
                    "scanned=%u noMesh=%u distSkip=%u noGeom=%u aabbReject=%u primHit=%u "
                    "lvl=%d/%d ms=%u wrap=%d enough=%d",
                    g_ready.load(std::memory_order_relaxed) ? 1 : 0, pubN, snap.instances.size(),
                    snap.bodies, snap.bounds, snap.spheres, snap.boxes,
                    snap.scanned, snap.noMesh, snap.distSkip, snap.noGeom,
                    snap.aabbReject, snap.primHit,
                    g_scanLevel, levelCount, snap.rebuildMs, wrapped ? 1 : 0, enough ? 1 : 0);
                DebugSystem::LogConsole(line);
            }
        }

        if (wrapped) {
            g_scanLevel = 0;
            g_scanActor = 0;
            if (enough) {
                g_accum = Snapshot{};
                g_emptyWraps = 0;
            } else if (snap.instances.empty() && ++g_emptyWraps >= 2) {
                g_accum = Snapshot{};
                g_emptyWraps = 0;
            }
        }
    }

    inline void IngestActor(uint64_t actor)
    {
        if (!ctx || ctx->cr3 == 0)
            return;
        std::unique_lock<std::mutex> lk(g_accumMtx, std::try_to_lock);
        if (!lk.owns_lock())
            return;
        TryAddActor(g_accum, actor, g_radiusOrigin, g_skipPawn);
    }

    inline bool Ready()
    {
        return g_ready.load(std::memory_order_acquire);
    }

    inline int PublishedCount()
    {
        if (!g_ready.load(std::memory_order_acquire))
            return 0;
        std::shared_lock<std::shared_mutex> lk(g_snapMtx);
        const int pub = g_pub.load(std::memory_order_acquire);
        const Snapshot& snap = g_buf[pub];
        if (!snap.ready)
            return 0;
        return (int)snap.instances.size();
    }

    inline bool Visible(const Vector3& cam, const FRotator& rot, const Vector3& head, uint64_t skipActor)
    {
        SPOOF_FUNC;
        g_radiusOrigin = cam;
        if (!Ready())
            return true;
        if (!std::isfinite(cam.x) || !std::isfinite(head.x))
            return true;

        std::shared_lock<std::shared_mutex> lk(g_snapMtx);
        const int pub = g_pub.load(std::memory_order_acquire);
        const Snapshot& snap = g_buf[pub];
        if (!snap.ready || (int)snap.instances.size() < kMinReadyInstances)
            return true;

        const Vector3 fwd = CameraForward(rot);
        Vector3 orig(
            cam.x + fwd.x * kOriginBiasUu,
            cam.y + fwd.y * kOriginBiasUu,
            cam.z + fwd.z * kOriginBiasUu);
        Vector3 delta(head.x - orig.x, head.y - orig.y, head.z - orig.z);
        const double dist = delta.Length();
        if (dist <= kSelfHitUu + kSkinUu)
            return true;
        const Vector3 dir = delta / dist;
        const double tMin = kSelfHitUu;
        const double tMax = dist - kSkinUu;

        for (const auto& inst : snap.instances) {
            if (inst.actor == skipActor)
                continue;
            if (!RayAabbOverlap(orig, dir, tMax, inst.aabbMin, inst.aabbMax))
                continue;
            auto git = snap.geoms.find(inst.geomKey);
            if (git == snap.geoms.end())
                continue;
            if (HitGeom(git->second, inst.c2w, orig, dir, tMin, tMax))
                return false;
        }
        return true;
    }

    inline Vector3 HeadFromPawn(const PlayerDynamicData& pd)
    {
        auto it = pd.cachedBones.find(Head);
        if (it != pd.cachedBones.end() && is_valid_bone_position(it->second) && !it->second.IsZero())
            return it->second;
        Vector3 h = pd.worldLoc;
        h.z += pd.isDBNO ? 40.0 : 90.0;
        return h;
    }

    inline bool PawnVisible(const Vector3& cam, const FRotator& rot, const PlayerDynamicData& pd)
    {
        return Visible(cam, rot, HeadFromPawn(pd), pd.entity);
    }

    inline bool PawnVisibleHead(const Vector3& cam, const FRotator& rot, const PlayerDynamicData& pd, const Vector3& head)
    {
        Vector3 h = head;
        if (h.IsZero() || !is_valid_bone_position(h))
            h = HeadFromPawn(pd);
        return Visible(cam, rot, h, pd.entity);
    }

    inline void Clear()
    {
        std::unique_lock<std::shared_mutex> lk(g_snapMtx);
        g_ready.store(false, std::memory_order_release);
        g_buf[0].ready = false;
        g_buf[1].ready = false;
    }

    inline void Start()
    {
        bool expected = false;
        if (!g_started.compare_exchange_strong(expected, true))
            return;
        CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
            for (;;) {
                Rebuild();
                Sleep(kRebuildSleepMs);
            }
            return 0;
        }, nullptr, 0, nullptr);
    }

}
