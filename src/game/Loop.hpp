#include "WorldCache.hpp"
#include "EspReadyDiag.hpp"
#include "World.hpp"
#include "Cache.hpp"
#include "draws/Draws.h"
#include "draws/Textures.h"
#include "PlatformBadges.hpp"
#include "settings/Settings.hpp"
#include "menu/string_encryption.hpp"
#include "menu/framework/input/input.hpp"
#include "sdk/Reflection.hpp"
#include "app/RenderDevice.hpp"
#include "gui/gui.h"
#include <unordered_set>
#include <algorithm>
#include <cstdint>
#include <chrono>
#include <random>
#include <cmath>
#include <cstring>
#include <cctype>
#include <iostream>
#include "dgb/Spoof.h"

static inline bool LocalWorldToScreen(
    const Vector3& world_pos,
    const Vector3& cameraLocation,
    const FRotator& cameraRotation,
    float cameraFOV,
    float aspectRatio,
    uint32_t screen_width,
    uint32_t screen_height,
    Vector2* out_screen)
{
    SPOOF_FUNC;
    return WorldToScreen(world_pos, cameraLocation, cameraRotation, cameraFOV, aspectRatio, screen_width, screen_height, out_screen);
}

// Horizontal half-extent of the ESP bounding box around an entity's Root, in world
// units — standard UE character capsule radius.
static constexpr double kEspCapsuleRadius = 34.0;

// Range past which a failed skeleton read is attributed to LOD collapse rather than to
// bad data, and a box-derived marker stands in for the pose. Taken from the logs rather
// than picked: the two players who only ever failed coherence at long range were never
// seen closer than 392m, while toestellweather - whose mesh is genuinely broken - never
// exceeded 261m. The gap has margin on both sides.
static constexpr float kDistantMarkerMeters = 350.0f;

static const std::vector<std::pair<Bones, Bones>> skeletonPairs =
{
	{ pelvis, spine_02 },
	{ spine_02, neck_02 },
	{ neck_02, Head },

	{ neck_02, upperarm_l },
	{ upperarm_l, lowerarm_l },
	{ lowerarm_l, hand_l },

	{ neck_02, upperarm_r },
	{ upperarm_r, lowerarm_r },
	{ lowerarm_r, hand_r },

	{ pelvis, thigh_l },
	{ thigh_l, calf_l },
	{ calf_l, foot_l },

	{ pelvis, thigh_r },
	{ thigh_r, calf_r },
	{ calf_r, foot_r },
};

// Ordered bone chains for the skeleton ESP, drawn as consecutive *available* bones
// rather than fixed pairs. skeletonPairs above collapses the torso to pelvis->spine_02->
// neck_02, which cannot show lean or twist, and stops at the ankle so foot roll is
// invisible. Walking the chain and bridging whatever does not resolve gets the finer
// pose on meshes that have it while degrading to exactly the old silhouette on meshes
// that do not - a missing spine_01 just means pelvis connects straight to spine_02.
static const std::vector<std::vector<Bones>> skeletonChains =
{
	{ pelvis, spine_01, spine_02, spine_03, neck_01, neck_02, Head },

	{ neck_02, upperarm_l, lowerarm_l, hand_l },
	{ neck_02, upperarm_r, lowerarm_r, hand_r },

	{ pelvis, thigh_l, calf_l, foot_l, ball_l },
	{ pelvis, thigh_r, calf_r, foot_r, ball_r },
};

// Visibility comes from LastRenderTimeOnScreen vs world clock (and LRT−LRTS gap).
// Raw samples still chatter near the threshold; debounce with asymmetric holds:
// drop to occluded fast (aim / color), require a longer hold to go visible again.
inline bool StableVisible(uint64_t entity, bool rawSample)
{
	SPOOF_FUNC;
	struct VisFilter {
		bool shown = false;
		bool pending = false;
		uint64_t pendingSinceMs = 0;
		bool init = false;
	};
	static std::unordered_map<uint64_t, VisFilter> s_visFilter;
	if (s_visFilter.size() > 256)
		s_visFilter.clear();

	constexpr uint64_t kVisToHiddenMs = 40;
	constexpr uint64_t kVisToShownMs = 140;
	auto& vf = s_visFilter[entity];
	const uint64_t nowMs = GetTickCount64();

	if (!vf.init) {
		vf.init = true;
		vf.shown = rawSample;
		vf.pending = rawSample;
		vf.pendingSinceMs = nowMs;
		return vf.shown;
	}
	if (rawSample == vf.shown || rawSample != vf.pending) {
		vf.pending = rawSample;
		vf.pendingSinceMs = nowMs;
		return vf.shown;
	}
	const uint64_t hold = rawSample ? kVisToShownMs : kVisToHiddenMs;
	if ((nowMs - vf.pendingSinceMs) >= hold)
		vf.shown = rawSample;
	return vf.shown;
}

// The bone pass refreshes cachedBones an order of magnitude slower than the overlay
// draws, so projecting the cached positions directly gives a skeleton that snaps between
// poses instead of moving with the player. Approach the cached target exponentially in
// world space - framerate independent, unlike a fixed per-frame alpha - and snap outright
// on anything longer than a stride so a corner turn, a fall or a teleport never drags a
// limb behind the body.
//
// Idempotent within a frame: neck_02 and pelvis are each shared by three chains, and
// stepping the filter once per reference would converge them faster than the bones that
// appear only once, skewing the pose.
inline Vector3 SmoothBoneForRender(uint64_t entity, Bones bone, const Vector3& target, float dt)
{
	SPOOF_FUNC;
	struct BoneSmooth {
		Vector3 pos{};
		int lastFrame = -1;
		bool init = false;
	};
	static std::unordered_map<uint64_t, BoneSmooth> s_boneSmooth;
	if (s_boneSmooth.size() > 4096)
		s_boneSmooth.clear();

	const uint64_t key = entity ^ ((static_cast<uint64_t>(bone) & 0xFFull) << 56);
	auto& bs = s_boneSmooth[key];
	const int frame = ImGui::GetFrameCount();

	if (!bs.init) {
		bs.init = true;
		bs.pos = target;
		bs.lastFrame = frame;
		return bs.pos;
	}
	if (bs.lastFrame == frame)
		return bs.pos;
	bs.lastFrame = frame;

	constexpr double kSnapDistance = 60.0; // cm - longer than one animation step
	if (bs.pos.distance(target) > kSnapDistance) {
		bs.pos = target;
		return bs.pos;
	}

	// 25ms time constant: enough to turn a 100ms bone step into continuous motion,
	// short enough that the lag it adds stays under a frame at 40fps.
	constexpr float kTauSeconds = 0.025f;
	const float step = (dt > 0.0f && dt < 0.5f) ? dt : 0.016f;
	float a = 1.0f - expf(-step / kTauSeconds);
	if (a < 0.0f) a = 0.0f;
	if (a > 1.0f) a = 1.0f;

	bs.pos.x += (target.x - bs.pos.x) * a;
	bs.pos.y += (target.y - bs.pos.y) * a;
	bs.pos.z += (target.z - bs.pos.z) * a;
	return bs.pos;
}

inline Vector3 GetCachedBone(const PlayerDynamicData& pd, Bones bone)
{
    SPOOF_FUNC;
    auto it = pd.cachedBones.find(bone);
    if (it != pd.cachedBones.end())
        return it->second;
    return Vector3{};
}

// Keybind results for the current frame.
//
// Toggle-mode binds need exactly one edge test per frame. Consumers here sit in
// per-target loops and inside conditional draw blocks, so they read this cache
// instead of polling the key themselves - the combat-mode toggle in particular
// used to live inside an `if (textFont)` branch and silently stopped responding
// whenever that block was skipped.
struct FrameBindStates
{
    bool aim = false;
    bool arcAim = false;
    bool weakspot = false;
    bool combat = false;
    bool zoom = false;
};

inline FrameBindStates g_FrameBinds{};

inline void UpdateFrameBinds()
{
    static Input::BindState s_aim, s_arcAim, s_weakspot, s_combat, s_zoom;

    g_FrameBinds.aim = Input::EvalBind(AimCfg.aimKey, AimCfg.aimKeyMode, s_aim);
    g_FrameBinds.arcAim = Input::EvalBind(ArcAimCfg.activationKey, ArcAimCfg.activationKeyMode, s_arcAim);
    g_FrameBinds.weakspot = Input::EvalBind(ArcAimCfg.weakspotKey, ArcAimCfg.weakspotKeyMode, s_weakspot);
    g_FrameBinds.combat = Input::EvalBind(combatModeKey, combatModeKeyMode, s_combat);
    g_FrameBinds.zoom = Input::EvalBind(zoomChangerKey, zoomChangerKeyMode, s_zoom);
}

template <typename T>
inline bool WriteWithStatus(uint64_t address, T value)
{
    SPOOF_FUNC;
    auto status = ctx->write_status(address, value);
    if (status != TransportStatus::Ok) {
        if (debugConsoleEnabled) {
            char buf[128];
            snprintf(buf, sizeof(buf), "[Write] skipped at 0x%llX: status=%d",
                     (unsigned long long)address, static_cast<int>(status));
            DebugSystem::LogConsole(buf);
        }
        return false;
    }
    return true;
}

namespace BoneVis
{
    static void CapsuleFilled(ImDrawList* dl, ImVec2 a, ImVec2 b, float halfW, ImU32 col)
    {
        SPOOF_FUNC;
        ImVec2 d(b.x - a.x, b.y - a.y);
        float len = sqrtf(d.x * d.x + d.y * d.y);
        if (len < 0.5f) return;
        ImVec2 perp(-d.y / len * halfW, d.x / len * halfW);
        float ang = atan2f(d.y, d.x);
        dl->PathLineTo(ImVec2(a.x + perp.x, a.y + perp.y));
        dl->PathLineTo(ImVec2(b.x + perp.x, b.y + perp.y));
        dl->PathArcTo(b, halfW, ang - 1.5707963f, ang + 1.5707963f, 8);
        dl->PathLineTo(ImVec2(b.x - perp.x, b.y - perp.y));
        dl->PathLineTo(ImVec2(a.x - perp.x, a.y - perp.y));
        dl->PathArcTo(a, halfW, ang + 1.5707963f, ang + 4.7123890f, 8);
        dl->PathFillConvex(col);
    }

    static void CapsuleOutline(ImDrawList* dl, ImVec2 a, ImVec2 b, float halfW, ImU32 col, float thickness)
    {
        SPOOF_FUNC;
        ImVec2 d(b.x - a.x, b.y - a.y);
        float len = sqrtf(d.x * d.x + d.y * d.y);
        if (len < 0.5f) return;
        ImVec2 perp(-d.y / len * halfW, d.x / len * halfW);
        float ang = atan2f(d.y, d.x);
        dl->PathLineTo(ImVec2(a.x + perp.x, a.y + perp.y));
        dl->PathLineTo(ImVec2(b.x + perp.x, b.y + perp.y));
        dl->PathArcTo(b, halfW, ang - 1.5707963f, ang + 1.5707963f, 8);
        dl->PathLineTo(ImVec2(b.x - perp.x, b.y - perp.y));
        dl->PathLineTo(ImVec2(a.x - perp.x, a.y - perp.y));
        dl->PathArcTo(a, halfW, ang + 1.5707963f, ang + 4.7123890f, 8);
        dl->PathStroke(col, ImDrawFlags_Closed, thickness);
    }

    enum LimbType { LIMB_SPINE, LIMB_SHOULDER, LIMB_UPPER_ARM, LIMB_LOWER_ARM, LIMB_HAND, LIMB_UPPER_LEG, LIMB_LOWER_LEG, LIMB_FOOT, LIMB_OTHER };

    static LimbType ClassifyLimb(Bones from, Bones to)
    {
        SPOOF_FUNC;
        if ((from == pelvis && to == spine_02) || (from == spine_02 && to == neck_02))
            return LIMB_SPINE;
        if ((from == neck_02 && to == upperarm_l) || (from == neck_02 && to == upperarm_r))
            return LIMB_SHOULDER;
        if ((from == upperarm_l && to == lowerarm_l) || (from == upperarm_r && to == lowerarm_r))
            return LIMB_UPPER_ARM;
        if ((from == lowerarm_l && to == hand_l) || (from == lowerarm_r && to == hand_r))
            return LIMB_LOWER_ARM;
        if ((from == pelvis && to == thigh_l) || (from == pelvis && to == thigh_r))
            return LIMB_UPPER_LEG;
        if ((from == thigh_l && to == calf_l) || (from == thigh_r && to == calf_r))
            return LIMB_LOWER_LEG;
        if ((from == calf_l && to == foot_l) || (from == calf_r && to == foot_r))
            return LIMB_FOOT;
        return LIMB_OTHER;
    }

    static float LimbScale(LimbType t)
    {
        SPOOF_FUNC;
        switch (t) {
        case LIMB_SPINE:     return 1.4f;
        case LIMB_SHOULDER:  return 0.8f;
        case LIMB_UPPER_ARM: return 0.55f;
        case LIMB_LOWER_ARM: return 0.45f;
        case LIMB_HAND:      return 0.35f;
        case LIMB_UPPER_LEG: return 0.8f;
        case LIMB_LOWER_LEG: return 0.6f;
        case LIMB_FOOT:      return 0.4f;
        default:             return 0.5f;
        }
    }

    static inline int Clamp255(int v) { return v < 0 ? 0 : (v > 255 ? 255 : v); }
    static inline float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
    static inline int LerpI(int a, int b, float t) { return a + (int)((float)(b - a) * t); }

    // boneShift carries the cached pose forward onto the live capsule position, the same
    // correction the ESP box applies. Without it the chams would sit metres behind a
    // running player while the box tracked them.
    static void Draw(
        ImDrawList* dl,
        const PlayerDynamicData& pd,
        const ViewContext& vc,
        float height,
        float distance,
        const float* chamsCol,
        const float* glowCol,
        const Vector3& boneShift = Vector3{})
    {
        SPOOF_FUNC;
        if (!pd.bones_valid || pd.cachedBones.empty())
            return;

        float distScale = Clampf(height / 300.0f, 0.08f, 2.5f);
        float baseWidth = chamsGlowWidth * distScale;
        int   layers    = chamsGlowLayers;
        int   baseR     = Clamp255((int)(chamsCol[0] * 255.0f));
        int   baseG     = Clamp255((int)(chamsCol[1] * 255.0f));
        int   baseB     = Clamp255((int)(chamsCol[2] * 255.0f));
        int   baseA     = Clamp255((int)(chamsCol[3] * 255.0f));
        int   glowR     = Clamp255((int)(glowCol[0] * 255.0f));
        int   glowG     = Clamp255((int)(glowCol[1] * 255.0f));
        int   glowB     = Clamp255((int)(glowCol[2] * 255.0f));
        int   glowA     = Clamp255((int)(glowCol[3] * 255.0f));

        struct BoneScreenPt { ImVec2 pos; bool valid; };
        std::unordered_map<Bones, BoneScreenPt> projected;

        auto projectBone = [&](Bones bone) -> BoneScreenPt& {
            auto it = projected.find(bone);
            if (it != projected.end()) return it->second;
            BoneScreenPt& pt = projected[bone];
            pt.valid = false;
            Vector3 w = GetCachedBone(pd, bone);
            if (w.IsZero()) return pt;
            w.x += boneShift.x;
            w.y += boneShift.y;
            Vector2 s{};
            if (vc.Project(w, &s)) {
                pt.pos   = ImVec2((float)s.x, (float)s.y);
                pt.valid = true;
            }
            return pt;
        };

        auto getLimbW = [&](Bones from, Bones to) -> float {
            return baseWidth * LimbScale(ClassifyLimb(from, to));
        };

        auto forEachLimb = [&](auto fn) {
            for (auto& bp : skeletonPairs) {
                auto& a = projectBone(bp.first);
                auto& b = projectBone(bp.second);
                if (!a.valid || !b.valid) continue;
                fn(a.pos, b.pos, getLimbW(bp.first, bp.second));
            }
        };

        auto& headPt = projectBone(Head);
        float headR  = baseWidth * 0.7f;
        if (headR < 2.0f) headR = 2.0f;

        if (chamsMode == 0)
        {
            float glowSpread = baseWidth * 0.6f;
            for (int layer = layers; layer >= 0; layer--)
            {
                float t = (float)layer / (float)layers;
                int   alpha;
                float extraW;
                int   r = baseR, g = baseG, b = baseB;
                if (layer == 0) {
                    alpha  = Clamp255((int)(baseA * 0.55f));
                    extraW = 0.0f;
                } else {
                    // Outer layers migrate toward the rim colour so the halo reads
                    // as a separate light source instead of a flat blur.
                    const float fadeA = (float)baseA + (float)(glowA - baseA) * t;
                    alpha  = Clamp255((int)(fadeA * 0.25f * (1.0f - t * t * 0.6f)));
                    extraW = glowSpread * t;
                    r = LerpI(baseR, glowR, t);
                    g = LerpI(baseG, glowG, t);
                    b = LerpI(baseB, glowB, t);
                }
                ImU32 gc = IM_COL32(r, g, b, alpha);
                forEachLimb([&](ImVec2 a, ImVec2 b, float w) {
                    CapsuleFilled(dl, a, b, (w + extraW) * 0.5f, gc);
                });
                if (headPt.valid)
                    dl->AddCircleFilled(headPt.pos, headR + extraW * 0.5f, gc, 20);
            }
        }
        else if (chamsMode == 1)
        {
            int   fillA  = Clamp255((int)(255.0f * chamsFillOpacity));
            ImU32 fill   = IM_COL32(baseR, baseG, baseB, fillA);
            ImU32 dark   = IM_COL32(0, 0, 0, Clamp255((int)(fillA * 0.7f)));
            ImU32 bright = IM_COL32(glowR, glowG, glowB, Clamp255((int)(glowA * 0.9f)));
            float et     = Clampf(1.5f * distScale, 1.0f, 8.0f);

            forEachLimb([&](ImVec2 a, ImVec2 b, float w) {
                CapsuleFilled(dl, a, b, w * 0.5f, fill);
            });
            forEachLimb([&](ImVec2 a, ImVec2 b, float w) {
                CapsuleOutline(dl, a, b, w * 0.5f, dark,   et + 1.0f);
                CapsuleOutline(dl, a, b, w * 0.5f, bright, et);
            });
            if (headPt.valid) {
                float hr = Clampf(headR, 3.0f, 40.0f);
                dl->AddCircleFilled(headPt.pos, hr, fill, 20);
                dl->AddCircle(headPt.pos, hr, dark, 20, et + 1.0f);
                dl->AddCircle(headPt.pos, hr, bright, 20, et);
            }
        }
        else if (chamsMode == 2)
        {
            ImU32 wireCol    = IM_COL32(glowR, glowG, glowB, Clamp255((int)(glowA * 0.95f)));
            ImU32 wireColDim = IM_COL32(baseR, baseG, baseB, Clamp255((int)(baseA * 0.35f)));
            ImU32 fillDim    = IM_COL32(baseR, baseG, baseB, Clamp255((int)(baseA * 0.06f)));
            float lineThick  = Clampf(1.2f * distScale, 1.0f, 4.0f);
            float crossThick = Clampf(0.8f * distScale, 0.5f, 2.0f);

            forEachLimb([&](ImVec2 a, ImVec2 b, float w) {
                CapsuleFilled(dl, a, b, w * 0.5f, fillDim);
            });

            forEachLimb([&](ImVec2 a, ImVec2 b, float w) {
                CapsuleOutline(dl, a, b, w * 0.5f, wireCol, lineThick);
            });

            forEachLimb([&](ImVec2 a, ImVec2 b, float w) {
                float halfW = w * 0.5f;
                ImVec2 d(b.x - a.x, b.y - a.y);
                float len = sqrtf(d.x * d.x + d.y * d.y);
                if (len < 4.0f) return;
                ImVec2 perp(-d.y / len * halfW, d.x / len * halfW);

                int segments = Clamp255((int)(len / (12.0f / distScale)));
                if (segments < 2) segments = 2;
                if (segments > 12) segments = 12;

                for (int i = 1; i < segments; i++)
                {
                    float t = (float)i / (float)segments;
                    ImVec2 mid(a.x + d.x * t, a.y + d.y * t);
                    float taper = 1.0f - 2.0f * fabsf(t - 0.5f);
                    taper = 0.4f + taper * 0.6f;
                    ImVec2 p1(mid.x + perp.x * taper, mid.y + perp.y * taper);
                    ImVec2 p2(mid.x - perp.x * taper, mid.y - perp.y * taper);
                    dl->AddLine(p1, p2, wireColDim, crossThick);
                }

                dl->AddLine(a, b, wireColDim, crossThick);
            });

            if (headPt.valid) {
                float hr = Clampf(headR, 2.0f, 40.0f);
                dl->AddCircleFilled(headPt.pos, hr, fillDim, 16);
                dl->AddCircle(headPt.pos, hr, wireCol, 16, lineThick);
                int crossCount = Clamp255((int)(hr / 3.0f));
                if (crossCount < 2) crossCount = 2;
                if (crossCount > 6) crossCount = 6;
                for (int i = 1; i < crossCount; i++) {
                    float off = hr * (2.0f * i / crossCount - 1.0f);
                    float halfChord = sqrtf(hr * hr - off * off);
                    dl->AddLine(
                        ImVec2(headPt.pos.x + off, headPt.pos.y - halfChord),
                        ImVec2(headPt.pos.x + off, headPt.pos.y + halfChord),
                        wireColDim, crossThick);
                }
            }
        }
        else if (chamsMode == 3)
        {
            int   fillA = Clamp255((int)(255.0f * chamsFillOpacity * 0.4f));
            ImU32 fill  = IM_COL32(baseR, baseG, baseB, fillA);

            forEachLimb([&](ImVec2 a, ImVec2 b, float w) {
                CapsuleFilled(dl, a, b, w * 0.5f, fill);
            });

            struct LayerDef { int r, g, b, a; float thick; };
            LayerDef defs[3] = {
                { 0,     0,     0,     Clamp255((int)(baseA * 0.5f)),  Clampf(4.0f * distScale, 1.0f, 16.0f) },
                { baseR, baseG, baseB, Clamp255((int)(baseA * 0.6f)),  Clampf(2.5f * distScale, 1.0f, 12.0f) },
                { glowR, glowG, glowB, Clamp255((int)(glowA * 0.95f)), Clampf(1.2f * distScale, 1.0f, 6.0f)  },
            };
            for (int i = 0; i < 3; i++)
            {
                ImU32 lc = IM_COL32(defs[i].r, defs[i].g, defs[i].b, defs[i].a);
                forEachLimb([&](ImVec2 a, ImVec2 b, float w) {
                    CapsuleOutline(dl, a, b, w * 0.5f, lc, defs[i].thick);
                });
                if (headPt.valid) {
                    float hr = Clampf(headR, 2.5f, 40.0f);
                    if (i == 0) dl->AddCircleFilled(headPt.pos, hr, fill, 20);
                    dl->AddCircle(headPt.pos, hr, lc, 20, defs[i].thick);
                }
            }
        }
        else if (chamsMode == 4)
        {
            // Pseudo-3D. A limb is a flat capsule, so roundness has to be faked:
            // nested shells of shrinking width and rising brightness, each nudged
            // toward a fixed light vector. The off-axis bright core is what sells
            // it as a tube. Joint discs add contact shadow, rim stroke adds edge.
            const ImVec2 lightDir(-0.55f, -0.83f);
            const int    fillA = Clamp255((int)(255.0f * chamsFillOpacity));

            struct Shell { float widthMul; float shade; float alphaMul; float offMul; };
            static const Shell shells[3] = {
                { 1.00f, 0.40f, 1.00f, 0.00f },
                { 0.70f, 0.85f, 0.90f, 0.18f },
                { 0.36f, 1.35f, 0.80f, 0.34f },
            };

            for (int s = 0; s < 3; ++s)
            {
                const Shell& sd = shells[s];
                const ImU32 c = IM_COL32(
                    Clamp255((int)(baseR * sd.shade)),
                    Clamp255((int)(baseG * sd.shade)),
                    Clamp255((int)(baseB * sd.shade)),
                    Clamp255((int)(fillA * sd.alphaMul)));
                const float off = baseWidth * sd.offMul;
                forEachLimb([&](ImVec2 a, ImVec2 b, float w) {
                    ImVec2 sa(a.x + lightDir.x * off, a.y + lightDir.y * off);
                    ImVec2 sb(b.x + lightDir.x * off, b.y + lightDir.y * off);
                    CapsuleFilled(dl, sa, sb, w * 0.5f * sd.widthMul, c);
                });
            }

            const ImU32 aoCol = IM_COL32(0, 0, 0, Clamp255((int)(fillA * 0.40f)));
            for (auto& bp : skeletonPairs) {
                auto& jp = projectBone(bp.first);
                if (!jp.valid) continue;
                const float jr = baseWidth * 0.5f * LimbScale(ClassifyLimb(bp.first, bp.second)) * 0.85f;
                if (jr < 1.0f) continue;
                dl->AddCircleFilled(jp.pos, jr, aoCol, 12);
            }

            const float rimT   = Clampf(1.6f * distScale, 1.0f, 5.0f);
            const float rimOff = baseWidth * 0.15f;
            const ImU32 rim    = IM_COL32(glowR, glowG, glowB, Clamp255((int)(glowA * 0.95f)));
            forEachLimb([&](ImVec2 a, ImVec2 b, float w) {
                ImVec2 ra(a.x + lightDir.x * rimOff, a.y + lightDir.y * rimOff);
                ImVec2 rb(b.x + lightDir.x * rimOff, b.y + lightDir.y * rimOff);
                CapsuleOutline(dl, ra, rb, w * 0.5f, rim, rimT);
            });

            if (headPt.valid) {
                const float hr = Clampf(headR, 3.0f, 40.0f);
                for (int s = 0; s < 3; ++s) {
                    const Shell& sd = shells[s];
                    const ImU32 c = IM_COL32(
                        Clamp255((int)(baseR * sd.shade)),
                        Clamp255((int)(baseG * sd.shade)),
                        Clamp255((int)(baseB * sd.shade)),
                        Clamp255((int)(fillA * sd.alphaMul)));
                    const float off = hr * sd.offMul;
                    dl->AddCircleFilled(
                        ImVec2(headPt.pos.x + lightDir.x * off, headPt.pos.y + lightDir.y * off),
                        hr * sd.widthMul, c, 24);
                }
                dl->AddCircle(
                    ImVec2(headPt.pos.x + lightDir.x * hr * 0.15f,
                           headPt.pos.y + lightDir.y * hr * 0.15f),
                    hr, rim, 24, rimT);
            }
        }
    }
}


void ExecuteAssistARC(const Vector3& cameraLocation, const FRotator& cameraRotation, float cameraFOV, float aspectRatio, ImDrawList* dl, uintptr_t playerController, float projectileSpeed, float maxProjectileSpeed, float projectileGravity, float worldGravity, const std::vector<PlayerDynamicData>& snapshot)
{
    SPOOF_FUNC;
    if (!ArcAimCfg.enable || !playerController)
        return;

    const bool activationKeyPressed = g_FrameBinds.arcAim;

    if (!activationKeyPressed) {
        if (g_LockedArcEntity != 0) {
            auto now = std::chrono::steady_clock::now();
            float elapsed = std::chrono::duration<float>(now - g_ArcLockTime).count();
            if (elapsed > g_ArcLockTimeout)
                g_LockedArcEntity = 0;
        }
        return;
    }

    float centerX = Width * 0.5f;
    float centerY = Height * 0.5f;

    Vector3 bestTarget{};
    Vector3 bestVelocity{};
    const PlayerDynamicData* targetArc = nullptr;
    bool targetFound = false;
    std::string targetName;
    bool usingWeakspot = false;

    const PlayerDynamicData* lockedEntity = nullptr;
    if (ArcAimCfg.targetLock && g_LockedArcEntity != 0)
    {
        for (const auto& pd : snapshot)
        {
            if (pd.entity == g_LockedArcEntity && pd.npc && !pd.destroyed && !pd.stale)
            {
                Vector3 lockLoc = pd.worldLoc;
                if (is_valid_pointer_optimized(pd.skeletalMesh)) {
                    Vector3 meshLoc = GetSceneComponentWorldLocation(pd.skeletalMesh);
                    if (is_valid_bone_position(meshLoc))
                        lockLoc = meshLoc;
                }
                Vector2 screen{};
                if (WorldToScreen(lockLoc, cameraLocation, cameraRotation, cameraFOV, aspectRatio, Width, Height, &screen))
                {
                    float sDist = sqrt((screen.x - centerX) * (screen.x - centerX) +
                        (screen.y - centerY) * (screen.y - centerY));

                    float dist = (lockLoc - cameraLocation).Length() / 100.f;

                    if (sDist <= ArcAimCfg.fov && dist <= ArcAimCfg.maxDistance)
                    {
                        lockedEntity = &pd;
                        g_ArcLockTime = std::chrono::steady_clock::now();
                    }
                }
                break;
            }
        }

        if (!lockedEntity)
            g_LockedArcEntity = 0;
    }

    if (lockedEntity)
    {
        bool lockVis = true;
        if (ArcAimCfg.visibleonly)
            lockVis = IsVisible(lockedEntity->skeletalMesh);
        if (lockVis)
            targetArc = lockedEntity;
        else
            g_LockedArcEntity = 0;
    }

    if (!targetArc)
    {
        float bestArcDist = FLT_MAX;

        for (const auto& pd : snapshot)
        {
            if (pd.stale)
                continue;
            if (!pd.npc)
                continue;
            if (pd.maxHealth > 0.0 && pd.currentHealth <= 0.0)
                continue;

            auto arcInfo = ResolveArcInfo(pd.arcname);

            if (arcInfo.index >= 0 && arcInfo.index < 21)
            {
                if (!Cfg::Scene::ArcTypeFilterMulti[arcInfo.index])
                    continue;
            }

            if (ArcAimCfg.visibleonly) {
                if (!IsVisible(pd.skeletalMesh))
                    continue;
            }

            Vector3 arcLoc = pd.worldLoc;
            if (is_valid_pointer_optimized(pd.skeletalMesh)) {
                Vector3 meshLoc = GetSceneComponentWorldLocation(pd.skeletalMesh);
                if (is_valid_bone_position(meshLoc))
                    arcLoc = meshLoc;
            }

            float dist = (arcLoc - cameraLocation).Length() / 100.f;
            if (dist > ArcAimCfg.maxDistance)
                continue;

            Vector2 screen{};
            if (!WorldToScreen(arcLoc, cameraLocation, cameraRotation, cameraFOV, aspectRatio, Width, Height, &screen))
                continue;

            float sDist = sqrt((screen.x - centerX) * (screen.x - centerX) +
                (screen.y - centerY) * (screen.y - centerY));

            if (sDist > ArcAimCfg.fov)
                continue;

            if (sDist < bestArcDist)
            {
                bestArcDist = sDist;
                targetArc = &pd;
            }
        }
    }

    if (!targetArc)
        return;

    const bool weakspotKeyPressed = g_FrameBinds.weakspot;

    // Resolve weakspot bones while the weakspot modifier is held.
    if (weakspotKeyPressed && ArcAimCfg.weakspotMode)
    {
        uintptr_t mesh = targetArc->skeletalMesh;
        if (!is_valid_pointer_optimized(mesh) && is_valid_pointer_optimized(targetArc->entity))
            mesh = ctx->read<uintptr_t>(targetArc->entity + Offsets::Get().UPawn.Mesh);

        std::string weakName = targetArc->arcDisplayName;
        if (weakName.empty() || weakName == XC("ARC")) {
            auto info = ResolveArcInfo(targetArc->arcname);
            if (info.display && info.display[0] && info.index != -2)
                weakName = info.display;
            else if (!targetArc->arcname.empty())
                weakName = targetArc->arcname;
        }

        std::unordered_map<int, Vector3> liveWeakspots;
        if (is_valid_pointer_optimized(mesh) && !weakName.empty())
            liveWeakspots = GetArcWeakspots(mesh, weakName, targetArc->worldLoc);
        if (liveWeakspots.empty())
            liveWeakspots = targetArc->weakspots;

        // Prefer the weakspot closest to crosshair; if none project, pick nearest to camera.
        float bestWeakspotDist = FLT_MAX;
        float bestCamDist = FLT_MAX;
        Vector3 bestCamPos{};
        bool haveCamFallback = false;

        for (const auto& [idx, pos] : liveWeakspots)
        {
            (void)idx;
            if (pos.IsZero())
                continue;

            float camDist = (pos - cameraLocation).Length();
            if (camDist < bestCamDist) {
                bestCamDist = camDist;
                bestCamPos = pos;
                haveCamFallback = true;
            }

            Vector2 screen{};
            if (!WorldToScreen(pos, cameraLocation, cameraRotation, cameraFOV, aspectRatio, Width, Height, &screen))
                continue;

            float sDist = sqrt((screen.x - centerX) * (screen.x - centerX) +
                (screen.y - centerY) * (screen.y - centerY));

            if (sDist < bestWeakspotDist)
            {
                bestWeakspotDist = sDist;
                bestTarget = pos;
                targetFound = true;
                usingWeakspot = true;
            }
        }

        if (!targetFound && haveCamFallback) {
            bestTarget = bestCamPos;
            targetFound = true;
            usingWeakspot = true;
        }
    }

    if (!targetFound)
    {
        bestTarget = targetArc->worldLoc;
        if (is_valid_pointer_optimized(targetArc->skeletalMesh)) {
            Vector3 meshLoc = GetSceneComponentWorldLocation(targetArc->skeletalMesh);
            if (is_valid_bone_position(meshLoc))
                bestTarget = meshLoc;
        }
        targetFound = true;
        usingWeakspot = false;
    }

    bestVelocity = targetArc->velocity;
    targetName = targetArc->arcDisplayName;

    if (!targetFound)
        return;

    if (ArcAimCfg.lines && dl)
    {
        Vector2 targetScreen{};
        if (WorldToScreen(targetArc->worldLoc, cameraLocation, cameraRotation, cameraFOV, aspectRatio, Width, Height, &targetScreen))
        {
            ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(
                ImVec4(ArcAimCfg.col_line[0], ArcAimCfg.col_line[1], ArcAimCfg.col_line[2], ArcAimCfg.col_line[3]));
            dl->AddLine(ImVec2(centerX, centerY), ImVec2(targetScreen.x, targetScreen.y), lineColor, 1.5f);
        }
    }

    if (ArcAimCfg.targetLock && g_LockedArcEntity == 0)
    {
        g_LockedArcEntity = targetArc->entity;
        g_ArcLockTime = std::chrono::steady_clock::now();
    }

    if (ArcAimCfg.showTargetInfo && dl)
    {
        float distMeters = (bestTarget - cameraLocation).Length() / 100.f;

        const char* prefix = XC("Targetting ");
        const char* fromTxt = XC(" from ");
        const char* toTxt = XC(" to ");
        const char* modeTxt = usingWeakspot ? XC("Weakspot") : XC("Center");

        char distStr[32];
        snprintf(distStr, sizeof(distStr), XC("%dm"), (int)distMeters);

        ImFont* textFont = f.custom[1];
        if (textFont)
        {

            float fontSize = textFont->FontSize;
            float y = Height * 0.58f;

            ImVec2 szPrefix = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, prefix);
            ImVec2 szName = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, targetName.c_str());
            ImVec2 szFrom = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, fromTxt);
            ImVec2 szDist = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, distStr);
            ImVec2 szTo = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, toTxt);
            ImVec2 szMode = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, modeTxt);

            float totalWidth = szPrefix.x + szName.x + szFrom.x + szDist.x + szTo.x + szMode.x;
            float startX = (Width * 0.5f) - (totalWidth * 0.5f);

            auto drawText = [&](const char* txt, ImU32 col, float& x) {
                ImVec2 sz = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, txt);
                dl->AddText(textFont, fontSize, ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 200), txt);
                dl->AddText(textFont, fontSize, ImVec2(x, y), col, txt);
                x += sz.x;
                };

            drawText(prefix, IM_COL32(255, 255, 255, 255), startX);
            drawText(targetName.c_str(), IM_COL32(120, 255, 120, 255), startX);
            drawText(fromTxt, IM_COL32(255, 255, 255, 255), startX);
            drawText(distStr, IM_COL32(120, 255, 120, 255), startX);
            drawText(toTxt, IM_COL32(255, 255, 255, 255), startX);
            drawText(modeTxt, IM_COL32(255, 165, 0, 255), startX);

            if (g_LockedArcEntity != 0)
            {
                const char* lockTxt = XC("Locked");
                ImVec2 szLock = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, lockTxt);

                float lockY = y + fontSize + 4.f;
                float lockX = (Width * 0.5f) - (szLock.x * 0.5f);

                float barWidth = szLock.x + 20.f;
                float barX = (Width * 0.5f) - (barWidth * 0.5f);
                dl->AddLine(
                    ImVec2(barX, lockY - 2.f),
                    ImVec2(barX + barWidth, lockY - 2.f),
                    IM_COL32(255, 80, 80, 180),
                    1.5f
                );

                dl->AddText(textFont, fontSize, ImVec2(lockX + 1, lockY + 1), IM_COL32(0, 0, 0, 200), lockTxt);
                dl->AddText(textFont, fontSize, ImVec2(lockX, lockY), IM_COL32(255, 80, 80, 255), lockTxt);
            }


        }
    }

    static auto lastUpdate = std::chrono::steady_clock::now();
    auto now = std::chrono::steady_clock::now();
    float elapsed = std::chrono::duration<float, std::milli>(now - lastUpdate).count();
    const float arcHz = (ArcAimCfg.maxHz > 1.f) ? ArcAimCfg.maxHz : 120.f;
    if (elapsed < (1000.f / arcHz))
        return;
    lastUpdate = now;

    Vector3 aimPos = bestTarget;
    if (ArcAimCfg.prediction && projectileSpeed > 0.f)
    {

        float speed = (maxProjectileSpeed > 0.f) ? maxProjectileSpeed : projectileSpeed;
        float gravity = std::abs(worldGravity) * projectileGravity;

        float dx = aimPos.x - cameraLocation.x;
        float dy = aimPos.y - cameraLocation.y;
        float hDist = sqrt(dx * dx + dy * dy);
        float time = hDist / speed;

        aimPos.x += bestVelocity.x * time;
        aimPos.y += bestVelocity.y * time;
        aimPos.z += bestVelocity.z * time;
        aimPos.z += 0.5f * gravity * time * time;
    }

    if (!is_valid_pointer_optimized(playerController)) return;
    if (NT::Store::Entity->IsTransitioning()) return;

    uintptr_t rotAddr = playerController + Offsets::Get().UPlayerController.ControlRotation;
    FRotator rot = ctx->read<FRotator>(rotAddr);

    if (!std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll))
        return;

    auto norm = [](double a) {
        while (a > 180.0) a -= 360.0;
        while (a < -180.0) a += 360.0;
        return a;
        };

    double dx = aimPos.x - cameraLocation.x;
    double dy = aimPos.y - cameraLocation.y;
    double dz = aimPos.z - cameraLocation.z;
    double distXY = sqrt(dx * dx + dy * dy);

    if (distXY < 1.0)
        return;

    constexpr double RAD2DEG = 180.0 / 3.14159265358979323846;

    double targetPitch = atan2(dz, distXY) * RAD2DEG;
    double targetYaw = atan2(dy, dx) * RAD2DEG;

    double dPitch = norm(targetPitch - norm(rot.Pitch)) / (std::max)(1.0, (double)ArcAimCfg.smooth);
    double dYaw = norm(targetYaw - norm(rot.Yaw)) / (std::max)(1.0, (double)ArcAimCfg.smooth);

    FRotator newRot;
    newRot.Pitch = rot.Pitch + dPitch;
    newRot.Yaw = rot.Yaw + dYaw;
    newRot.Roll = rot.Roll;

    double pitch = norm(newRot.Pitch);
    if (pitch > 89.0) newRot.Pitch = 89.0;
    else if (pitch < -89.0) newRot.Pitch = -89.0 + 360.0;

    if (!std::isfinite(newRot.Pitch) || !std::isfinite(newRot.Yaw))
        return;

    WriteWithStatus(rotAddr, newRot);
}

void ExecuteAssist(const Vector3& cameraLocation,
    const Vector2& headScreen, const Vector3& headWorld, uintptr_t playerController)
{
    // Player aimbot intentionally disabled; ARC aim remains unchanged.
    (void)cameraLocation;
    (void)headScreen;
    (void)headWorld;
    (void)playerController;
}



namespace FrameSync {
    using Clock = std::chrono::steady_clock;
    using TimePoint = Clock::time_point;

    inline TimePoint lastRaidCheck       = Clock::now();
    inline TimePoint lastRaidLightCheck  = Clock::now();
    inline TimePoint lastPlayerListUpdate = Clock::now();
    inline TimePoint lastPlayerMerge     = Clock::now();
    inline TimePoint lastDynamicWorld    = Clock::now();
    inline TimePoint lastSkeleton        = Clock::now();
    inline TimePoint lastTransitionStart = Clock::now();
    inline TimePoint lastPcmRetry        = Clock::now();
    inline TimePoint waitingWorldSince   = Clock::now();
    inline TimePoint lastCr3Recover      = Clock::now();
    inline bool pcmFoundThisRaid         = false;
    inline int raidLightFailStreak       = 0;

    inline int64_t ElapsedMs(const TimePoint& from) {
        return std::chrono::duration_cast<std::chrono::milliseconds>(Clock::now() - from).count();
    }

    inline void TryRecoverMemoryContext() {
        SPOOF_FUNC;
        if (!ctx || !ctx->process_id || !ctx->image_base)
            return;

        uintptr_t newCr3 = ctx->resolve_cr3_aggressive(ctx->process_id, ctx->image_base);
        if (newCr3)
            ctx->cr3 = newCr3;

        using namespace NT::Store;
        if (!Entity)
            return;

        PointerCache p = Entity->GetPointerSnapshot();
        bool vpAlive = false;
        if (p.ViewPort && p.ViewPort >= 0x10000ULL) {
            uint64_t uw = ctx->read<uint64_t>(p.ViewPort + Offsets::Get().UViewPort.Uworld);
            vpAlive = (uw && uw >= 0x10000ULL && uw <= 0x7FFFFFFFFFFFULL);
        }

        if (!vpAlive) {
            uint64_t vp = 0;
            uint32_t vpIdx = 0;
            if (TryRefreshViewPortOnce(vp, vpIdx))
                Entity->SetViewPort(vp, vpIdx);
        }
    }

    inline void Tick() {
        SPOOF_FUNC;
        if (!ctx)
            return;

        if (ctx->cr3 == 0) {
            if (ElapsedMs(lastCr3Recover) > 2000) {
                lastCr3Recover = Clock::now();
                TryRecoverMemoryContext();
            }
            if (ctx->cr3 == 0)
                return;
        }

        using namespace NT::Store;
        if (!Entity || !Scene || !AsyncPlayer || !AsyncWorld || !BoneSyncer)
            return;

        RaidState state = Entity->GetRaidState();

        switch (state) {

        case RaidState::WaitingForWorld:
        {
            if (ElapsedMs(waitingWorldSince) > 5000 && ElapsedMs(lastCr3Recover) > 5000) {
                lastCr3Recover = Clock::now();
                waitingWorldSince = Clock::now();
                TryRecoverMemoryContext();
            }

            if (ElapsedMs(lastRaidCheck) > 200) {
                lastRaidCheck = Clock::now();
                if (Entity->CheckInRaid()) {
                    pcmFoundThisRaid = false;
                    raidLightFailStreak = 0;
                    lastPcmRetry = Clock::now();
                    waitingWorldSince = Clock::now();

                    PointerCache p = Entity->GetPointerSnapshot();
                    if (is_valid_pointer_optimized(p.UWorld)) {
                        Scene->ScanForPCM(p.UWorld, false);
                        p = Entity->GetPointerSnapshot();
                        pcmFoundThisRaid = PcmHasUsableLivePov(p.PlayerCameraManager);
                    }
                    Entity->UpdatePlayerList();
                    lastPlayerListUpdate = Clock::now();
                    lastRaidLightCheck = Clock::now();
                }
            }
            break;
        }

        case RaidState::Transitioning:
        {
            // Wipe ESP caches immediately so lobby/map-select never shows last-raid ghosts.
            pcmFoundThisRaid = false;
            Scene->ForceReset();
            Entity->SetTransitioningDone();
            lastRaidCheck = Clock::now();
            waitingWorldSince = Clock::now();
            raidLightFailStreak = 0;
            break;
        }

        case RaidState::InRaid:
        {
            if (ElapsedMs(lastRaidLightCheck) > 200) {
                lastRaidLightCheck = Clock::now();
                if (!Entity->CheckInRaidLight()) {
                    // Faster leave detection for menu/lobby; slightly sticky for DMA blips.
                    if (++raidLightFailStreak >= 3) {
                        raidLightFailStreak = 0;
                        Entity->InvalidateRaidState();
                        lastTransitionStart = Clock::now();
                        Scene->ForceReset();
                        break;
                    }
                } else {
                    raidLightFailStreak = 0;
                }
            }

            if (ElapsedMs(lastPcmRetry) > 50) {
                lastPcmRetry = Clock::now();
                PointerCache p = Entity->GetPointerSnapshot();
                if (is_valid_pointer_optimized(p.UWorld) &&
                    !PcmHasUsableLivePov(p.PlayerCameraManager)) {
                    // Persistent / level 0 only. The 133-level FName walk saturates DMA
                    // and is the reason rematch ESP sat blank for minutes.
                    Scene->ScanForPCM(p.UWorld, false);
                    p = Entity->GetPointerSnapshot();
                    if (PcmHasUsableLivePov(p.PlayerCameraManager)) {
                        pcmFoundThisRaid = true;
                        Entity->UpdatePlayerList();
                        lastPlayerListUpdate = Clock::now();
                    } else {
                        pcmFoundThisRaid = false;
                    }
                } else if (PcmHasUsableLivePov(p.PlayerCameraManager)) {
                    pcmFoundThisRaid = true;
                }
            }

            // Heavy IndexPlayers / BoneSync / DynamicWorld run on a worker thread.
            // Keep only light raid housekeeping on the render path.
            // Faster list refresh until PCM/POV is ready so first boxes appear sooner.
            const uint64_t listInterval = pcmFoundThisRaid ? 400ull : 150ull;
            if (ElapsedMs(lastPlayerListUpdate) > listInterval) {
                lastPlayerListUpdate = Clock::now();
                Entity->UpdatePlayerList();
            }

            break;
        }

        }
    }
}

static void DrawPlayerPlatformBadge(ImDrawList* dl, ImFont* font, float fontSize, float x, float y, uint8_t plat)
{
    if (!dl || plat == 0)
        return;
    float scale = platformIconScale;
    if (scale < 0.5f)
        scale = 0.5f;
    if (scale > 3.f)
        scale = 3.f;
    const float icon = fontSize * 1.35f * scale;
    if (DrawPlatformBadgeIcon(dl, x, y + (fontSize - icon) * 0.5f, icon, plat))
        return;
    if (!font)
        return;
    const float r = fontSize * 0.42f * scale;
    ImU32 fill = IM_COL32(80, 80, 80, 255);
    char letter = '?';
    switch (plat) {
    case 1: fill = IM_COL32(27, 40, 56, 255); letter = 'S'; break;
    case 2: fill = IM_COL32(16, 124, 16, 255); letter = 'X'; break;
    case 3: fill = IM_COL32(0, 55, 145, 255); letter = 'P'; break;
    case 4: fill = IM_COL32(48, 48, 48, 255); letter = 'E'; break;
    default: return;
    }
    const ImVec2 c{ x + r, y + fontSize * 0.5f };
    dl->AddCircleFilled(c, r, fill, 12);
    dl->AddCircle(c, r, IM_COL32(0, 0, 0, 180), 12, 1.0f);
    char ch[2] = { letter, 0 };
    const float ls = fontSize * 0.62f * scale;
    ImVec2 ts = font->CalcTextSizeA(ls, FLT_MAX, 0.0f, ch);
    dl->AddText(font, ls, { c.x - ts.x * 0.5f, c.y - ts.y * 0.5f }, IM_COL32(255, 255, 255, 255), ch);
}

#ifdef _DEBUG
static void EmitCamZoomLine(
	uint64_t tMs,
	uint64_t frame,
	int custom,
	int key,
	const char* native,
	bool pcmValid,
	const char* source,
	bool lockReadOk,
	const char* lockedStr,
	const char* povStr,
	float commanded,
	const char* w2sStr,
	float aspect,
	int vpW,
	int vpH,
	uint64_t cameraAge,
	int accepted,
	int fallback,
	uint64_t failures,
	int worldDraw)
{
	if (!debugConsoleEnabled)
		return;
	char buf[768];
	snprintf(buf, sizeof(buf),
		"[CamZoom] t=%llu frame=%llu custom=%d key=%d native=%s pcm=%s source=%s "
		"lockRead=%s locked=%s pov=%s commanded=%.2f w2s=%s aspect=%.3f viewport=%dx%d "
		"cameraAge=%llu accepted=%d fallback=%d failures=%llu worldDraw=%d",
		(unsigned long long)tMs,
		(unsigned long long)frame,
		custom,
		key,
		native ? native : "UNKNOWN",
		pcmValid ? "valid" : "invalid",
		source ? source : "POV",
		lockReadOk ? "OK" : "FAIL",
		lockedStr ? lockedStr : "NA",
		povStr ? povStr : "NA",
		commanded,
		w2sStr ? w2sStr : "NA",
		aspect,
		vpW,
		vpH,
		(unsigned long long)cameraAge,
		accepted,
		fallback,
		(unsigned long long)failures,
		worldDraw);
	DebugSystem::LogConsole(buf);
}
#endif

void FrameDispatch()
{
    SPOOF_FUNC;
    try {

    // FrameSync::Tick runs on a worker — keep Present path draw-only.

    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    if (!dl)
    {
        return;
    }

    // DisplaySize is the overlay client rect that ImGui_ImplWin32_NewFrame just sampled,
    // i.e. the exact pixel space this draw list lands in, which AppWindow::SyncToGame
    // keeps pinned to Arc's client rect. Taking it from here rather than from the
    // monitor metrics means a resolution or window change is picked up on the next frame
    // with no reinit.
    const ImVec2 displaySize = ImGui::GetIO().DisplaySize;
    SyncViewportSize((int)displaySize.x, (int)displaySize.y);

    UpdateFrameBinds();
    combatModeActive = g_FrameBinds.combat;

    static LARGE_INTEGER s_qpcFreq = []{ LARGE_INTEGER f; QueryPerformanceFrequency(&f); return f; }();
    auto qpcNow = [&]() -> int64_t { LARGE_INTEGER t; QueryPerformanceCounter(&t); return t.QuadPart; };
    auto qpcUs  = [&](int64_t start, int64_t end) -> double { return (double)(end - start) * 1000000.0 / (double)s_qpcFreq.QuadPart; };

    const bool doProf = benchmarkEnabled;
    int64_t tFrameStart = doProf ? qpcNow() : 0;

    struct ProfileEntry { const char* name; double us; };
    std::vector<ProfileEntry> profEntries;
    if (doProf)
        profEntries.reserve(10);

    int64_t tA = 0, tB = 0;

    PointerCache framePointers;
    float localProjectileSpeed = 0;
    float localMaxProjectileSpeed = 0;
    float localProjectileGravity = 0;
    float localWorldGravity = 0;

    if (doProf) tA = qpcNow();
    framePointers = NT::Store::Entity->GetPointerSnapshot();

    auto* playerBuf = NT::Store::Scene->AcquirePlayerBuffer();
    struct BufGuard { NT::Store::CScene::PlayerBuffer*& b; ~BufGuard() { if (b) { b->Release(); b = nullptr; } } } _bufGuard{playerBuf};
    static const std::vector<PlayerDynamicData> emptyCache;

    const bool inRaid = NT::Store::Entity &&
        NT::Store::Entity->GetRaidState() == NT::Store::RaidState::InRaid;

    // Draw only the live buffer — no stale fallback (ghosts / flicker).
    const auto& localCache = (inRaid && playerBuf && !playerBuf->data.empty()) ? playerBuf->data : emptyCache;

    {
        std::lock_guard<std::mutex> lockGameData(g_CacheMutex);
        localProjectileSpeed = Cache.ProjectileSpeed;
        localMaxProjectileSpeed = Cache.MaxProjectileSpeed;
        localProjectileGravity = Cache.ProjectileGravity;
        localWorldGravity = Cache.WorldGravity;
    }
    if (doProf) profEntries.push_back({ XC("Acquire"), qpcUs(tA, qpcNow()) });

    // A viewport is no longer required to draw. UWorld comes from GWorld.
    if (framePointers.ViewPort == 0 && !is_valid_pointer_optimized(framePointers.UWorld))
    {
        RenderViewportLoadingNotification();
        return;
    }

    {
        uintptr_t currentUWorld = framePointers.UWorld;
        uintptr_t currentViewPort = framePointers.ViewPort;
        size_t playerCount = 0;
        size_t worldCount = 0;

        for (const auto& pd : localCache)
        {
                playerCount++;
        }
        worldCount = localCache.size() - playerCount;

        ImFont* textFont = f.custom.size() > 0 ? f.custom[0] : nullptr;
        if (textFont)
        {
            float fontSize = textFont->FontSize;
            float padding = 10.0f;
            float yPos = padding;
            float lineHeight = fontSize + 4.0f;

            char buffer[256];
            const char* platformStr = (g_PlatformVersion == PlatformVersion::Xbox) ? XC("Xbox") : XC("Steam");
            snprintf(buffer, sizeof(buffer), XC("Platform: %s"), platformStr);
            ImVec2 textSize0 = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buffer);
            float xPos0 = Width - textSize0.x - padding;
            dl->AddText(textFont, fontSize, ImVec2(xPos0 + 1, yPos + 1), IM_COL32(0, 0, 0, 200), buffer);
            dl->AddText(textFont, fontSize, ImVec2(xPos0, yPos), IM_COL32(255, 255, 255, 255), buffer);

            yPos += lineHeight;
            snprintf(buffer, sizeof(buffer), XC("UWorld: 0x%llX"), (unsigned long long)currentUWorld);
            ImVec2 textSize1 = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buffer);
            float xPos1 = Width - textSize1.x - padding;

            dl->AddText(textFont, fontSize, ImVec2(xPos1 + 1, yPos + 1), IM_COL32(0, 0, 0, 200), buffer);
            dl->AddText(textFont, fontSize, ImVec2(xPos1, yPos), IM_COL32(255, 255, 255, 255), buffer);

            yPos += lineHeight;
            snprintf(buffer, sizeof(buffer), XC("ViewPort: 0x%llX"), (unsigned long long)currentViewPort);
            ImVec2 textSizeVP = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buffer);
            float xPosVP = Width - textSizeVP.x - padding;
            dl->AddText(textFont, fontSize, ImVec2(xPosVP + 1, yPos + 1), IM_COL32(0, 0, 0, 200), buffer);
            dl->AddText(textFont, fontSize, ImVec2(xPosVP, yPos), IM_COL32(255, 255, 255, 255), buffer);
            /*
            yPos += lineHeight;
            snprintf(buffer, sizeof(buffer), XC("Players: %zu"), playerCount);
            ImVec2 textSize2 = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buffer);
            float xPos2 = Width - textSize2.x - padding;

            dl->AddText(textFont, fontSize, ImVec2(xPos2 + 1, yPos + 1), IM_COL32(0, 0, 0, 200), buffer);
            dl->AddText(textFont, fontSize, ImVec2(xPos2, yPos), IM_COL32(255, 255, 255, 255), buffer);

            yPos += lineHeight;
            snprintf(buffer, sizeof(buffer), XC("WorldItems: %zu"), worldCount);
            ImVec2 textSize3 = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, buffer);
            float xPos3 = Width - textSize3.x - padding;
            dl->AddText(textFont, fontSize, ImVec2(xPos3 + 1, yPos + 1), IM_COL32(0, 0, 0, 200), buffer);
            dl->AddText(textFont, fontSize, ImVec2(xPos3, yPos), IM_COL32(255, 255, 255, 255), buffer);*/

            if (combatModeActive)
            {
                yPos += lineHeight;
                const char* battleModeText = XC("BattleMode");
                ImVec2 textSizeBattle = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, battleModeText);
                float xPosBattle = Width - textSizeBattle.x - padding;

                dl->AddText(textFont, fontSize, ImVec2(xPosBattle + 1, yPos + 1), IM_COL32(0, 0, 0, 200), battleModeText);
                dl->AddText(textFont, fontSize, ImVec2(xPosBattle, yPos), IM_COL32(255, 0, 0, 255), battleModeText);
            }

            if (debugOverlayEnabled)
            {
                yPos += lineHeight + 5.0f;
                const char* debugHeader = XC("[DEBUG]");
                ImVec2 headerSize = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0.0f, debugHeader);
                float xHeader = Width - headerSize.x - padding;
                dl->AddText(textFont, fontSize, ImVec2(xHeader + 1, yPos + 1), IM_COL32(0, 0, 0, 200), debugHeader);
                dl->AddText(textFont, fontSize, ImVec2(xHeader, yPos), IM_COL32(255, 200, 0, 255), debugHeader);
                yPos += lineHeight;

                auto debugMsgs = DebugSystem::GetMessages();
                for (const auto& msg : debugMsgs)
                {
                    ImVec2 msgSize = textFont->CalcTextSizeA(fontSize * 0.85f, FLT_MAX, 0.0f, msg.c_str());
                    float xMsg = Width - msgSize.x - padding;
                    dl->AddText(textFont, fontSize * 0.85f, ImVec2(xMsg + 1, yPos + 1), IM_COL32(0, 0, 0, 180), msg.c_str());
                    dl->AddText(textFont, fontSize * 0.85f, ImVec2(xMsg, yPos), IM_COL32(200, 200, 200, 255), msg.c_str());
                    yPos += lineHeight * 0.85f;
                }
            }
        }
    }


	static bool s_zoomOverrideActive = false;
	bool gameAdsActive = false;
	bool localZooming = false;
#ifdef _DEBUG
	float camZoomLockedVal = 0.f;
	bool camZoomLockedOk = false;
	uintptr_t camZoomLockedPcm = 0;
	bool camZoomWroteLocked = false;
	bool povCandidateAccepted = false;
	float povSample = 0.f;
	bool povSampleOk = false;
#endif

	if (!NT::Store::Entity->IsTransitioning())
	{
		uintptr_t playerCameraManager = framePointers.PlayerCameraManager;
		if (playerCameraManager != 0 && is_valid_pointer_optimized(playerCameraManager))
		{
			const auto& camOff = Offsets::Get().CameraManager;
			const auto& offAll = Offsets::Get();

			// One-shot, read-only. Three unrelated object types so whichever
			// slot rule falls out is a rule and not a fit to one class.
			if (debugConsoleEnabled)
			{
				static bool s_slotProbeDone = false;
				if (!s_slotProbeDone)
				{
					s_slotProbeDone = true;
					Reflection::ProbeObjectSlots(ctx->image_base, playerCameraManager,
						"PlayerCameraManager");
					if (is_valid_pointer_optimized(framePointers.LocalPawn))
						Reflection::ProbeObjectSlots(ctx->image_base, framePointers.LocalPawn,
							"LocalPawn");
					if (is_valid_pointer_optimized(framePointers.UWorld))
						Reflection::ProbeObjectSlots(ctx->image_base, framePointers.UWorld,
							"UWorld");

					// The slot probe only proves the class pointer resolves. The
					// property walk needs the FField chain offsets on top of
					// that, so it has to be exercised separately to know whether
					// reflection is actually usable.
					const std::vector<Reflection::Property> props =
						Reflection::DumpProperties(ctx->image_base, playerCameraManager, 256);

					char rline[160];
					snprintf(rline, sizeof(rline),
						"[RefCheck] PlayerCameraManager properties=%d", (int)props.size());
					DebugSystem::LogConsole(rline);

					if (props.empty())
					{
						Reflection::TraceWalk(ctx->image_base, playerCameraManager,
							"PlayerCameraManager");

						// The trace says which hop fails; this says what the
						// right offset would have been.
						const uint64_t cls = NameResolver::ClassPrivate(
							ctx->image_base, playerCameraManager);
						Reflection::ProbeStructLayout(ctx->image_base, cls,
							"PlayerCameraManager");
					}
					else
					{
						// Names and offsets together: a stale FField_Offset shows
						// up as plausible names against nonsense offsets.
						std::string acc;
						for (size_t i = 0; i < props.size(); ++i)
						{
							char one[96];
							snprintf(one, sizeof(one), "%s=0x%X ",
								props[i].name.c_str(), (unsigned)props[i].offset);
							acc += one;
							if (((i + 1) % 6) == 0 || i + 1 == props.size())
							{
								DebugSystem::LogConsole("[RefProps] " + acc);
								acc.clear();
							}
						}
					}
				}
			}

			// Address the camera's own resolved FOV, which is what actually drives
			// rendering. Writing LockedFov only matters if the game copies it here.
			const uintptr_t povFovAddr = playerCameraManager + camOff.ViewTarget
				+ offAll.FTViewTarget.POV + offAll.FMinimalViewInfo.FOV;

			// LockedFov at DefaultFOV+4 drives the render on its own - the game
			// already keeps the FOV locked for its own ADS system, so the engine
			// applies it without anything else being set.
			//
			// It never propagates back into the POV struct read above, which is
			// why povFovAddr keeps reporting the unzoomed value while the screen
			// is zoomed. A full sweep of the camera manager found the commanded
			// FOV at 0x3EC and nowhere else, so there is no field to read it back
			// from; W2S is handed the commanded value directly instead.
			const uintptr_t lockedFovAddr = playerCameraManager + camOff.LockedFov;

			const bool zoomKeyPressed = zoomChangerKeybindEnabled && g_FrameBinds.zoom;
			localZooming = LocalPawnIsZooming(framePointers.LocalPawn);
			const float adsPov = ctx->read<float>(povFovAddr);
			const float adsDefaultFov = ctx->read<float>(playerCameraManager + camOff.DefaultFOV);
			const auto lens = CamFovIntent::ClassifyLens(adsPov);
			gameAdsActive = localZooming || lens != CamFovIntent::Lens::Hip;
			if (std::isfinite(adsPov) && adsPov >= 72.0f)
				CamFovIntent::NoteHipPov(adsPov);
			InterlockedExchange(&localPlayerScoped, gameAdsActive ? 1L : 0L);

			// Zoom key is allowed in ADS. Magnify the FOV frozen on key-down so
			// live POV is not remagged every frame (that compounded 80→9→3).
			// Hip FOV changer stays hip-only; the 62-76 ramp uses live POV.
			static bool s_zoomKeyHeld = false;
			static float s_zoomMagBase = 90.0f;
			bool wantOverride = false;
			float desiredFov = 0.0f;
			if (zoomKeyPressed) {
				if (!s_zoomKeyHeld) {
					s_zoomKeyHeld = true;
					if (lens == CamFovIntent::Lens::Ads &&
						std::isfinite(adsPov) && adsPov >= 8.0f && adsPov <= 170.0f)
						s_zoomMagBase = adsPov;
					else if (std::isfinite(adsDefaultFov) && adsDefaultFov >= 40.0f && adsDefaultFov <= 120.0f)
						s_zoomMagBase = adsDefaultFov;
					else
						s_zoomMagBase = 90.0f;
				}
				wantOverride = true;
				desiredFov = CamFovIntent::MagnifyFov(s_zoomMagBase, zoomChangerMagnification);
			} else {
				s_zoomKeyHeld = false;
				if (lens == CamFovIntent::Lens::Hip && zoomChangerEnabled) {
					wantOverride = true;
					desiredFov = zoomChangerFOV;
				}
			}

			if (wantOverride)
			{
				desiredFov = std::clamp(desiredFov, 1.0f, 170.0f);

				float currentFov = 0.0f;

				// Tell the camera validator that any large FOV step from here is
				// ours, so it does not mistake it for a torn read and fall back to
				// a cached pose. Marked before the write so the very first zoomed
				// frame the render thread sees is already covered.
				CamFovIntent::Mark(desiredFov);

				// Re-assert rather than writing once on change: the game rewrites
				// LockedFov during its own camera update, so a single write is
				// dropped within a frame or two. The read-back keeps this to one
				// write per real divergence rather than one per frame.
				currentFov = ctx->read<float>(lockedFovAddr);
#ifdef _DEBUG
				if (ctx->last_status() == TransportStatus::Ok && std::isfinite(currentFov)) {
					camZoomLockedVal = currentFov;
					camZoomLockedOk = true;
					camZoomLockedPcm = playerCameraManager;
				}
#endif
				if (ctx->last_status() != TransportStatus::Ok || std::fabs(currentFov - desiredFov) > 0.01f)
#ifdef _DEBUG
				{
					WriteWithStatus(lockedFovAddr, desiredFov);
					camZoomWroteLocked = true;
				}
#else
					WriteWithStatus(lockedFovAddr, desiredFov);
#endif
				{
					const float lockNow = ctx->read<float>(lockedFovAddr);
					if (ctx->last_status() == TransportStatus::Ok && std::isfinite(lockNow)) {
						CamFovIntent::NoteLockReadback(lockNow);
#ifdef _DEBUG
						camZoomLockedVal = lockNow;
						camZoomLockedOk = true;
						camZoomLockedPcm = playerCameraManager;
#endif
					} else {
						CamFovIntent::NoteLockReadback(0.0f);
					}
				}

				if (debugConsoleEnabled)
				{
					static uint64_t s_lastZoomDiagMs = 0;
					const uint64_t nowZoom = GetTickCount64();
					if ((nowZoom - s_lastZoomDiagMs) > 2000)
					{
						s_lastZoomDiagMs = nowZoom;
						const float readBack = ctx->read<float>(lockedFovAddr);
						const bool readBackOk = ctx->last_status() == TransportStatus::Ok;
						const float defaultFov = ctx->read<float>(playerCameraManager + camOff.DefaultFOV);
						const float povNow = ctx->read<float>(povFovAddr);
						char zline[288];
						snprintf(zline, sizeof(zline),
							"[ZoomDiag] pcm=0x%llX lockOff=0x%llX want=%.2f before=%.2f after=%.2f "
							"pov=%.2f readOk=%d defaultFov=%.2f key=%d mag=%.1f ads=%d base=%.2f",
							(unsigned long long)playerCameraManager,
							(unsigned long long)camOff.LockedFov,
							desiredFov, currentFov, readBack, povNow,
							readBackOk ? 1 : 0, defaultFov,
							zoomKeyPressed ? 1 : 0, zoomChangerMagnification,
							gameAdsActive ? 1 : 0, s_zoomMagBase);
						DebugSystem::LogConsole(zline);
					}
				}

				s_zoomOverrideActive = true;
			}
			else if (s_zoomOverrideActive)
			{
				// Release once. Holding this every frame would fight the game's own
				// legitimate uses of LockedFov.
				WriteWithStatus(lockedFovAddr, 0.0f);
#ifdef _DEBUG
				camZoomWroteLocked = true;
#endif

				// Restart the window on release: the FOV ramping back is just as
				// large a step as the one going in.
				CamFovIntent::Mark();
				s_zoomOverrideActive = false;
			}
#ifdef _DEBUG
			if (!camZoomLockedOk) {
				const float lf = ctx->read<float>(lockedFovAddr);
				if (ctx->last_status() == TransportStatus::Ok && std::isfinite(lf)) {
					camZoomLockedVal = lf;
					camZoomLockedOk = true;
					camZoomLockedPcm = playerCameraManager;
				}
			}
#endif
		}

		static int lastViewMode = 0;
		(void)lastViewMode;
		// viewMode has no menu and was writing UViewPort.ViewIndex on change. That
		// write is unused; leave ViewIndex alone.
	}

#ifdef _DEBUG
	if (camProjectionTest == 2) {
		const uintptr_t pcmNow = framePointers.PlayerCameraManager;
		if (is_valid_pointer_optimized(pcmNow)) {
			if (camZoomLockedPcm != 0 && camZoomLockedPcm != pcmNow)
				camZoomLockedOk = false;
			if (!camZoomLockedOk || camZoomWroteLocked) {
				const uintptr_t addr = pcmNow + Offsets::Get().CameraManager.LockedFov;
				const float lv = ctx->read<float>(addr);
				if (ctx->last_status() == TransportStatus::Ok && std::isfinite(lv)) {
					camZoomLockedVal = lv;
					camZoomLockedOk = true;
					camZoomLockedPcm = pcmNow;
				} else {
					camZoomLockedOk = false;
					camZoomLockedPcm = 0;
				}
			}
		} else {
			camZoomLockedOk = false;
			camZoomLockedPcm = 0;
		}
	}
#endif

	uintptr_t localPawnPtr = framePointers.LocalPawn;
	{
		static float s_lastFlashR = -1, s_lastFlashG = -1, s_lastFlashB = -1, s_lastFlashA = -1;
		static bool s_lastFlashEnabled = false;
		bool flashChanged = Cfg::Scene::flashlightColorChangerEnabled != s_lastFlashEnabled
			|| Cfg::Scene::flashlightColor[0] != s_lastFlashR
			|| Cfg::Scene::flashlightColor[1] != s_lastFlashG
			|| Cfg::Scene::flashlightColor[2] != s_lastFlashB
			|| Cfg::Scene::flashlightColor[3] != s_lastFlashA;
		if (flashChanged && Cfg::Scene::flashlightColorChangerEnabled &&
			!NT::Store::Entity->IsTransitioning() &&
			(localPawnPtr != 0) && is_valid_pointer_optimized(localPawnPtr))
		{
			UPawn localPawn(localPawnPtr);
			if (localPawn.IsValid())
			{
				UFlashlightComponent flashlight = localPawn.GetFlashlightComponent();
				if (flashlight.IsValid())
				{
					FLinearColor newColor(Cfg::Scene::flashlightColor[0], Cfg::Scene::flashlightColor[1], Cfg::Scene::flashlightColor[2], Cfg::Scene::flashlightColor[3]);
					flashlight.SetFlashlightColor(newColor);
				}
			}
			s_lastFlashR = Cfg::Scene::flashlightColor[0];
			s_lastFlashG = Cfg::Scene::flashlightColor[1];
			s_lastFlashB = Cfg::Scene::flashlightColor[2];
			s_lastFlashA = Cfg::Scene::flashlightColor[3];
		}
		s_lastFlashEnabled = Cfg::Scene::flashlightColorChangerEnabled;
	}

    uintptr_t localUWorld = framePointers.UWorld;
    uintptr_t localPlayerController = framePointers.PlayerController;
    uintptr_t localLocalPawn = framePointers.LocalPawn;
    uintptr_t localDrawPS = 0;
    uintptr_t localAckPawn = 0;
    if (is_valid_pointer_optimized(localPlayerController))
        localAckPawn = ctx->read<uintptr_t>(
            localPlayerController + Offsets::Get().UPlayerController.AcknowledgedPawn);
    if (is_valid_pointer_optimized(localLocalPawn))
        localDrawPS = ctx->read<uintptr_t>(localLocalPawn + Offsets::Get().UPawn.PlayerState);
    const LocalAvatarIds localIds = CaptureLocalAvatarIds(
        localLocalPawn, localAckPawn, localDrawPS,
        localPlayerController, framePointers.PlayerCameraManager);
    Vector3 localRelativeLocation = framePointers.RelativeLocation;

    bool frameValid = true;
    bool usedFallbackCamera = false;
    uintptr_t localPlayerCameraManager = framePointers.PlayerCameraManager;
    Vector3 cameraLocation{};
    FRotator cameraRotation{};
    float cameraFOV = 90.0f;
    float aspectRatio = (float)Width / (float)Height;

    auto IsValidCameraFrame = [&](const Vector3& loc, const FRotator& rot, float fov) -> bool
    {
        if (fov < 1.0f || fov > 170.0f)
            return false;
        if (!std::isfinite(loc.x) || !std::isfinite(loc.y) || !std::isfinite(loc.z))
            return false;
        if (!std::isfinite(rot.Pitch) || !std::isfinite(rot.Yaw) || !std::isfinite(rot.Roll))
            return false;
        if (fabs(loc.x) < 0.001f && fabs(loc.y) < 0.001f && fabs(loc.z) < 0.001f)
            return false;
        if (PcmPovLooksLikeDummyLens(loc, rot))
            return false;
        return true;
    };

    if (doProf) tA = qpcNow();
    uintptr_t drawPcm = framePointers.PlayerCameraManager;
    if (LooksLikeCanonicalUObjectPtr(localPlayerController) &&
        g_PlatformVersion != PlatformVersion::Xbox)
    {
        const uintptr_t cand = ctx->read<uintptr_t>(
            localPlayerController + Offsets::Steam::UPlayerController_PlayerCameraManager);
        // Prefer the PC field when the cached PCM is missing or its POV is not
        // ready yet — rematch often has PC.PCM live before the scan commits it.
        if (is_valid_pointer_optimized(cand) &&
            (!is_valid_pointer_optimized(drawPcm) || cand != drawPcm))
        {
            if (!is_valid_pointer_optimized(drawPcm) || PcmLooksBindable(cand, localPlayerController))
                drawPcm = cand;
        }
    }
    if (is_valid_pointer_optimized(drawPcm))
    {
        frameValid = GetCameraInfo(drawPcm, cameraLocation, cameraRotation, cameraFOV, localUWorld);
        if (frameValid)
        {
            // ControlRotation before IsValidCameraFrame — spawn POV often has
            // Pitch/Yaw 0 and would be rejected as dummy/degenerate otherwise.
            if (cameraRotation.Pitch == 0.0 && cameraRotation.Yaw == 0.0) {
                FRotator cr{};
                if (TryControlRotationFromPcm(drawPcm, cr) ||
                    TryControlRotationFromPc(localPlayerController, cr))
                    cameraRotation = cr;
            }
            aspectRatio = (Height != 0) ? ((float)Width / (float)Height) : 1.0f;
            frameValid = IsValidCameraFrame(cameraLocation, cameraRotation, cameraFOV);
            if (frameValid && drawPcm != framePointers.PlayerCameraManager && NT::Store::Entity) {
#ifdef DEBUG_CONSOLE
                CamBindDiag::BeginSet(CamBindDiag::Source::DrawPcField);
#endif
                NT::Store::Entity->SetPlayerCameraManager(drawPcm);
            }
#ifdef _DEBUG
            if (frameValid) {
                povCandidateAccepted = true;
                povSample = cameraFOV;
                povSampleOk = true;
            }
#endif
        }
        // Cached PCM failed — retry the PC field explicitly once.
        if (!frameValid && LooksLikeCanonicalUObjectPtr(localPlayerController) &&
            g_PlatformVersion != PlatformVersion::Xbox)
        {
            const uintptr_t cand = ctx->read<uintptr_t>(
                localPlayerController + Offsets::Steam::UPlayerController_PlayerCameraManager);
            if (is_valid_pointer_optimized(cand) && cand != drawPcm) {
                Vector3 loc2{};
                FRotator rot2{};
                float fov2 = 90.f;
                if (GetCameraInfo(cand, loc2, rot2, fov2, localUWorld)) {
                    if (rot2.Pitch == 0.0 && rot2.Yaw == 0.0) {
                        FRotator cr{};
                        if (TryControlRotationFromPcm(cand, cr) ||
                            TryControlRotationFromPc(localPlayerController, cr))
                            rot2 = cr;
                    }
                    if (IsValidCameraFrame(loc2, rot2, fov2)) {
                        drawPcm = cand;
                        cameraLocation = loc2;
                        cameraRotation = rot2;
                        cameraFOV = fov2;
                        aspectRatio = (Height != 0) ? ((float)Width / (float)Height) : 1.0f;
                        frameValid = true;
                        if (NT::Store::Entity) {
#ifdef DEBUG_CONSOLE
                            CamBindDiag::BeginSet(CamBindDiag::Source::DrawPcField);
#endif
                            NT::Store::Entity->SetPlayerCameraManager(drawPcm);
                        }
                    }
                }
            }
        }
    }
    else
    {
        frameValid = false;
#ifdef DEBUG_CONSOLE
        CamReadDiag::ResetSnap();
        CamReadDiag::g_last.reject = CamReadDiag::Reject::NoPcm;
#endif
    }
    if (is_valid_pointer_optimized(drawPcm))
        localPlayerCameraManager = drawPcm;
    // Pawn-root / PCM-root cameras project from the body at FOV 90 while the
    // real lens is 6m behind at 80. Log: pcm=invalid source=FALLBACK worldDraw=1
    // for minutes until ScanForPCM bound. Skip ESP instead.
    if (!frameValid)
    {
        CachedCameraData prevCam = NT::Store::Entity->GetCameraSnapshot();
        const uint64_t nowCamMs = GetNowMs();
        const uint64_t camAge = (nowCamMs >= prevCam.lastUpdateMs)
            ? (nowCamMs - prevCam.lastUpdateMs)
            : ~0ull;
        // Hold last-good a bit longer across spawn POV holes (was 2s).
        if (prevCam.valid &&
            prevCam.uworld != 0 &&
            prevCam.uworld == localUWorld &&
            camAge <= 3500 &&
            IsValidCameraFrame(prevCam.location, prevCam.rotation, prevCam.fov))
        {
            cameraLocation = prevCam.location;
            cameraRotation = prevCam.rotation;
            cameraFOV = prevCam.fov;
            if (prevCam.aspectRatio > 0.01f)
                aspectRatio = prevCam.aspectRatio;
            frameValid = true;
            usedFallbackCamera = true;
        }
    }
    // Debug 50 rematch: POV location is live while Pitch/Yaw stay exactly 0 (FMinimalViewInfo
    // default). IsValidCameraFrame accepts that, so W2S looked along +Y until ViewTarget
    // wrote a real rotator (~1.5s). Patch rotation only — do not invent a camera pose.
    if (frameValid && cameraRotation.Pitch == 0.0 && cameraRotation.Yaw == 0.0)
    {
        FRotator cr{};
        if (TryControlRotationFromPcm(localPlayerCameraManager, cr) ||
            TryControlRotationFromPc(localPlayerController, cr))
            cameraRotation = cr;
        else {
            uintptr_t rotPc = 0;
            if (is_valid_pointer_optimized(localPlayerController))
                rotPc = localPlayerController;
            else if (is_valid_pointer_optimized(localPlayerCameraManager)) {
                const uintptr_t owner = ctx->read<uintptr_t>(
                    localPlayerCameraManager + Offsets::Get().CameraManager.PCOwner);
                if (is_valid_pointer_optimized(owner))
                    rotPc = owner;
            }
            if (is_valid_pointer_optimized(rotPc)) {
                const FRotator cr2 = ctx->read<FRotator>(
                    rotPc + Offsets::Get().UPlayerController.ControlRotation);
                if (std::isfinite(cr2.Pitch) && std::isfinite(cr2.Yaw) && std::isfinite(cr2.Roll) &&
                    !(cr2.Pitch == 0.0 && cr2.Yaw == 0.0) &&
                    fabs(cr2.Pitch) <= 90.5 && fabs(cr2.Yaw) <= 360.5 && fabs(cr2.Roll) <= 180.5)
                {
                    cameraRotation = cr2;
                }
            }
        }
    }
    if (doProf) profEntries.push_back({ XC("Camera"), qpcUs(tA, qpcNow()) });
    (void)localPlayerCameraManager;

    // The premise this used to rest on - that a LockedFov write shows up in the
    // POV the ESP reads - is false. [ZoomDiag] has the screen visibly zoomed to
    // the commanded value while POV.FOV at +0xCE8 still reports 45 or 80, so the
    // renderer is taking its FOV from somewhere this code does not read.
    // Projecting with the unzoomed number is the misalignment.
    //
    // Overlay zoom key (and hip FOV changer) still substitute commanded FOV.
    // Game ADS/scope is the opposite problem: POV drops to ~52 while FOV changer
    // keeps Mark(99.55), so substituting commanded shrinks every box toward center.
    {
        localZooming = LocalPawnIsZooming(localLocalPawn);
        float adsDefaultFov = 90.0f;
        if (is_valid_pointer_optimized(drawPcm)) {
            adsDefaultFov = ctx->read<float>(
                drawPcm + Offsets::Get().CameraManager.DefaultFOV);
        }
        gameAdsActive = localZooming || CamFovIntent::PovLooksLikeGameAds(cameraFOV, adsDefaultFov);
        if (std::isfinite(cameraFOV) && cameraFOV >= 72.0f)
            CamFovIntent::NoteHipPov(cameraFOV);
        InterlockedExchange(&localPlayerScoped, gameAdsActive ? 1L : 0L);
    }
#ifdef _DEBUG
    const float commandedFovVal = CamFovIntent::Commanded();
    const float appliedFovVal = CamFovIntent::ForW2S();
    const int projMode = (camProjectionTest == 1 || camProjectionTest == 2) ? camProjectionTest : 0;
    const char* w2sSource = "POV";
    auto fovPlausible = [](float f) {
        return std::isfinite(f) && f >= 1.0f && f <= 170.0f;
    };
    if (camZoomLockedOk && camZoomLockedPcm != framePointers.PlayerCameraManager)
        camZoomLockedOk = false;

    const bool overlayZoomKey = zoomChangerKeybindEnabled && g_FrameBinds.zoom;
    (void)overlayZoomKey;
    if (projMode == 0) {
        if (appliedFovVal > 0.0f) {
            cameraFOV = appliedFovVal;
            w2sSource = "COMMANDED";
        } else if (usedFallbackCamera) {
            w2sSource = "FALLBACK";
        } else {
            w2sSource = "POV";
        }
    } else if (projMode == 1) {
        w2sSource = "POV";
        if (povCandidateAccepted && fovPlausible(povSample)) {
            cameraFOV = povSample;
        } else {
            frameValid = false;
        }
    } else {
        const bool lockedUse = camZoomLockedOk
            && camZoomLockedPcm == framePointers.PlayerCameraManager
            && fovPlausible(camZoomLockedVal);
        if (lockedUse) {
            cameraFOV = camZoomLockedVal;
            w2sSource = "LOCKED";
            if (!IsValidCameraFrame(cameraLocation, cameraRotation, cameraFOV))
                frameValid = false;
        } else if (povCandidateAccepted && fovPlausible(povSample)) {
            cameraFOV = povSample;
            w2sSource = "POV";
        } else {
            frameValid = false;
            w2sSource = "POV";
        }
    }
#else
    if (const float appliedFov = CamFovIntent::ForW2S(); appliedFov > 0.0f)
        cameraFOV = appliedFov;
#endif

#ifdef _DEBUG
    {
        if (camProjectionTest == 1 || camProjectionTest == 2) {
            const char* lab = (camProjectionTest == 1) ? "PROJECTION TEST: POV" : "PROJECTION TEST: LOCKED";
            ImFont* font = ImGui::GetFont();
            const float fs = 18.0f;
            ImVec2 sz = font ? font->CalcTextSizeA(fs, FLT_MAX, 0.0f, lab) : ImVec2(220.0f, 18.0f);
            const float x = ((float)Width - sz.x) * 0.5f;
            const float y = 12.0f;
            dl->AddText(font, fs, ImVec2(x + 1.0f, y + 1.0f), IM_COL32(0, 0, 0, 220), lab);
            dl->AddText(font, fs, ImVec2(x, y), IM_COL32(255, 220, 0, 255), lab);
        }

        static bool s_prevZoomOverride = false;
        static bool s_prevZoomChanger = false;
        static int s_prevProjMode = 0;
        static bool s_prevLockPovSplit = false;
        static bool s_prevGameAds = false;
        static uint64_t s_lastCamZoomLogMs = 0;
        static uint64_t s_camZoomFrame = 0;
        static uint64_t s_camFailStreak = 0;
        ++s_camZoomFrame;

        if (povCandidateAccepted)
            s_camFailStreak = 0;
        else if (inRaid)
            ++s_camFailStreak;

        const uint64_t nowZoomMs = GetTickCount64();
        uint64_t cameraAgeMs = 0;
        if (CamLastGood::initialized && nowZoomMs >= CamLastGood::tick)
            cameraAgeMs = nowZoomMs - CamLastGood::tick;

        const bool pcmValid = is_valid_pointer_optimized(drawPcm);
        const int customNow = s_zoomOverrideActive ? 1 : 0;
        const int keyNow = (zoomChangerKeybindEnabled && g_FrameBinds.zoom) ? 1 : 0;

        const char* nativeStr = "UNKNOWN";
        if (is_valid_pointer_optimized(localLocalPawn))
            nativeStr = localZooming ? "1" : "0";
        else if (localLocalPawn != 0) {
            for (const auto& pd : localCache) {
                if (pd.entity == localLocalPawn) {
                    nativeStr = pd.isZooming ? "1" : "0";
                    break;
                }
            }
        }

        bool lockPovSplit = false;
        if (camZoomLockedOk && povSampleOk)
            lockPovSplit = std::fabs(camZoomLockedVal - povSample) >= 5.0f;

        bool emit = false;
        if (customNow != (s_prevZoomOverride ? 1 : 0))
            emit = true;
        if (zoomChangerEnabled != s_prevZoomChanger)
            emit = true;
        if (projMode != s_prevProjMode)
            emit = true;
        if (camZoomLockedOk && povSampleOk && lockPovSplit != s_prevLockPovSplit)
            emit = true;
        if (gameAdsActive != s_prevGameAds)
            emit = true;
        if (inRaid && (nowZoomMs - s_lastCamZoomLogMs) >= 500)
            emit = true;

        if (emit) {
            char lockedStr[32];
            char povStr[32];
            char w2sStr[32];
            if (camZoomLockedOk)
                snprintf(lockedStr, sizeof(lockedStr), "%.2f", camZoomLockedVal);
            else
                snprintf(lockedStr, sizeof(lockedStr), "NA");
            if (povSampleOk)
                snprintf(povStr, sizeof(povStr), "%.2f", povSample);
            else
                snprintf(povStr, sizeof(povStr), "NA");
            if (frameValid)
                snprintf(w2sStr, sizeof(w2sStr), "%.2f", cameraFOV);
            else
                snprintf(w2sStr, sizeof(w2sStr), "NA");

            EmitCamZoomLine(
                nowZoomMs,
                s_camZoomFrame,
                customNow,
                keyNow,
                nativeStr,
                pcmValid,
                w2sSource,
                camZoomLockedOk,
                lockedStr,
                povStr,
                commandedFovVal,
                w2sStr,
                aspectRatio,
                Width,
                Height,
                cameraAgeMs,
                povCandidateAccepted ? 1 : 0,
                usedFallbackCamera ? 1 : 0,
                s_camFailStreak,
                frameValid ? 1 : 0);
            s_lastCamZoomLogMs = nowZoomMs;
        }

        s_prevZoomOverride = s_zoomOverrideActive;
        s_prevZoomChanger = zoomChangerEnabled;
        s_prevProjMode = projMode;
        s_prevGameAds = gameAdsActive;
        if (camZoomLockedOk && povSampleOk)
            s_prevLockPovSplit = lockPovSplit;
    }
#endif

    if (frameValid)
        NT::Store::Entity->UpdateCamera(cameraLocation, cameraRotation, cameraFOV, aspectRatio, localUWorld);

#ifdef DEBUG_CONSOLE
    auto EspReadyTick = [&](bool drewPlayerEsp) {
        if (!inRaid)
            return;
        EspReadyDiag::Frame fr{};
        fr.inRaid = true;
        fr.world = is_valid_pointer_optimized(localUWorld);
        fr.pcmCached = is_valid_pointer_optimized(framePointers.PlayerCameraManager);
        fr.povValid = frameValid;
        fr.drewPlayerEsp = drewPlayerEsp;
        EspReadyDiag::NoteMilestones(fr);
        if (!EspReadyDiag::WantsPublish(fr))
            return;
        EspReadyDiag::Publish(fr);
    };

    auto CamBindTick = [&]() {
        CamBindDiag::Frame cf{};
        cf.world = is_valid_pointer_optimized(localUWorld);
        cf.localPc = is_valid_pointer_optimized(localPlayerController);
        cf.localPawn = is_valid_pointer_optimized(localLocalPawn);
        cf.pcmCached = is_valid_pointer_optimized(framePointers.PlayerCameraManager);
        cf.cameraAccepted = frameValid;
        cf.lastGoodUsed = CamReadDiag::g_last.lastGoodUsed;
        cf.usedFallback = usedFallbackCamera;
        if (cf.lastGoodUsed)
            cf.lastGoodAgeMs = CamReadDiag::g_last.lastGoodAgeMs;
        else if (usedFallbackCamera) {
            CachedCameraData prevAge = NT::Store::Entity->GetCameraSnapshot();
            const uint64_t nowAge = GetNowMs();
            cf.lastGoodAgeMs = (nowAge >= prevAge.lastUpdateMs) ? (nowAge - prevAge.lastUpdateMs) : ~0ull;
        }
        const uintptr_t povPcm = is_valid_pointer_optimized(drawPcm)
            ? drawPcm : framePointers.PlayerCameraManager;
        if (is_valid_pointer_optimized(povPcm) &&
            povPcm == framePointers.PlayerCameraManager) {
            cf.viewTargetKnown = true;
            cf.viewTargetPresent = is_valid_pointer_optimized(localIds.viewTarget) ? 1 : 0;
            if (cf.viewTargetPresent == 1) {
                if (is_valid_pointer_optimized(localLocalPawn))
                    cf.viewTargetMatchesPawn = (localIds.viewTarget == localLocalPawn) ? 1 : 0;
                else
                    cf.viewTargetMatchesPawn = -1;
            }
        }
        if (frameValid && CamReadDiag::g_last.reject != CamReadDiag::Reject::None &&
            !cf.lastGoodUsed && !cf.usedFallback) {
            cf.loopRejectOverride = true;
            cf.loopReject = CamReadDiag::g_last.reject;
        }
        if (!frameValid) {
            if (!cf.world) {
                cf.loopRejectOverride = true;
                cf.loopReject = CamReadDiag::Reject::NoWorld;
            } else if (!is_valid_pointer_optimized(drawPcm) &&
                !is_valid_pointer_optimized(framePointers.PlayerCameraManager)) {
                cf.loopRejectOverride = true;
                cf.loopReject = CamReadDiag::Reject::NoPcm;
            } else if (!IsValidCameraFrame(cameraLocation, cameraRotation, cameraFOV)) {
                cf.loopRejectOverride = true;
                if (!std::isfinite(cameraLocation.x) || !std::isfinite(cameraLocation.y) ||
                    !std::isfinite(cameraLocation.z))
                    cf.loopReject = CamReadDiag::Reject::LocationNonFinite;
                else if (fabs(cameraLocation.x) < 0.001f && fabs(cameraLocation.y) < 0.001f &&
                    fabs(cameraLocation.z) < 0.001f)
                    cf.loopReject = CamReadDiag::Reject::LocationZero;
                else if (!std::isfinite(cameraFOV) || cameraFOV < 1.0f || cameraFOV > 170.0f)
                    cf.loopReject = CamReadDiag::Reject::FovOutOfRange;
                else
                    cf.loopReject = CamReadDiag::g_last.reject;
            }
        }
        CamBindDiag::Publish(cf);
    };
#endif

    if (!frameValid)
    {
#ifdef DEBUG_CONSOLE
        CamBindTick();
        EspReadyTick(false);
#endif
        if (debugConsoleEnabled) {
            static uint64_t s_camSkipLastMs = 0;
            uint64_t nowSkipMs = GetTickCount64();
            if ((nowSkipMs - s_camSkipLastMs) >= 1000) {
                s_camSkipLastMs = nowSkipMs;
                char sline[320];
                uintptr_t pcmField = 0;
                if (is_valid_pointer_optimized(localPlayerController) &&
                    g_PlatformVersion != PlatformVersion::Xbox)
                    pcmField = ctx->read<uintptr_t>(
                        localPlayerController + Offsets::Steam::UPlayerController_PlayerCameraManager);
                snprintf(sline, sizeof(sline),
                    "[CamDiag] skipped ESP: no live camera for UWorld=0x%llX pcm=0x%llX "
                    "pc=0x%llX pcmField=0x%llX pawn=0x%llX loc=(%.0f,%.0f,%.0f) rot=(%.1f,%.1f) fov=%.1f",
                    (unsigned long long)localUWorld,
                    (unsigned long long)framePointers.PlayerCameraManager,
                    (unsigned long long)localPlayerController,
                    (unsigned long long)pcmField,
                    (unsigned long long)localLocalPawn,
                    cameraLocation.x, cameraLocation.y, cameraLocation.z,
                    cameraRotation.Pitch, cameraRotation.Yaw, cameraFOV);
                DebugSystem::LogConsole(sline);
            }
        }
        return;
    }

#ifdef DEBUG_CONSOLE
    CamBindTick();
#endif

    ViewContext vc;
    vc.Build(cameraLocation, cameraRotation, cameraFOV, aspectRatio, Width, Height);

    uint64_t nowMs = std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()).count();

    if (localCache.empty())
    {
#ifdef DEBUG_CONSOLE
        EspReadyTick(false);
#endif
        return;
    }

    std::vector<ArrowTarget> localArrows;
    localArrows.reserve(localCache.size());

    float bestAimDist = FLT_MAX;
    Vector3 bestAimBone{};
    Vector2 bestAimScreen{};
    Vector3 originalBonePos{};
    bool TargetVisible = false;
    bool aimTargetFound = false;
    std::string bestTargetName{};
    float centerX = Width * 0.5f;
    float centerY = Height * 0.5f;
    auto ApplyMotion = [&](const PlayerDynamicData& pd, const Vector3& pos) -> Vector3
    {
        if (pd.lastUpdateMs == 0)
            return pos;
        uint64_t dtMs = (nowMs > pd.lastUpdateMs) ? (nowMs - pd.lastUpdateMs) : 0;
        // Keep extrapolation short — long dt + bad velocity is a common ESP snap source.
        if (dtMs == 0 || dtMs > 80)
            return pos;
        const double speed = pd.velocity.Length();
        if (!std::isfinite(speed) || speed < 5.0 || speed > 2500.0)
            return pos;
        double dt = static_cast<double>(dtMs) / 1000.0;
        return pos + (pd.velocity * dt);
    };

    if (drawfov && AimCfg.enable)
    {
        ImU32 fovColor = ImGui::ColorConvertFloat4ToU32(ImVec4(col_fov[0], col_fov[1], col_fov[2], col_fov[3]));
        dl->AddCircle(ImVec2(Width * 0.5f, Height * 0.5f), AimCfg.fov, fovColor, 64, 1.5f);
    }

    if (ArcAimCfg.drawFov && ArcAimCfg.enable && !drawfov)
    {
        ImU32 arcFovColor = ImGui::ColorConvertFloat4ToU32(ImVec4(ArcAimCfg.col_fov[0], ArcAimCfg.col_fov[1], ArcAimCfg.col_fov[2], ArcAimCfg.col_fov[3]));
        dl->AddCircle(ImVec2(Width * 0.5f, Height * 0.5f), AimCfg.fov, arcFovColor, 64, 1.5f);
    }

    int cacheIterationCount = 0;
    int skippedNoFont = 0;
    int processedEntities = 0;
    int dbgRealPlayers = 0;
    int dbgEspDrawn = 0;
    int dbgSkelDrawn = 0;
    int dbgNoSkel = 0;
    int dbgW2sFail = 0;
    int dbgTeamSkip = 0;
    int dbgSelfSkip = 0;
    int dbgFrozenSkip = 0;
    int dbgBonesValid = 0;
    int dbgFullSkelFlag = 0;
    int dbgBoneUnresolvedSkip = 0;
    int dbgPosFallbackDrawn = 0;
    int dbgPlatOk = 0;
    int dbgOtherSkip = 0;
    std::string dbgNoSkelNames;

    // dbgOtherSkip stays the total so the [Render] other= field keeps its meaning;
    // these break it down by cause. Five separate gates were feeding one counter,
    // which is why the log could not show which one was dropping entities.
    int dbgSkipBaseLoc = 0;
    int dbgSkipDistance = 0;
    int dbgSkipBoxExtent = 0;
    int dbgSkipOffScreenX = 0;
    int dbgSkipInvisible = 0;
    int dbgSkipVisualsOff = 0;

    // Every enemy that reaches this loop and does not get drawn is named with the gate
    // that dropped it. The aggregate counters could say three players were culled but
    // never which three, so a "that guy had no ESP" report could not be matched to a
    // cause. Squad members are left out - they are meant to be invisible.
    std::string dbgSkipNames;
    auto noteSkip = [&dbgSkipNames](const std::string& who, const char* why) {
        if (dbgSkipNames.size() > 400)
            return;
        if (!dbgSkipNames.empty())
            dbgSkipNames += ", ";
        dbgSkipNames += who.empty() ? "?" : who;
        dbgSkipNames += ':';
        dbgSkipNames += why;
    };

    // Smallest ESP box this frame, for [BoxDiag]. The box extent collapses as the
    // view direction lines up with an entity's vertical axis, so tracking the
    // minimum against camera pitch and the entity's elevation angle is what shows
    // whether a near-zero box is geometry or a bad read.
    float dbgMinBoxH = FLT_MAX;
    float dbgMinBoxElev = 0.f;
    float dbgMinBoxDist = 0.f;
    std::string dbgMinBoxName;

    // [W2SDiag] buffer: filled at the real dbgW2sFail failure point below (reuses
    // the actual Project() rejection, does not re-derive it), flushed only if this
    // frame's total w2sFail reaches the flicker threshold - see the flush site
    // right after the per-entity loop for the >=8 gate and the global rate limit.
    struct W2SDiagEntry {
        std::string name;
        Vector3 worldPos;
        float camDist;
        Vector2 projected;
        float depth;
        bool hasProjected;
        const char* reason;
    };
    std::vector<W2SDiagEntry> w2sDiagBuffer;

    enum class WorldStackType { Loot, Container, Salvage };
    struct WorldStackItem
    {
        WorldStackType type;
        Vector2 screen;
        Vector3 world;
        float dist;
        ImU32 color;
        const char* name = "";
        int amount = 0;
        int price = 0;
        double weight = 0.0;
        bool opened = false;
    };

    std::vector<WorldStackItem> worldStackItems;
    if (Cfg::Scene::FovStacking)
        worldStackItems.reserve(localCache.size());


    int64_t tLoopStart = doProf ? qpcNow() : 0;
    double tPlayerUs = 0;

    double matchTimeLeftSec = -1.0;
    {
        static double s_cachedMatchTime = -1.0;
        static uint64_t s_lastMatchTimeReadMs = 0;
        uint64_t curMs = GetNowMs();
        if (Cfg::Scene::ExtractionsESP && Cfg::Scene::ExtractionTimeLeft &&
            framePointers.UWorld != 0 && is_valid_pointer_optimized(framePointers.UWorld) &&
            (curMs - s_lastMatchTimeReadMs) > 1000)
        {
            s_lastMatchTimeReadMs = curMs;
            uintptr_t levelCollections = ctx->read<uintptr_t>(framePointers.UWorld + Offsets::Get().UWorld.LevelCollections);
            if (is_valid_pointer_optimized(levelCollections)) {
                uintptr_t gameState = ctx->read<uintptr_t>(levelCollections + Offsets::Get().FLevelCollection.GameState);
                if (is_valid_pointer_optimized(gameState)) {
                    uintptr_t stageInfoBase = gameState + Offsets::Get().AGameState.StageInfo;
                    double stageBuf[2];
                    if (ctx->read_array((uintptr_t)(stageInfoBase + Offsets::Get().StageInfo.TimeLeft), stageBuf, sizeof(stageBuf)))
                        s_cachedMatchTime = stageBuf[0] - stageBuf[1];
                }
            }
        }
        matchTimeLeftSec = s_cachedMatchTime;
    }

    // Frame identifier for the new diagnostics below - a real per-frame counter,
    // distinct from the GetTickCount64() wall-clock deltas used to gate log rate.
    static uint64_t s_diagFrameTick = 0;
    ++s_diagFrameTick;

    // [CamDiag]: one-shot-per-second snapshot of the camera state and the derived
    // projection basis, immediately before any per-entity W2S calls. NOTE: this
    // codebase does not read a view/projection matrix from the target process at
    // all - ViewContext::Build() (SDK.hpp) reconstructs an equivalent basis locally
    // from cameraLocation/cameraRotation/cameraFOV (see Task 1 report). There is no
    // "viewMat"/"projMat" object to dump, so this substitutes the closest real
    // equivalents: vc.right/vc.origin stand in for the requested viewMat[0]/[3]
    // rows, and vc.invTanFovAspect/vc.invTanFov stand in for projMat[0][0]/[1][1]
    // (they are literally the perspective scale terms this codebase uses in place
    // of a projection matrix).
    if (debugConsoleEnabled) {
        static uint64_t s_camDiagLastMs = 0;
        uint64_t nowCamDiagMs = GetTickCount64();
        if ((nowCamDiagMs - s_camDiagLastMs) >= 1000) {
            s_camDiagLastMs = nowCamDiagMs;
            char cline[384];
            snprintf(cline, sizeof(cline),
                "[CamDiag] tick=%llu camPos=(%.1f,%.1f,%.1f) camRot=(%.2f,%.2f,%.2f) fov=%.2f "
                "viewRight=(%.4f,%.4f,%.4f,0) viewOrigin=(%.1f,%.1f,%.1f,1) "
                "projScale00=%.5f projScale11=%.5f",
                (unsigned long long)s_diagFrameTick,
                cameraLocation.x, cameraLocation.y, cameraLocation.z,
                cameraRotation.Pitch, cameraRotation.Yaw, cameraRotation.Roll,
                cameraFOV,
                vc.right.x, vc.right.y, vc.right.z,
                vc.origin.x, vc.origin.y, vc.origin.z,
                vc.invTanFovAspect, vc.invTanFov);
            DebugSystem::LogConsole(cline);
        }
    }

    // [CamTear] (instrumentation only, no fix, no gating of rendering): independent
    // probe using two back-to-back SEPARATE field reads of the same POV block
    // GetCameraInfo() reads. GetCameraInfo's primary path is a single contiguous
    // read_array blob (Task 1 case A) with a fallback to 3 separate ctx->read<>()
    // calls only if that blob read fails (Task 1 case B) - the fallback is real,
    // reachable code, not dead code, and is exactly what the read-failure bursts
    // this bug report suspects (EAC scan interference) would trigger more often.
    // A blob read is also not provably atomic with respect to the target process's
    // own concurrent writes to that memory, so this checks for tearing regardless
    // of which path produced this frame's real camera data, without modifying
    // GetCameraInfo or gating anything on the result - log only.
    if (debugConsoleEnabled && is_valid_pointer_optimized(framePointers.PlayerCameraManager)) {
        uintptr_t ctViewTarget = framePointers.PlayerCameraManager + Offsets::Get().CameraManager.ViewTarget;
        uintptr_t ctPov = ctViewTarget + Offsets::Get().FTViewTarget.POV;
        const auto& ctOff = Offsets::Get().FMinimalViewInfo;

        Vector3 ctLoc1 = ctx->read<Vector3>(ctPov + ctOff.Location);
        FRotator ctRot1 = ctx->read<FRotator>(ctPov + ctOff.Rotation);
        float ctFov1 = ctx->read<float>(ctPov + ctOff.FOV);
        Vector3 ctLoc2 = ctx->read<Vector3>(ctPov + ctOff.Location);
        FRotator ctRot2 = ctx->read<FRotator>(ctPov + ctOff.Rotation);
        float ctFov2 = ctx->read<float>(ctPov + ctOff.FOV);

        static uint64_t s_camTearWindowStart = 0;
        static int s_camTearLoggedThisWindow = 0;
        uint64_t nowTearMs = GetTickCount64();
        if ((nowTearMs - s_camTearWindowStart) > 1000) {
            s_camTearWindowStart = nowTearMs;
            s_camTearLoggedThisWindow = 0;
        }

        auto checkTearField = [&](const char* fieldName, double v1, double v2, double epsilon) {
            if (s_camTearLoggedThisWindow >= 10) return;
            if (!std::isfinite(v1) || !std::isfinite(v2)) return;
            double delta = std::fabs(v1 - v2);
            if (delta > epsilon) {
                ++s_camTearLoggedThisWindow;
                char tline[192];
                snprintf(tline, sizeof(tline),
                    "[CamTear] tick=%llu field=%s read1=%.4f read2=%.4f delta=%.4f",
                    (unsigned long long)s_diagFrameTick, fieldName, v1, v2, delta);
                DebugSystem::LogConsole(tline);
            }
        };
        // These used to fire at 0.01uu / 0.01deg, which logged ordinary mouse
        // motion between the six sequential field reads as a "tear" (~700 lines
        // a raid). Keep the probe, but only report disagreements that cannot be
        // camera motion: a zeroed axis vs a live one, or a several-degree hop.
        checkTearField(XC("locX"), ctLoc1.x, ctLoc2.x, 50.0);
        checkTearField(XC("locY"), ctLoc1.y, ctLoc2.y, 50.0);
        checkTearField(XC("locZ"), ctLoc1.z, ctLoc2.z, 50.0);
        checkTearField(XC("pitch"), ctRot1.Pitch, ctRot2.Pitch, 5.0);
        checkTearField(XC("yaw"), ctRot1.Yaw, ctRot2.Yaw, 5.0);
        checkTearField(XC("roll"), ctRot1.Roll, ctRot2.Roll, 5.0);
        checkTearField(XC("fov"), (double)ctFov1, (double)ctFov2, 1.0);
    }

    for (const auto& pd : localCache)
    {
        try
        {
        if (pd.stale)
            continue;
        cacheIterationCount++;
        int fontIdx = std::clamp(fontIndex, 0, (int)f.custom.size() - 1);
        ImFont* textFont = f.custom[fontIdx];
        if (!textFont)
        {
            skippedNoFont++;
            continue;
        }

        if (Cfg::Scene::BoundsESP && !combatModeActive &&
            (pd.loot || pd.container || pd.salvage || pd.raidercache) &&
            pd.entity != 0 && is_valid_pointer_optimized(pd.entity))
        {
            float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;
            float maxBoundsDist = 200.f;
            if (pd.loot) maxBoundsDist = Cfg::Scene::MaxDistanceItems;
            else if (pd.container) maxBoundsDist = Cfg::Scene::MaxDistanceContainers;
            else if (pd.salvage) maxBoundsDist = Cfg::Scene::MaxDistanceSalvage;
            else if (pd.raidercache) maxBoundsDist = Cfg::Scene::MaxDistanceRaiderCache;

            if (distMeters <= maxBoundsDist)
            {
                const auto& off = Offsets::Get();
                uintptr_t root = ctx->read<uintptr_t>(pd.entity + off.UObject.RootComponent);
                if (is_valid_pointer_optimized(root))
                {
                    uintptr_t meshComp = root;
                    uintptr_t sm = ctx->read<uintptr_t>(meshComp + off.UStaticMeshComponent.StaticMesh);

                    if (!is_valid_pointer_optimized(sm)) {
                        TArray childArr;
                        if (ctx->read_array(root + off.USceneComponent.AttachChildren, &childArr, sizeof(TArray)) &&
                            is_valid_pointer_optimized(childArr.Data) && childArr.Count > 0 && childArr.Count <= 32)
                        {
                            int cnt = childArr.Count;
                            std::vector<uint64_t> kids(cnt);
                            if (ctx->read_array(childArr.Data, kids.data(), cnt * sizeof(uint64_t))) {
                                for (int ci = 0; ci < cnt; ci++) {
                                    if (!is_valid_pointer_optimized(kids[ci])) continue;
                                    uintptr_t childSm = ctx->read<uintptr_t>(kids[ci] + off.UStaticMeshComponent.StaticMesh);
                                    if (is_valid_pointer_optimized(childSm)) {
                                        meshComp = kids[ci];
                                        sm = childSm;
                                        break;
                                    }
                                }
                            }
                        }
                    }

                    if (is_valid_pointer_optimized(sm))
                    {
                        FTransform c2w = ctx->read<FTransform>(meshComp + off.USceneComponent.ComponentToWorld);
                        if (std::isfinite(c2w.translation.x) && std::isfinite(c2w.translation.y) && std::isfinite(c2w.translation.z))
                        {
                            if (c2w.scale.x == 0 && c2w.scale.y == 0 && c2w.scale.z == 0)
                                c2w.scale = Vector3(1, 1, 1);

                            const uintptr_t BOUNDS_OFF = off.UStaticMesh.ExtendedBounds;
                            Vector3 origin, extent;
                            if (ctx->read_array(sm + BOUNDS_OFF, &origin, sizeof(Vector3)) &&
                                ctx->read_array(sm + BOUNDS_OFF + sizeof(Vector3), &extent, sizeof(Vector3)) &&
                                std::isfinite(extent.x) && std::isfinite(extent.y) && std::isfinite(extent.z) &&
                                (extent.x > 1.0 || extent.y > 1.0 || extent.z > 1.0) &&
                                extent.x < 50000.0 && extent.y < 50000.0 && extent.z < 50000.0)
                            {
                                double sX = fabs(c2w.scale.x); if (sX < 0.001) sX = 1.0;
                                double sY = fabs(c2w.scale.y); if (sY < 0.001) sY = 1.0;
                                double sZ = fabs(c2w.scale.z); if (sZ < 0.001) sZ = 1.0;
                                double sx = extent.x * sX;
                                double sy = extent.y * sY;
                                double sz = extent.z * sZ;

                                Vector3 scaledO(origin.x * c2w.scale.x, origin.y * c2w.scale.y, origin.z * c2w.scale.z);
                                Vector3 rotO = QuatRotate(scaledO, c2w.rot);
                                Vector3 wC(c2w.translation.x + rotO.x, c2w.translation.y + rotO.y, c2w.translation.z + rotO.z);

                                Vector3 corners[8] = {
                                    Vector3(-sx,-sy,-sz), Vector3( sx,-sy,-sz),
                                    Vector3( sx, sy,-sz), Vector3(-sx, sy,-sz),
                                    Vector3(-sx,-sy, sz), Vector3( sx,-sy, sz),
                                    Vector3( sx, sy, sz), Vector3(-sx, sy, sz)
                                };
                                Vector3 wCorners[8];
                                for (int ci = 0; ci < 8; ci++) {
                                    Vector3 r = QuatRotate(corners[ci], c2w.rot);
                                    wCorners[ci] = Vector3(wC.x + r.x, wC.y + r.y, wC.z + r.z);
                                }

                                ImVec2 sc[8];
                                int vis = 0;
                                for (int ci = 0; ci < 8; ci++) {
                                    Vector2 s;
                                    if (vc.Project(wCorners[ci], &s)) {
                                        sc[ci] = ImVec2((float)s.x, (float)s.y);
                                        vis++;
                                    } else {
                                        sc[ci] = ImVec2(-9999, -9999);
                                    }
                                }

                                if (vis >= 2) {
                                    ImU32 bCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
                                        Cfg::Scene::BoundsColor[0],
                                        Cfg::Scene::BoundsColor[1],
                                        Cfg::Scene::BoundsColor[2],
                                        Cfg::Scene::BoundsColor[3]));
                                    auto edge = [&](int a, int b) {
                                        if (sc[a].x > -9000 && sc[b].x > -9000)
                                            dl->AddLine(sc[a], sc[b], bCol, 1.0f);
                                    };
                                    edge(0,1); edge(1,2); edge(2,3); edge(3,0);
                                    edge(4,5); edge(5,6); edge(6,7); edge(7,4);
                                    edge(0,4); edge(1,5); edge(2,6); edge(3,7);
                                }
                            }
                        }
                    }
                }
            }
        }

        if (pd.corpse && Cfg::Scene::CorpsesESP && !combatModeActive)
            {
                processedEntities++;
                float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;
                if (distMeters > Cfg::Scene::MaxDistanceCorpses)
                    continue;

                Vector2 corpseScreen{};
                if (!vc.Project(pd.worldLoc, &corpseScreen))
                {
                    continue;
                }

                ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                if (!textFont)
                    continue;

                float size = textFont->FontSize;

                ImU32 colCorpse = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(
                        Cfg::Scene::CorpsesColor[0],
                        Cfg::Scene::CorpsesColor[1],
                        Cfg::Scene::CorpsesColor[2],
                        Cfg::Scene::CorpsesColor[3]
                    )
                );

                char line1[64];
                if (Cfg::Scene::WorldNameWithDistance)
                    snprintf(line1, sizeof(line1), XC("Player Corpse [%dm]"), (int)distMeters);
                else
                    snprintf(line1, sizeof(line1), XC("Player Corpse"));
                ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, line1);

                float x1 = corpseScreen.x - sz1.x * 0.5f;
                float y1 = corpseScreen.y - sz1.y - 12.f;

                dl->AddText(textFont, size, { x1 + 1, y1 + 1 }, IM_COL32(0, 0, 0, 160), line1);
                dl->AddText(textFont, size, { x1,     y1 }, colCorpse, line1);

                float circleY = y1 + sz1.y + 6.f;
                if (!Cfg::Scene::WorldNameWithDistance)
                {
                    char distStr[32];
                    snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)distMeters);

                    ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                    float x2 = corpseScreen.x - sz2.x * 0.5f;
                    float y2 = y1 + sz1.y + 2.f;

                    dl->AddText(textFont, size, { x2 + 1, y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                    dl->AddText(textFont, size, { x2,     y2 }, IM_COL32(180, 255, 140, 255), distStr);
                    circleY = y2 + sz2.y + 6.f;
                }

                if (Cfg::Scene::WorldShowCircle)
                {
                    float circleR = 3.3f;
                    dl->AddCircleFilled(ImVec2(corpseScreen.x, circleY), circleR, colCorpse);
                    dl->AddCircle(ImVec2(corpseScreen.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                }


                continue;
            }

            if (Cfg::Scene::GrenadeESP && pd.grenade && !combatModeActive)
            {
                if (!ThreatTypeSelected(Cfg::Scene::GrenadeTypeMask, pd.threatBit))
                    continue;

                float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;
                if (distMeters > Cfg::Scene::MaxDistanceGrenade)
                    continue;

                Vector2 screen{};
                if (!vc.Project(pd.worldLoc, &screen))
                    continue;


                ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                if (!textFont) continue;


                float size = textFont->FontSize;

                ImU32 col = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(Cfg::Scene::GrenadeColor[0],
                        Cfg::Scene::GrenadeColor[1],
                        Cfg::Scene::GrenadeColor[2],
                        Cfg::Scene::GrenadeColor[3]));

                const char* name = pd.arcDisplayName.empty() ? XC("Grenade") : pd.arcDisplayName.c_str();
                ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, name);
                float x1 = screen.x - sz1.x * 0.5f;
                float y1 = screen.y - sz1.y - 12.f;
                dl->AddText(textFont, size, { x1 + 1,y1 + 1 }, IM_COL32(0, 0, 0, 160), name);
                dl->AddText(textFont, size, { x1,y1 }, col, name);

                char distStr[32];
                snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)distMeters);
                ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                float x2 = screen.x - sz2.x * 0.5f;
                float y2 = y1 + sz1.y + 2.f;
                dl->AddText(textFont, size, { x2 + 1,y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                dl->AddText(textFont, size, { x2,y2 }, IM_COL32(255, 255, 255, 255), distStr);



                if (distMeters <= 10.f)
                {
                    const char* warning = XC("!!! WARNING GRENADE !!!");
                    ImVec2 szWarn = textFont->CalcTextSizeA(size * 1.6f, FLT_MAX, 0, warning);
                    float xWarn = Width * 0.5f - szWarn.x * 0.5f;
                    float yWarn = Height - szWarn.y - 80.f;
                    dl->AddText(textFont, size * 1.6f, { xWarn + 2,yWarn + 2 }, IM_COL32(0, 0, 0, 220), warning);
                    dl->AddText(textFont, size * 1.6f, { xWarn,yWarn }, col, warning);
                }
                continue;
            }

            if (Cfg::Scene::MinesESP && pd.mine && !combatModeActive)
            {
                if (!ThreatTypeSelected(Cfg::Scene::MineTypeMask, pd.threatBit))
                    continue;

                float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;
                if (distMeters > Cfg::Scene::MaxDistanceMines)
                    continue;

                Vector2 screen{};
                if (!vc.Project(pd.worldLoc, &screen))
                    continue;

                ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                if (!textFont) continue;


                float size = textFont->FontSize;

                ImU32 col = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(Cfg::Scene::MinesColor[0],
                        Cfg::Scene::MinesColor[1],
                        Cfg::Scene::MinesColor[2],
                        Cfg::Scene::MinesColor[3]));

                const char* name = pd.arcDisplayName.empty() ? XC("Mine") : pd.arcDisplayName.c_str();
                ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, name);
                float x1 = screen.x - sz1.x * 0.5f;
                float y1 = screen.y - sz1.y - 12.f;
                dl->AddText(textFont, size, { x1 + 1,y1 + 1 }, IM_COL32(0, 0, 0, 160), name);
                dl->AddText(textFont, size, { x1,y1 }, col, name);

                char distStr[32];
                snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)distMeters);
                ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                float x2 = screen.x - sz2.x * 0.5f;
                float y2 = y1 + sz1.y + 2.f;
                dl->AddText(textFont, size, { x2 + 1,y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                dl->AddText(textFont, size, { x2,y2 }, IM_COL32(255, 255, 255, 255), distStr);



                if (distMeters <= 10.f)
                {
                    const char* warning = XC("!!! WARNING MINE !!!");
                    ImVec2 szWarn = textFont->CalcTextSizeA(size * 1.6f, FLT_MAX, 0, warning);
                    float xWarn = Width * 0.5f - szWarn.x * 0.5f;
                    float yWarn = Height - szWarn.y - 80.f;
                    dl->AddText(textFont, size * 1.6f, { xWarn + 2,yWarn + 2 }, IM_COL32(0, 0, 0, 220), warning);
                    dl->AddText(textFont, size * 1.6f, { xWarn,yWarn }, col, warning);
                }
                continue;
            }

            if (Cfg::Scene::TrapsESP && pd.trap && !combatModeActive)
            {
                if (!ThreatTypeSelected(Cfg::Scene::TrapTypeMask, pd.threatBit))
                    continue;

                float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;
                if (distMeters > Cfg::Scene::MaxDistanceTraps)
                    continue;

                Vector2 screen{};
                if (!vc.Project(pd.worldLoc, &screen))
                    continue;

                ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                if (!textFont) continue;


                float size = textFont->FontSize;

                ImU32 col = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(Cfg::Scene::TrapsColor[0],
                        Cfg::Scene::TrapsColor[1],
                        Cfg::Scene::TrapsColor[2],
                        Cfg::Scene::TrapsColor[3]));

                const char* name = pd.arcDisplayName.empty() ? XC("Trap") : pd.arcDisplayName.c_str();
                ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, name);
                float x1 = screen.x - sz1.x * 0.5f;
                float y1 = screen.y - sz1.y - 12.f;
                dl->AddText(textFont, size, { x1 + 1,y1 + 1 }, IM_COL32(0, 0, 0, 160), name);
                dl->AddText(textFont, size, { x1,y1 }, col, name);

                char distStr[32];
                snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)distMeters);
                ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                float x2 = screen.x - sz2.x * 0.5f;
                float y2 = y1 + sz1.y + 2.f;
                dl->AddText(textFont, size, { x2 + 1,y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                dl->AddText(textFont, size, { x2,y2 }, IM_COL32(255, 255, 255, 255), distStr);



                if (distMeters <= 10.f)
                {
                    const char* warning = XC("!!! WARNING TRAP !!!");
                    ImVec2 szWarn = textFont->CalcTextSizeA(size * 1.6f, FLT_MAX, 0, warning);
                    float xWarn = Width * 0.5f - szWarn.x * 0.5f;
                    float yWarn = Height - szWarn.y - 80.f;
                    dl->AddText(textFont, size * 1.6f, { xWarn + 2,yWarn + 2 }, IM_COL32(0, 0, 0, 220), warning);
                    dl->AddText(textFont, size * 1.6f, { xWarn,yWarn }, col, warning);
                }
                continue;
            }

            if (pd.raidercache && Cfg::Scene::RaiderCacheESP && !combatModeActive)
            {
                float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;
                if (distMeters > Cfg::Scene::MaxDistanceRaiderCache)
                    continue;

                Vector2 rcScreen{};
                if (!vc.Project(pd.worldLoc, &rcScreen))
                    continue;

                ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                if (!textFont)
                    continue;



                float size = textFont->FontSize;

                ImU32 colCache = IM_COL32(255, 200, 90, 255);

                char name[64];
                if (Cfg::Scene::WorldNameWithDistance)
                    snprintf(name, sizeof(name), XC("Raider Cache [%dm]"), (int)distMeters);
                else
                    snprintf(name, sizeof(name), XC("Raider Cache"));
                ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, name);

                float x1 = rcScreen.x - sz1.x * 0.5f;
                float y1 = rcScreen.y - sz1.y - 12.f;

                dl->AddText(textFont, size, { x1 + 1, y1 + 1 }, IM_COL32(0, 0, 0, 160), name);
                dl->AddText(textFont, size, { x1,     y1 }, colCache, name);

                float y2 = y1 + sz1.y + 2.f;
                if (!Cfg::Scene::WorldNameWithDistance)
                {
                    char distStr[32];
                    snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)distMeters);

                    ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                    float x2 = rcScreen.x - sz2.x * 0.5f;

                    dl->AddText(textFont, size, { x2 + 1, y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                    dl->AddText(textFont, size, { x2,     y2 }, IM_COL32(180, 255, 140, 255), distStr);
                    y2 += sz2.y + 2.f;
                }

                const char* state = pd.searched ? XC("Opened") : XC("Closed");
                ImU32 colState = pd.searched ? IM_COL32(255, 120, 120, 255) : IM_COL32(120, 220, 255, 255);

                ImVec2 sz3 = textFont->CalcTextSizeA(size, FLT_MAX, 0, state);
                float x3 = rcScreen.x - sz3.x * 0.5f;
                float y3 = y2;

                dl->AddText(textFont, size, { x3 + 1, y3 + 1 }, IM_COL32(0, 0, 0, 160), state);
                dl->AddText(textFont, size, { x3,     y3 }, colState, state);

                float circleY = y3 + sz3.y + 6.f;
                if (Cfg::Scene::WorldShowCircle)
                {
                    float circleR = 3.3f;
                    dl->AddCircleFilled(ImVec2(rcScreen.x, circleY), circleR, colCache);
                    dl->AddCircle(ImVec2(rcScreen.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                }


                continue;
            }

            if (pd.arcprobe)
            {
                if (Cfg::Scene::ArcProbesESP && !combatModeActive)
                {
                    float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;
                    if (distMeters <= Cfg::Scene::MaxDistanceArcProbes)
                    {
                        Vector2 apScreen{};
                        if (vc.Project(pd.worldLoc, &apScreen))
                        {
                            ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                            if (textFont)
                            {
                                float size = textFont->FontSize;
                                ImU32 colArcProbe = ImGui::ColorConvertFloat4ToU32(
                                    ImVec4(
                                        Cfg::Scene::ArcProbesColor[0],
                                        Cfg::Scene::ArcProbesColor[1],
                                        Cfg::Scene::ArcProbesColor[2],
                                        Cfg::Scene::ArcProbesColor[3]
                                    )
                                );

                                char name[64];
                                if (Cfg::Scene::WorldNameWithDistance)
                                    snprintf(name, sizeof(name), XC("Arc Probe [%dm]"), (int)distMeters);
                                else
                                    snprintf(name, sizeof(name), XC("Arc Probe"));
                                ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, name);
                                float x1 = apScreen.x - sz1.x * 0.5f;
                                float y1 = apScreen.y - sz1.y - 12.f;
                                dl->AddText(textFont, size, { x1 + 1, y1 + 1 }, IM_COL32(0, 0, 0, 160), name);
                                dl->AddText(textFont, size, { x1, y1 }, colArcProbe, name);

                                float circleY = y1 + sz1.y + 6.f;
                                if (!Cfg::Scene::WorldNameWithDistance)
                                {
                                    char distStr[32];
                                    snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)distMeters);
                                    ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                                    float x2 = apScreen.x - sz2.x * 0.5f;
                                    float y2 = y1 + sz1.y + 2.f;
                                    dl->AddText(textFont, size, { x2 + 1, y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                                    dl->AddText(textFont, size, { x2, y2 }, IM_COL32(180, 255, 140, 255), distStr);
                                    circleY = y2 + sz2.y + 6.f;
                                }

                                if (Cfg::Scene::WorldShowCircle)
                                {
                                    float circleR = 3.3f;
                                    dl->AddCircleFilled(ImVec2(apScreen.x, circleY), circleR, colArcProbe);
                                    dl->AddCircle(ImVec2(apScreen.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                                }
                            }
                        }
                    }
                }
                continue;
            }

            if (pd.salvage && Cfg::Scene::SalvageESP && !combatModeActive)
            {
                auto* info = SceneTypes::Salvage::Get(pd.ContainerName);
                if (info && !Cfg::Scene::SalvageTypeFilterMulti[info->typeIndex])
                    continue;

                float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;
                if (distMeters > Cfg::Scene::MaxDistanceSalvage)
                    continue;

                Vector2 salvScreen{};
                if (vc.Project(pd.worldLoc, &salvScreen))
                {
                    ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                    if (!textFont)
                        continue;

                    float size = textFont->FontSize;
                    ImU32 colSalvage = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(
                            Cfg::Scene::SalvageColor[0],
                            Cfg::Scene::SalvageColor[1],
                            Cfg::Scene::SalvageColor[2],
                            Cfg::Scene::SalvageColor[3]
                        )
                    );

                    const char* salvName = info ? info->displayName : XC("Salvage");

                    if (Cfg::Scene::FovStacking)
                    {
                        worldStackItems.push_back(WorldStackItem{
                            WorldStackType::Salvage,
                            salvScreen,
                            pd.worldLoc,
                            distMeters,
                            colSalvage,
                            salvName
                        });
                        continue;
                    }



                    char salvLine[96];
                    if (Cfg::Scene::WorldNameWithDistance)
                        snprintf(salvLine, sizeof(salvLine), XC("%s [%dm]"), salvName, (int)distMeters);
                    else
                        snprintf(salvLine, sizeof(salvLine), XC("%s"), salvName);
                    ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, salvLine);
                    float x1 = salvScreen.x - sz1.x * 0.5f;
                    float y1 = salvScreen.y - sz1.y - 12.f;
                    dl->AddText(textFont, size, { x1 + 1, y1 + 1 }, IM_COL32(0, 0, 0, 160), salvLine);
                    dl->AddText(textFont, size, { x1, y1 }, colSalvage, salvLine);

                    float circleY = y1 + sz1.y + 6.f;
                    if (!Cfg::Scene::WorldNameWithDistance)
                    {
                        char distStr[32];
                        snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)distMeters);
                        ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                        float x2 = salvScreen.x - sz2.x * 0.5f;
                        float y2 = y1 + sz1.y + 2.f;
                        dl->AddText(textFont, size, { x2 + 1, y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                        dl->AddText(textFont, size, { x2, y2 }, IM_COL32(180, 255, 140, 255), distStr);
                        circleY = y2 + sz2.y + 6.f;
                    }

                    if (Cfg::Scene::WorldShowCircle)
                    {
                        float circleR = 3.3f;
                        dl->AddCircleFilled(ImVec2(salvScreen.x, circleY), circleR, colSalvage);
                        dl->AddCircle(ImVec2(salvScreen.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                    }


                }
                continue;
            }

            if (pd.extraction && Cfg::Scene::ExtractionsESP && !combatModeActive)
            {

                if (pd.lootCatId < 9 && !Cfg::Scene::ExtractionTypeFilterMulti[pd.lootCatId])
                    continue;

                float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;

                Vector2 extScreen{};
                if (vc.Project(pd.worldLoc, &extScreen))
                {
                    ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                    if (!textFont)
                        continue;


                    float size = textFont->FontSize;
                    ImU32 colExtraction = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(
                            Cfg::Scene::ExtractionsColor[0],
                            Cfg::Scene::ExtractionsColor[1],
                            Cfg::Scene::ExtractionsColor[2],
                            Cfg::Scene::ExtractionsColor[3]
                        )
                    );

                    const char* extName = pd.extractioncat.empty() ? XC("Extraction") : pd.extractioncat.c_str();
                    char extLine[96];
                    if (Cfg::Scene::WorldNameWithDistance)
                        snprintf(extLine, sizeof(extLine), XC("%s [%dm]"), extName, (int)distMeters);
                    else
                        snprintf(extLine, sizeof(extLine), XC("%s"), extName);

                    ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, extLine);
                    float x1 = extScreen.x - sz1.x * 0.5f;
                    float y1 = extScreen.y - sz1.y - 12.f;
                    dl->AddText(textFont, size, { x1 + 1, y1 + 1 }, IM_COL32(0, 0, 0, 160), extLine);
                    dl->AddText(textFont, size, { x1, y1 }, colExtraction, extLine);

                    float y2 = y1 + sz1.y + 2.f;
                    if (!Cfg::Scene::WorldNameWithDistance)
                    {
                        char distStr[32];
                        snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)distMeters);
                        ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                        float x2 = extScreen.x - sz2.x * 0.5f;
                        dl->AddText(textFont, size, { x2 + 1, y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                        dl->AddText(textFont, size, { x2, y2 }, IM_COL32(180, 255, 140, 255), distStr);
                        y2 += sz2.y + 2.f;
                    }


                    const char* stateStr = XC("Unknown");
                    ImU32 colState = IM_COL32(128, 128, 128, 255);
                    uint8_t state = pd.extractionState;
                    switch (state)
                    {
                    case 0: stateStr = XC("Dormant"); colState = IM_COL32(100, 100, 100, 255); break;
                    case 1: stateStr = XC("Disabled"); colState = IM_COL32(200, 50, 50, 255); break;
                    case 2: stateStr = XC("Closed"); colState = IM_COL32(150, 150, 200, 255); break;
                    case 3: stateStr = XC("Closing"); colState = IM_COL32(200, 150, 50, 255); break;
                    case 4: stateStr = XC("Requested"); colState = IM_COL32(200, 200, 50, 255); break;
                    case 5: stateStr = XC("Arriving"); colState = IM_COL32(50, 200, 200, 255); break;
                    case 6: stateStr = XC("Ready"); colState = IM_COL32(50, 255, 50, 255); break;
                    default: stateStr = XC("Unknown"); colState = IM_COL32(128, 128, 128, 255); break;
                    }

                    ImVec2 sz3 = textFont->CalcTextSizeA(size, FLT_MAX, 0, stateStr);
                    float x3 = extScreen.x - sz3.x * 0.5f;
                    float y3 = y2;
                    dl->AddText(textFont, size, { x3 + 1, y3 + 1 }, IM_COL32(0, 0, 0, 160), stateStr);
                    dl->AddText(textFont, size, { x3, y3 }, colState, stateStr);

                    float y4 = y3 + sz3.y + 2.f;
                    if (Cfg::Scene::ExtractionTimeLeft && matchTimeLeftSec >= 0.0 && pd.entity != 0 && is_valid_pointer_optimized(pd.entity)) {
                        double timeLeftAfterShutdown = 0.0;
                        if (ctx->read_array((uintptr_t)(pd.entity + Offsets::Get().ASalvageExtractionPointBase.TimeLeftAfterShutdown), &timeLeftAfterShutdown, sizeof(double))) {
                            double extractionSec = matchTimeLeftSec - timeLeftAfterShutdown;
                            int sec = (int)extractionSec;
                            if (sec < 0) sec = 0;
                            int min = sec / 60;
                            sec %= 60;
                            char timeLine[32];
                            snprintf(timeLine, sizeof(timeLine), XC("%d:%02d"), min, sec);
                            ImVec2 sz4 = textFont->CalcTextSizeA(size, FLT_MAX, 0, timeLine);
                            float x4 = extScreen.x - sz4.x * 0.5f;
                            dl->AddText(textFont, size, { x4 + 1, y4 + 1 }, IM_COL32(0, 0, 0, 160), timeLine);
                            dl->AddText(textFont, size, { x4, y4 }, colExtraction, timeLine);
                            y4 += sz4.y + 2.f;
                        }
                    }

                    float circleY = y4 + 4.f;
                    if (Cfg::Scene::WorldShowCircle)
                    {
                        float circleR = 3.3f;
                        dl->AddCircleFilled(ImVec2(extScreen.x, circleY), circleR, colExtraction);
                        dl->AddCircle(ImVec2(extScreen.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                    }


                }
                continue;
            }

            if (pd.container && Cfg::Scene::ContainersESP && !combatModeActive)
            {
                auto* info = SceneTypes::Container::Get(pd.ContainerName);
                if (info && !Cfg::Scene::ContainerTypeFilterMulti[info->typeIndex])
                    continue;

                float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;
                if (distMeters > Cfg::Scene::MaxDistanceContainers)
                    continue;
                if (!Cfg::Scene::ContainersShowOpened && pd.searched)
                    continue;

                Vector2 contScreen{};
                if (vc.Project(pd.worldLoc, &contScreen))
                {
                    ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                    if (!textFont)
                        continue;

                    float size = textFont->FontSize;
                    ImU32 colContainer = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(
                            Cfg::Scene::ContainersColor[0],
                            Cfg::Scene::ContainersColor[1],
                            Cfg::Scene::ContainersColor[2],
                            Cfg::Scene::ContainersColor[3]
                        )
                    );

                    const char* contName = info ? info->displayName : XC("Container");

                    if (Cfg::Scene::FovStacking)
                    {
                        worldStackItems.push_back(WorldStackItem{
                            WorldStackType::Container,
                            contScreen,
                            pd.worldLoc,
                            distMeters,
                            colContainer,
                            contName,
                            0,
                            0,
                            0.0,
                            pd.searched
                        });
                        continue;
                    }



                    char contLine[96];
                    if (Cfg::Scene::WorldNameWithDistance)
                        snprintf(contLine, sizeof(contLine), XC("%s [%dm]"), contName, (int)distMeters);
                    else
                        snprintf(contLine, sizeof(contLine), XC("%s"), contName);
                    ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, contLine);
                    float x1 = contScreen.x - sz1.x * 0.5f;
                    float y1 = contScreen.y - sz1.y - 12.f;
                    dl->AddText(textFont, size, { x1 + 1, y1 + 1 }, IM_COL32(0, 0, 0, 160), contLine);
                    dl->AddText(textFont, size, { x1, y1 }, colContainer, contLine);

                    float y2 = y1 + sz1.y + 2.f;
                    if (!Cfg::Scene::WorldNameWithDistance)
                    {
                        char distStr[32];
                        snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)distMeters);
                        ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                        float x2 = contScreen.x - sz2.x * 0.5f;
                        dl->AddText(textFont, size, { x2 + 1, y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                        dl->AddText(textFont, size, { x2, y2 }, IM_COL32(180, 255, 140, 255), distStr);
                        y2 += sz2.y + 2.f;
                    }

                    const char* state = pd.searched ? XC("Opened") : XC("Closed");
                    ImU32 colorState = pd.searched ? IM_COL32(255, 120, 120, 255) : IM_COL32(120, 220, 255, 255);
                    ImVec2 sz3 = textFont->CalcTextSizeA(size, FLT_MAX, 0, state);
                    float x3 = contScreen.x - sz3.x * 0.5f;
                    float y3 = y2;
                    dl->AddText(textFont, size, { x3 + 1, y3 + 1 }, IM_COL32(0, 0, 0, 160), state);
                    dl->AddText(textFont, size, { x3, y3 }, colorState, state);

                    float circleY = y3 + sz3.y + 6.f;
                    if (Cfg::Scene::WorldShowCircle)
                    {
                        float circleR = 3.3f;
                        dl->AddCircleFilled(ImVec2(contScreen.x, circleY), circleR, colContainer);
                        dl->AddCircle(ImVec2(contScreen.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                    }


                }
                continue;
            }


            if (pd.loot)
            {
                if (!Cfg::Scene::ItemsESP)
                {
                    continue;
                }
                if (combatModeActive)
                {
                    continue;
                }

                processedEntities++;
                float distMeters = (pd.worldLoc - cameraLocation).Length() / 100.f;

                if (distMeters > Cfg::Scene::MaxDistanceItems)
                {
                    continue;
                }

                int catId = static_cast<int>(pd.lootCatId);
                if (catId < 0 || catId >= 22)
                {
                    continue;
                }

                try {
                    if (!Cfg::Scene::ItemsCategoryFilterMulti[catId])
                        continue;
                } catch (...) {
                    continue;
                }

                if (Cfg::Scene::Filterbyprice)
                {
                    if (Cfg::Scene::PricePolicy == 0)
                    {
                        if (pd.itemvalue < Cfg::Scene::MinPrice)
                            continue;
                    }
                    else
                    {
                        if (pd.itemvalue > Cfg::Scene::MinPrice || pd.itemvalue <= 0)
                            continue;
                    }
                }

                Vector2 lootScreen{};
                if (cameraLocation.IsZero() || !vc.Project(pd.worldLoc, &lootScreen))
                    continue;

                ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                if (!textFont)
                    continue;



                float size = textFont->FontSize;

                ImU32 colItem = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(
                        Cfg::Scene::ItemsColor[0],
                        Cfg::Scene::ItemsColor[1],
                        Cfg::Scene::ItemsColor[2],
                        Cfg::Scene::ItemsColor[3]
                    )
                );

                if (Cfg::Scene::FovStacking)
                {
                    const char* lootNameStr = pd.LootName.empty() ? XC("Unknown") : pd.LootName.c_str();
                    worldStackItems.push_back(WorldStackItem{
                        WorldStackType::Loot,
                        lootScreen,
                        pd.worldLoc,
                        distMeters,
                        colItem,
                        lootNameStr,
                        pd.lootamount,
                        pd.itemvalue,
                        GetActorWeight(pd.entity),
                        false
                    });

                    continue;
                }

                float iconSz = 28.0f;
                float textStartY = lootScreen.y - 10.f;

                if (Cfg::Scene::Icon && test::Device9) {
                    ImTextureID iconTex = GetOrCreateLootIcon(test::Device9, pd.LootName);
                    if (iconTex) {
                        float ix = lootScreen.x - iconSz * 0.5f;
                        float iy = textStartY - iconSz - 4.f;
                        dl->AddImage(iconTex, ImVec2(ix, iy), ImVec2(ix + iconSz, iy + iconSz));
                    }
                }

                float nextY = textStartY;

                {
                    char nameLine[192];
                    const char* lootNameStr = pd.LootName.empty() ? XC("Unknown") : pd.LootName.c_str();
                    int off = 0;

                    if (Cfg::Scene::Amount)
                        off += snprintf(nameLine + off, sizeof(nameLine) - off, XC("x%d "), pd.lootamount);
                    if (Cfg::Scene::ShowName)
                        off += snprintf(nameLine + off, sizeof(nameLine) - off, XC("%s"), lootNameStr);
                    if (Cfg::Scene::Price && pd.itemvalue > 0)
                        off += snprintf(nameLine + off, sizeof(nameLine) - off, XC(" [%d$]"), pd.itemvalue);
                    if (Cfg::Scene::WorldNameWithDistance)
                        off += snprintf(nameLine + off, sizeof(nameLine) - off, XC(" [%dm]"), (int)distMeters);

                    if (off > 0)
                    {
                        ImVec2 szName = textFont->CalcTextSizeA(size, FLT_MAX, 0, nameLine);
                        float xName = lootScreen.x - szName.x * 0.5f;
                        dl->AddText(textFont, size, { xName + 1, nextY + 1 }, IM_COL32(0, 0, 0, 160), nameLine);
                        dl->AddText(textFont, size, { xName, nextY }, colItem, nameLine);
                        nextY += szName.y + 2.f;
                    }
                }

                double _actorW = GetActorWeight(pd.entity);
                if (Cfg::Scene::Weight && _actorW > 0.01)
                {
                    char wLine[64];
                    snprintf(wLine, sizeof(wLine), XC("%.1f kg"), _actorW);
                    ImVec2 szW = textFont->CalcTextSizeA(size, FLT_MAX, 0, wLine);
                    float xW = lootScreen.x - szW.x * 0.5f;
                    dl->AddText(textFont, size, { xW + 1, nextY + 1 }, IM_COL32(0, 0, 0, 160), wLine);
                    dl->AddText(textFont, size, { xW, nextY }, IM_COL32(200, 200, 255, 255), wLine);
                    nextY += szW.y + 2.f;
                }

                float circleY = nextY + 4.f;
                if (Cfg::Scene::WorldShowCircle)
                {
                    float circleR = 3.3f;
                    dl->AddCircleFilled(ImVec2(lootScreen.x, circleY), circleR, colItem);
                    dl->AddCircle(ImVec2(lootScreen.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                }

                continue;
            }

            if (Cfg::Scene::ARCsESP && pd.npc && !combatModeActive)
            {
                if (MatchesLocalAvatar(pd.entity, pd.playerState, localIds))
                    continue;

                Vector3 arcWorld = ApplyMotion(pd, pd.worldLoc);
                float distMeters = (arcWorld - cameraLocation).Length() / 100.f;
                if (distMeters > Cfg::Scene::MaxDistanceArcs)
                    continue;

                // Hide only when health confirms dead — IsDestroyed alone false-positives.
                const bool confirmedDead = pd.maxHealth > 0.0 && pd.currentHealth <= 0.0;
                if (confirmedDead && !Cfg::Scene::ARCsShowDestroyed)
                    continue;

                auto arcInfo = ResolveArcInfo(pd.arcname);
                const char* arcName = (!pd.arcDisplayName.empty())
                    ? pd.arcDisplayName.c_str()
                    : (arcInfo.display && arcInfo.display[0] ? arcInfo.display : XC("ARC"));

                if (arcInfo.index >= 0 && arcInfo.index < 21)
                {
                    if (!Cfg::Scene::ArcTypeFilterMulti[arcInfo.index])
                        continue;
                }

                bool arcVisible = pd.isVisible;
                if (arcVisibilityCheck)
                    arcVisible = StableVisible(pd.entity, IsVisible(pd.skeletalMesh));
                else
                    arcVisible = true;

                if (arcVisibilityCheck && !arcVisible)
                    continue;

                Vector2 npcScreen{};
                if (!vc.Project(arcWorld, &npcScreen))
                    continue;


                ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
                if (!textFont)
                    continue;


                float size = textFont->FontSize;

                ImU32 colArc = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(
                        Cfg::Scene::ARCsColor[0],
                        Cfg::Scene::ARCsColor[1],
                        Cfg::Scene::ARCsColor[2],
                        Cfg::Scene::ARCsColor[3]
                    )
                );
                if (arcVisibilityCheck && !arcVisible)
                {
                    colArc = ImGui::ColorConvertFloat4ToU32(
                        ImVec4(col_invisible[0], col_invisible[1], col_invisible[2], col_invisible[3]));
                }

                char line1[64];
                snprintf(line1, sizeof(line1), XC("%s | %dm"), arcName, (int)distMeters);
                ImVec2 t1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, line1);
                float x1 = npcScreen.x - t1.x * 0.5f;
                float y1 = npcScreen.y - t1.y - 10.f;
                dl->AddText(textFont, size, { x1 + 1, y1 + 1 }, IM_COL32(0, 0, 0, 180), line1);
                dl->AddText(textFont, size, { x1, y1 }, colArc, line1);


                const char* state = confirmedDead ? XC("Destroyed") : XC("Alive");
                ImU32 colState = confirmedDead
                    ? IM_COL32(255, 60, 60, 255)
                    : IM_COL32(60, 255, 120, 255);
                ImVec2 t2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, state);
                float x2 = npcScreen.x - t2.x * 0.5f;
                float y2 = y1 + t1.y + 2.f;
                dl->AddText(textFont, size, { x2 + 1, y2 + 1 }, IM_COL32(0, 0, 0, 180), state);
                dl->AddText(textFont, size, { x2, y2 }, colState, state);

                if (Cfg::Scene::ARCsShowHealthBar && pd.maxHealth > 0.0 && !confirmedDead)
                {
                    double pct = pd.currentHealth / pd.maxHealth;
                    if (pct < 0.0) pct = 0.0;
                    if (pct > 1.0) pct = 1.0;

                    ImU32 hpCol;
                    if (pct > 0.66)
                        hpCol = ImGui::ColorConvertFloat4ToU32(ImVec4(healthHighColor[0], healthHighColor[1], healthHighColor[2], healthHighColor[3]));
                    else if (pct > 0.33)
                        hpCol = ImGui::ColorConvertFloat4ToU32(ImVec4(healthMediumColor[0], healthMediumColor[1], healthMediumColor[2], healthMediumColor[3]));
                    else
                        hpCol = ImGui::ColorConvertFloat4ToU32(ImVec4(healthLowColor[0], healthLowColor[1], healthLowColor[2], healthLowColor[3]));

                    ImU32 hpBackCol = ImGui::ColorConvertFloat4ToU32(ImVec4(healthBackColor[0], healthBackColor[1], healthBackColor[2], healthBackColor[3]));

                    constexpr float arcBarW = 40.0f;
                    constexpr float arcBarH = 4.0f;
                    float barX = npcScreen.x - arcBarW * 0.5f;
                    float barY = y2 + t2.y + 3.0f;

                    dl->AddRectFilled({ barX, barY }, { barX + arcBarW, barY + arcBarH }, hpBackCol);
                    float fillW = arcBarW * static_cast<float>(pct);
                    dl->AddRectFilled({ barX, barY }, { barX + fillW, barY + arcBarH }, hpCol);
                    dl->AddRect({ barX, barY }, { barX + arcBarW, barY + arcBarH }, IM_COL32(0, 0, 0, 200), 0.f, 0, 1.0f);
                }

                continue;
            }

        if (pd.npc || pd.drone || pd.loot || pd.container || pd.extraction ||
            pd.corpse || pd.raidercache || pd.salvage || pd.grenade ||
            pd.mine || pd.trap || pd.arcprobe)
            continue;

        const bool isLocalPawn = MatchesLocalAvatar(pd.entity, pd.playerState, localIds);
        if (isLocalPawn) {
            dbgSelfSkip++;
            continue;
        }

        dbgRealPlayers++;
        if (pd.platform != 0) dbgPlatOk++;
        if (pd.bones_valid) dbgBonesValid++;
        if (pd.fullSkeleton) dbgFullSkelFlag++;

        int64_t tPlayerStart = doProf ? qpcNow() : 0;

        // Squad members are never drawn. is_team still gates aim selection
        // (below) and the enemy counter in WorldCache, so detection stays live.
        if (pd.is_team) {
            dbgTeamSkip++;
            continue;
        }

        // Extracted leftover pawns (mesh+capsule frozen for minutes). Must not
        // fall through to the pos-only box — that is what flashed noSkel ESP on
        // Steve03 / xBUTTHURTSx between freeze-set oscillations.
        if (NT::Store::BoneSyncer && NT::Store::BoneSyncer->IsFrozenGhost(pd.entity)) {
            dbgFrozenSkip++;
            noteSkip(pd.name, "frozenGhost");
            continue;
        }

        // No fallback to pd.worldLoc/root-component here: that path is independent of
        // bone resolution entirely and is what produced the doorway false-ESP bug
        // (rendered from a stale/unrelated location while the bone-array read for
        // that entity was failing, c2w=(0,0,0) in [BoneDiag]). Require a Head+Root
        // sample that was actually refreshed recently - no data, or stale data, means
        // no draw. Widened from 500ms to 2000ms: intermittent bone-read failures
        // (memory access timing jitter against an actively-perturbed EAC process,
        // observed as boneUnresolvedSkip=3-7/frame) were tripping the 500ms gate
        // multiple times per second and flickering real players in/out of ESP.
        // Ragdoll detection (SlowCache.hpp, ~150ms to trigger) is now the primary
        // catcher for genuinely-dead/static entities, so this gate no longer needs
        // to be tight enough to do that job too - it only needs to catch entities
        // that have gone truly silent for multiple seconds.
        const uint64_t nowBoneMs = GetTickCount64();
        const bool hasFreshBoneLoc = pd.bones_valid && pd.headRootMs != 0 &&
            (nowBoneMs - pd.headRootMs) <= 2000;

        // ...with one exception: a player whose position has proven itself.
        //
        // Requiring bones outright also blanks players who are entirely real and
        // whose capsule position is perfectly good, just not yet resolved by the
        // bone pass - which is every player for the first moments after they join,
        // and any player whose mesh the bone reads keep missing. They stayed
        // invisible for the whole raid.
        //
        // What made the doorway ghosts dangerous was not that they lacked bones, it
        // was that their position was junk: a stale or unrelated read that jumped
        // around. So the fallback is gated on the position itself holding together -
        // sampled a few times a second, each sample within a plausible travel
        // distance of the last. Garbage fails that immediately and still gets
        // nothing drawn; a real player passes it in a quarter of a second. Bone-
        // dependent visuals stay off in this mode, since the whole premise is that
        // there is no trustworthy pose.
        struct PosConfirm {
            Vector3 lastPos{};
            uint64_t lastSampleMs = 0;
            int stableCount = 0;
        };
        static std::unordered_map<uint64_t, PosConfirm> s_posConfirm;
        constexpr uint64_t kPosSampleIntervalMs = 50;
        constexpr double kPosMaxTravelPerSample = 600.0; // sprint speed with headroom
        constexpr int kPosConfirmSamples = 5;            // ~250ms of agreement

        bool posConfirmed = false;
        if (is_valid_bone_position(pd.worldLoc) && !pd.worldLoc.IsZero()) {
            if (s_posConfirm.size() > 512)
                s_posConfirm.clear();
            auto& pc = s_posConfirm[pd.entity];
            if ((nowBoneMs - pc.lastSampleMs) >= kPosSampleIntervalMs) {
                const bool firstSample = (pc.lastSampleMs == 0);
                const double moved = firstSample ? 0.0 : pc.lastPos.distance(pd.worldLoc);
                if (firstSample || moved <= kPosMaxTravelPerSample) {
                    if (pc.stableCount < kPosConfirmSamples)
                        pc.stableCount++;
                } else {
                    pc.stableCount = 0;
                }
                pc.lastPos = pd.worldLoc;
                pc.lastSampleMs = nowBoneMs;
            }
            posConfirmed = (pc.stableCount >= kPosConfirmSamples);
        } else {
            auto it = s_posConfirm.find(pd.entity);
            if (it != s_posConfirm.end())
                it->second.stableCount = 0;
        }

        bool posOnlyFallback = false;
        if (!hasFreshBoneLoc) {
            if (!posConfirmed) {
                dbgBoneUnresolvedSkip++;
                noteSkip(pd.name, "noBonesNoPos");
                continue;
            }
            posOnlyFallback = true;
            dbgPosFallbackDrawn++;
        }

        // pd.fullSkeleton can still read true off an older pass while the bone data
        // behind it has gone stale, so it cannot gate pose rendering on its own here.
        const bool fullSkeletonUsable = pd.fullSkeleton && !posOnlyFallback;

        auto rootBoneIt = pd.cachedBones.find(Root);
        const bool haveCachedRoot = rootBoneIt != pd.cachedBones.end() &&
            is_valid_bone_position(rootBoneIt->second);

        // The live root read is the freshest position anything here has: taken this
        // frame, unclamped, and the same read that feeds pd.worldLoc upstream. The
        // cached Root comes off the bone pass, which runs an order of magnitude slower,
        // so on a moving player it is always behind.
        //
        // It used to be adopted only when it landed within 250 units of the cached Root,
        // which inverted the gate's intent. It was meant to reject a bad live read; what
        // it actually rejected was a good live read that had correctly moved on from a
        // stale bone sample. Half a second of sprinting puts a player more than 250 units
        // past their last cached Root, so the correction was locked out at exactly the
        // moment it mattered and the box sat metres behind the player. Every single
        // [GhostProbe] line caught it in the act - base 300 to 530 units out while
        // worldLoc and the mesh transform agreed with each other to within a few units.
        Vector3 baseLoc{};
        bool haveBase = false;
        if (is_valid_pointer_optimized(pd.rootComponent)) {
            Vector3 live = GetSceneComponentWorldLocation(pd.rootComponent);
            if (is_valid_bone_position(live)) {
                baseLoc = live;
                haveBase = true;
            }
        }
        // The cached Root is off the bone pass, and in fallback mode that pass is exactly
        // what has gone stale - falling back to it there would reintroduce the stale
        // position the confirmation gate exists to keep out. pd.worldLoc is safe by
        // comparison: WorldCache drops an entity outright rather than publish a position
        // it could not read this tick, so it is always a live sample.
        if (!haveBase && haveCachedRoot && !posOnlyFallback) {
            baseLoc = rootBoneIt->second;
            haveBase = true;
        }
        if (!haveBase)
            baseLoc = pd.worldLoc;

        // Carrying the cached pose forward onto the live position. The bones and the
        // cached Root come out of the same pass, so the gap between that Root and the
        // live capsule is exactly how far the whole skeleton has fallen behind, and
        // adding it back keeps the limbs on the body instead of trailing the box.
        //
        // Horizontal only. The capsule-centre-to-foot distance changes with stance, so a
        // Z term here would lift a crouching player off the ground. Clamped because a
        // cached Root that is stale for some other reason must not be able to fling the
        // skeleton across the map - past that the pose is better left where it is.
        Vector3 boneShift{};
        if (haveCachedRoot) {
            constexpr double kMaxBoneShift = 600.0;
            const double sx = baseLoc.x - rootBoneIt->second.x;
            const double sy = baseLoc.y - rootBoneIt->second.y;
            if ((sx * sx + sy * sy) <= (kMaxBoneShift * kMaxBoneShift)) {
                boneShift.x = (float)sx;
                boneShift.y = (float)sy;
            }
        }
        auto boneAt = [&](Bones bone) -> Vector3 {
            Vector3 w = GetCachedBone(pd, bone);
            if (w.IsZero())
                return w;
            w.x += boneShift.x;
            w.y += boneShift.y;
            return w;
        };

        if (baseLoc.IsZero() || !is_valid_bone_position(baseLoc)) {
            dbgOtherSkip++;
            dbgSkipBaseLoc++;
            noteSkip(pd.name, "badBaseLoc");
            continue;
        }

        // The box is drawn from baseLoc, but the ghost filter, the health read, the
        // ragdoll detector and every skeleton coherence check reason about pd.worldLoc.
        // When those two disagree by more than a body length, one of them is stale and
        // the box is being painted somewhere the rest of the pipeline does not believe
        // in. EVO-lotion held that disagreement at 66m for a whole match: [MeshStatic]
        // put its worldLoc within 60cm of a mesh frozen at (121765,528718) while
        // [W2SDiag] had the render path tracking a capsule walking through (127740,
        // 527479). Printing both sides with the mesh transform is what says which one
        // is lying - the live root read is unclamped, so if baseLoc is the honest one
        // then worldLoc is still being pinned somewhere the recovery does not reach.
        if (debugConsoleEnabled && is_valid_bone_position(pd.worldLoc) &&
            baseLoc.distance(pd.worldLoc) > 300.0)
        {
            static std::unordered_map<uint64_t, uint64_t> s_ghostProbeLogMs;
            if (s_ghostProbeLogMs.size() > 256)
                s_ghostProbeLogMs.clear();
            uint64_t& lastLog = s_ghostProbeLogMs[pd.entity];
            const uint64_t nowProbe = GetTickCount64();
            if ((nowProbe - lastLog) > 3000) {
                lastLog = nowProbe;
                Vector3 meshC2w = GetSceneComponentWorldLocation(pd.skeletalMesh);
                char gline[320];
                snprintf(gline, sizeof(gline),
                    "[GhostProbe] name=%s base=(%.0f,%.0f,%.0f) worldLoc=(%.0f,%.0f,%.0f) "
                    "meshC2w=(%.0f,%.0f,%.0f) gap=%.0f fullSkel=%d",
                    pd.name.empty() ? "<empty>" : pd.name.c_str(),
                    baseLoc.x, baseLoc.y, baseLoc.z,
                    pd.worldLoc.x, pd.worldLoc.y, pd.worldLoc.z,
                    meshC2w.x, meshC2w.y, meshC2w.z,
                    baseLoc.distance(pd.worldLoc), pd.fullSkeleton ? 1 : 0);
                DebugSystem::LogConsole(gline);
            }
        }

        float distMeters = (baseLoc - cameraLocation).Length() / 100.f;
        if (distance_vis < kEspDistanceUnlimited && distMeters > distance_vis) {
            dbgOtherSkip++;
            dbgSkipDistance++;
            noteSkip(pd.name, "dist");
            continue;
        }

        const bool isKnocked = pd.isDBNO || (pd.maxHealth > 0.0 && pd.currentHealth <= 0.0);
        bool knockedStandingBones = false;

        Vector3 feetPos = baseLoc;
        Vector3 headPos = baseLoc;
        headPos.z += isKnocked ? 40.0f : 90.0f;

        Vector2 headScreen{}, feetScreen{};
        if (!vc.Project(headPos, &headScreen) || !vc.Project(feetPos, &feetScreen)) {
            dbgW2sFail++;

            // Buffered unconditionally here (cheap: only runs on an already-failed
            // entity), flushed after the loop only if this frame's total w2sFail
            // reaches the flicker threshold - see the flush site below the loop.
            Vector2 diagProjected{};
            ViewContext::ProjectResult diagResult = vc.ProjectDiag(headPos, &diagProjected);
            if (diagResult == ViewContext::ProjectResult::Ok)
                diagResult = vc.ProjectDiag(feetPos, &diagProjected);
            Vector3 diagDelta(headPos.x - vc.origin.x, headPos.y - vc.origin.y, headPos.z - vc.origin.z);
            float diagDepth = (float)(diagDelta.x * vc.forward.x + diagDelta.y * vc.forward.y + diagDelta.z * vc.forward.z);
            w2sDiagBuffer.push_back(W2SDiagEntry{
                pd.name.empty() ? std::string("<empty>") : pd.name,
                headPos,
                distMeters,
                diagProjected,
                diagDepth,
                diagResult == ViewContext::ProjectResult::Ok,
                ProjectResultName(diagResult)
            });
            noteSkip(pd.name, ProjectResultName(diagResult));

            continue;
        }

        // Gated before the AABB work below, not after it. An entity roughly 90 degrees
        // off the view axis still passes Project() with a near-zero depth, so its corners
        // project to five-figure coordinates and trip the absurd-extent guard - which made
        // boxExtent absorb skips that belong to this counter and left [BoxDiag] reporting
        // the box of something far off the side of the screen instead of the smallest
        // visible one. Checking X first attributes the skip correctly and skips eight
        // pointless projections.
        //
        // Both endpoints have to be off the same side before the entity is dropped.
        // Testing the head alone discarded players whose head projected wide while
        // their body was still on screen, and the old fixed 200px margin was tight
        // enough at 1080p that an enemy at the very edge of view could fall outside
        // it. A margin of one screen width keeps the absurd-projection case (a target
        // near 90 degrees off-axis lands in the tens of thousands) while leaving
        // anything remotely on screen alone.
        const float offScreenMargin = (float)Width;
        const bool headOffLeft = headScreen.x < -offScreenMargin;
        const bool feetOffLeft = feetScreen.x < -offScreenMargin;
        const bool headOffRight = headScreen.x > Width + offScreenMargin;
        const bool feetOffRight = feetScreen.x > Width + offScreenMargin;
        if ((headOffLeft && feetOffLeft) || (headOffRight && feetOffRight)) {
            dbgOtherSkip++;
            dbgSkipOffScreenX++;
            noteSkip(pd.name, "offScreenX");
            continue;
        }

        // Box geometry comes from a projected world AABB rather than the Root->head
        // vertical segment. A vertical segment has no projected height once the view
        // direction lines up with world Z - looking down on someone from above - which
        // collapsed the box to nothing and then got the entity culled outright. An AABB
        // always keeps its horizontal footprint, so it has a floor at every angle.
        double aabbMinX = baseLoc.x - kEspCapsuleRadius;
        double aabbMaxX = baseLoc.x + kEspCapsuleRadius;
        double aabbMinY = baseLoc.y - kEspCapsuleRadius;
        double aabbMaxY = baseLoc.y + kEspCapsuleRadius;
        double aabbMinZ = baseLoc.z;
        double aabbMaxZ = isKnocked ? (baseLoc.z + 40.0) : (baseLoc.z + 90.0);
        {
            // Floor the box at the feet, not at baseLoc. baseLoc is the root component,
            // and UE puts that at the capsule *centre* - the ~92cm c2wOffset every
            // healthy pawn reports in [MeshStatic] - so starting the AABB there throws
            // away the entire lower body. What was left was roughly 88cm of world height
            // against the fixed 68cm footprint, which renders square for a standing
            // player and wider-than-tall for a crouched one: the horizontal box.
            //
            // Prefer the real foot bone, and fall back to the nominal capsule half
            // height when it is missing, so a player whose bone sample has not landed
            // yet still gets a box that reaches the ground rather than a stunted one.
            constexpr double kCapsuleHalfHeight = 90.0;
            double footZ = baseLoc.z - kCapsuleHalfHeight;
            const Vector3 rootBoneShifted = boneAt(Root);
            if (haveCachedRoot && baseLoc.distance(rootBoneShifted) < 250.0)
                footZ = (std::min)(footZ, (double)rootBoneShifted.z);
            aabbMinZ = (std::min)(aabbMinZ, footZ);

            // Real Head bone whenever the skeleton read produced one; the +90 above is
            // only the fallback for a Root-resolved-but-Head-missing read.
            const Vector3 headBoneShifted = boneAt(Head);
            if (!headBoneShifted.IsZero() && is_valid_bone_position(headBoneShifted) &&
                baseLoc.distance(headBoneShifted) < 300.0)
                aabbMaxZ = (std::max)(aabbMaxZ, (double)headBoneShifted.z);
            if (aabbMaxZ < aabbMinZ)
                std::swap(aabbMinZ, aabbMaxZ);

            // A head bone that reads low must not be able to flatten the box into a
            // horizontal bar on a standing pawn. Crouched is around 120cm and clears this
            // untouched. Knocked pawns are supposed to be prone: do not force 90cm of
            // height, and if the bone cache is still a standing pose, flatten it.
            if (!isKnocked) {
                constexpr double kMinBodyHeight = 90.0;
                if (aabbMaxZ - aabbMinZ < kMinBodyHeight)
                    aabbMaxZ = aabbMinZ + kMinBodyHeight;
            }

            // Limb bones are erased when the skeleton read fails, so they can only
            // tighten a pose (prone, crouched, leaning), never define the box alone.
            // A bone that passes is_valid_bone_position but sits further than a body
            // length from Root is a stale/foreign transform, and letting it into the
            // AABB would inflate the box to nonsense.
            if (fullSkeletonUsable) {
                constexpr double kMaxBoneRadius = 200.0;
                for (const auto& boneEntry : pd.cachedBones) {
                    Vector3 bp = boneEntry.second;
                    if (bp.IsZero())
                        continue;
                    bp.x += boneShift.x;
                    bp.y += boneShift.y;
                    if (!is_valid_bone_position(bp) || baseLoc.distance(bp) > kMaxBoneRadius)
                        continue;
                    aabbMinX = (std::min)(aabbMinX, bp.x);
                    aabbMaxX = (std::max)(aabbMaxX, bp.x);
                    aabbMinY = (std::min)(aabbMinY, bp.y);
                    aabbMaxY = (std::max)(aabbMaxY, bp.y);
                    aabbMinZ = (std::min)(aabbMinZ, bp.z);
                    aabbMaxZ = (std::max)(aabbMaxZ, bp.z);
                }
            }

            // DBNO detected (asherick in Debug 51) while the bone cache is still a standing
            // ref pose. Keep the XY footprint, flatten Z, and skip the upright skeleton.
            if (isKnocked && (aabbMaxZ - aabbMinZ) > 70.0) {
                knockedStandingBones = true;
                aabbMaxZ = aabbMinZ + 45.0;
            }
        }

        {
            Vector3 boxHead(
                (aabbMinX + aabbMaxX) * 0.5,
                (aabbMinY + aabbMaxY) * 0.5,
                aabbMaxZ);
            Vector3 boxFeet(
                (aabbMinX + aabbMaxX) * 0.5,
                (aabbMinY + aabbMaxY) * 0.5,
                aabbMinZ);
            Vector2 hs{}, fs{};
            if (vc.Project(boxHead, &hs) && vc.Project(boxFeet, &fs)) {
                headPos = boxHead;
                feetPos = boxFeet;
                headScreen = hs;
                feetScreen = fs;
            }
        }

        Vector2 boxMinScreen{}, boxMaxScreen{};
        int projectedCorners = 0;
        for (int ci = 0; ci < 8; ++ci) {
            Vector3 corner(
                (ci & 1) ? aabbMaxX : aabbMinX,
                (ci & 2) ? aabbMaxY : aabbMinY,
                (ci & 4) ? aabbMaxZ : aabbMinZ);

            Vector2 cornerScreen{};
            if (!vc.Project(corner, &cornerScreen))
                continue; // behind the near plane; the remaining corners still define a box

            if (projectedCorners == 0) {
                boxMinScreen = cornerScreen;
                boxMaxScreen = cornerScreen;
            } else {
                boxMinScreen.x = (std::min)(boxMinScreen.x, cornerScreen.x);
                boxMinScreen.y = (std::min)(boxMinScreen.y, cornerScreen.y);
                boxMaxScreen.x = (std::max)(boxMaxScreen.x, cornerScreen.x);
                boxMaxScreen.y = (std::max)(boxMaxScreen.y, cornerScreen.y);
            }
            projectedCorners++;
        }
        if (projectedCorners == 0) {
            dbgW2sFail++;
            noteSkip(pd.name, "noCorners");
            continue;
        }

        // Ghost / wrong-space reads show up as non-finite or absurd screen extents.
        // A merely small box is legitimate geometry (distant or directly overhead) and
        // gets clamped to a marker below instead of being dropped - world-space validity
        // is already enforced by is_valid_bone_position(baseLoc), the distance_vis gate
        // and the 250-unit live-vs-bone coherence check above.
        const float boxH = boxMaxScreen.y - boxMinScreen.y;
        const float boxW = boxMaxScreen.x - boxMinScreen.x;
        const float kAbsurdExtent = (float)Height * 4.0f;
        if (!std::isfinite(boxMinScreen.x) || !std::isfinite(boxMinScreen.y) ||
            !std::isfinite(boxMaxScreen.x) || !std::isfinite(boxMaxScreen.y) ||
            boxH > kAbsurdExtent || boxW > kAbsurdExtent) {
            dbgOtherSkip++;
            dbgSkipBoxExtent++;
            noteSkip(pd.name, "boxExtent");
            continue;
        }

        if (boxH < dbgMinBoxH) {
            const double elevDx = baseLoc.x - cameraLocation.x;
            const double elevDy = baseLoc.y - cameraLocation.y;
            const double elevDz = baseLoc.z - cameraLocation.z;
            const double elevHoriz = sqrt(elevDx * elevDx + elevDy * elevDy);
            dbgMinBoxH = boxH;
            dbgMinBoxDist = distMeters;
            dbgMinBoxName = pd.name.empty() ? std::string("<empty>") : pd.name;
            dbgMinBoxElev = (elevHoriz > 0.01)
                ? (float)(atan2(elevDz, elevHoriz) * 57.2957795130823)
                : (elevDz >= 0.0 ? 90.f : -90.f);
        }

        // Micro-jitter only — never hold stale screen coords (that caused snap-back).
        // The projected AABB rides along so the box keeps the same damping it had when
        // it was derived from headScreen/feetScreen.
        struct EspScreenSmooth {
            float hx = 0, hy = 0, fx = 0, fy = 0;
            float bminx = 0, bminy = 0, bmaxx = 0, bmaxy = 0;
            bool init = false;
        };
        static std::unordered_map<uint64_t, EspScreenSmooth> s_espSmooth;
        auto& sm = s_espSmooth[pd.entity];
        const EspScreenSmooth freshSample{
            (float)headScreen.x, (float)headScreen.y,
            (float)feetScreen.x, (float)feetScreen.y,
            boxMinScreen.x, boxMinScreen.y, boxMaxScreen.x, boxMaxScreen.y,
            true
        };
        if (!sm.init) {
            sm = freshSample;
        } else {
            const float dhx = (float)headScreen.x - sm.hx;
            const float dhy = (float)headScreen.y - sm.hy;
            const float d = sqrtf(dhx * dhx + dhy * dhy);
            if (d < 28.0f) {
                constexpr float a = 0.70f; // favor fresh sample; light damp only
                sm.hx = sm.hx * (1.f - a) + (float)headScreen.x * a;
                sm.hy = sm.hy * (1.f - a) + (float)headScreen.y * a;
                sm.fx = sm.fx * (1.f - a) + (float)feetScreen.x * a;
                sm.fy = sm.fy * (1.f - a) + (float)feetScreen.y * a;
                sm.bminx = sm.bminx * (1.f - a) + boxMinScreen.x * a;
                sm.bminy = sm.bminy * (1.f - a) + boxMinScreen.y * a;
                sm.bmaxx = sm.bmaxx * (1.f - a) + boxMaxScreen.x * a;
                sm.bmaxy = sm.bmaxy * (1.f - a) + boxMaxScreen.y * a;
                headScreen.x = sm.hx; headScreen.y = sm.hy;
                feetScreen.x = sm.fx; feetScreen.y = sm.fy;
                boxMinScreen.x = sm.bminx; boxMinScreen.y = sm.bminy;
                boxMaxScreen.x = sm.bmaxx; boxMaxScreen.y = sm.bmaxy;
            } else {
                // Large move (turn / sprint / ADS) — take new sample immediately.
                sm = freshSample;
            }
        }
        if (s_espSmooth.size() > 256) {
            s_espSmooth.clear(); // rare; avoid walking map every frame
        }

        // Aim even when health hasn't been cached yet; skip confirmed dead / DBNO.
        const bool healthOk = !pd.isDBNO && (pd.maxHealth <= 0.0 || pd.currentHealth > 0.0);
        if (!pd.is_team && healthOk && distMeters <= AimCfg.maxDistance)
        {
            bool liveVisible = pd.isVisible;
            if (AimCfg.visibleonly) {
                liveVisible = IsVisible(pd.skeletalMesh);
            } else {
                liveVisible = true;
            }
            if (AimCfg.visibleonly && !liveVisible)
            {
                // still draw ESP below; just skip aim selection
            }
            else
            {
            static Bones kAimBones[] = { Head, neck_01, spine_03, pelvis };
            int mask = HitBoxMask;
            if (mask == 0)
                mask = (HitBox >= 0 && HitBox < 4) ? (1 << HitBox) : 1;

            std::vector<int> selected;
            selected.reserve(4);
            for (int i = 0; i < 4; ++i) {
                if (mask & (1 << i))
                    selected.push_back(i);
            }
            if (selected.empty())
                selected.push_back(0);

            auto considerBone = [&](Bones targetBone)
            {
                Vector3 aimBonePos = GetCachedBone(pd, targetBone);
                if (aimBonePos.IsZero())
                    return;

                Vector3 rawBone = aimBonePos;
                if (AimCfg.prediction && localProjectileSpeed > 0.f)
                {
                    float bulletSpeed = (localMaxProjectileSpeed > 0.f) ? localMaxProjectileSpeed : localProjectileSpeed;
                    float gravity = std::abs(localWorldGravity) * localProjectileGravity;

                    float dx = aimBonePos.x - cameraLocation.x;
                    float dy = aimBonePos.y - cameraLocation.y;
                    float horizontalDist = sqrt(dx * dx + dy * dy);
                    float timeToHit = horizontalDist / bulletSpeed;

                    Vector3 predictedPos;
                    predictedPos.x = aimBonePos.x + (pd.velocity.x * timeToHit);
                    predictedPos.y = aimBonePos.y + (pd.velocity.y * timeToHit);
                    predictedPos.z = aimBonePos.z + (pd.velocity.z * timeToHit);

                    float bulletDrop = 0.5f * gravity * timeToHit * timeToHit;
                    predictedPos.z += bulletDrop;

                    aimBonePos = predictedPos;
                }

                Vector2 aimBoneScreen{};
                if (!vc.Project(aimBonePos, &aimBoneScreen))
                    return;

                float screenDist = sqrt(
                    (aimBoneScreen.x - centerX) * (aimBoneScreen.x - centerX) +
                    (aimBoneScreen.y - centerY) * (aimBoneScreen.y - centerY)
                );

                if (screenDist <= AimCfg.fov && screenDist < bestAimDist)
                {
                    originalBonePos = rawBone;
                    bestAimDist = screenDist;
                    bestAimBone = aimBonePos;
                    bestAimScreen = aimBoneScreen;
                    bestTargetName = pd.name;
                    TargetVisible = liveVisible;
                    aimTargetFound = true;
                }
            };

            if (AimCfg.randomBone)
            {
                static std::mt19937 rng{ std::random_device{}() };
                std::uniform_int_distribution<int> dist(0, (int)selected.size() - 1);
                considerBone(kAimBones[selected[dist(rng)]]);
            }
            else
            {
                // Multi-bone: pick the selected bone closest to crosshair.
                for (int idx : selected)
                    considerBone(kAimBones[idx]);
            }
            }
        }

        if (!visualsb)
        {
            dbgSkipVisualsOff++;
            noteSkip(pd.name, "espOff");
            if (doProf) { tPlayerUs += qpcUs(tPlayerStart, qpcNow()); }
            continue;
        }

        if (offarrows)
        {
            ArrowTarget at{};
            at.isVisible = pd.isVisible;
            at.headScreen = headScreen;
            localArrows.push_back(at);
        }

        // Clamped to a minimum on-screen size so distant and directly-overhead entities
        // render as a small marker instead of vanishing. Width still comes from
        // box_width_ratio rather than the AABB's horizontal extent, so every downstream
        // consumer (paddings, bar geometry, chams distScale) keeps its existing tuning.
        constexpr float kMinBoxHeight = 6.0f;
        constexpr float kMinBoxWidth = 3.0f;
        float height = (std::max)(boxMaxScreen.y - boxMinScreen.y, kMinBoxHeight);
        float width = (std::max)(height * box_width_ratio, kMinBoxWidth);

        float topPadding = height * 0.06f + 2.0f;
        float bottomPadding = height * 0.04f + 2.0f;

        const float boxCenterX = (boxMinScreen.x + boxMaxScreen.x) * 0.5f;
        const float boxCenterY = (boxMinScreen.y + boxMaxScreen.y) * 0.5f;

        float boxLeft = boxCenterX - (width * 0.5f);
        float boxRight = boxCenterX + (width * 0.5f);
        float boxTop = boxCenterY - (height * 0.5f) - topPadding;
        float boxBottom = boxCenterY + (height * 0.5f) + bottomPadding;

        ImVec2 pMin(boxLeft, boxTop);
        ImVec2 pMax(boxRight, boxBottom);

        // Sampled fresh rather than taken from pd.isVisible so the colour reacts at
        // render rate instead of at the entity-poll rate, then debounced, because the
        // raw sample chatters for an occluded mesh. Reading it straight through was
        // what strobed the box and skeleton between the visible and invisible colours
        // for a player standing still inside a building.
        bool visRaw = true;
        bool drawVisible = true;
        if (visibilityCheck) {
            if (g_PlatformVersion != PlatformVersion::Xbox) {
                const auto vr = Platform::Steam::ClassifyVisible(pd.skeletalMesh);
                visRaw = vr.method ? vr.visible : pd.isVisible;
            } else {
                visRaw = IsVisible(pd.skeletalMesh);
            }
            drawVisible = StableVisible(pd.entity, visRaw);
        }

        if (debugConsoleEnabled) {
            static uint64_t s_drawVisMs = 0;
            const uint64_t nowVis = GetTickCount64();
            if ((nowVis - s_drawVisMs) > 4000) {
                s_drawVisMs = nowVis;
                Platform::Steam::MeshVisRead vr{};
                if (g_PlatformVersion != PlatformVersion::Xbox &&
                    is_valid_pointer_optimized(pd.skeletalMesh))
                    vr = Platform::Steam::ClassifyVisible(pd.skeletalMesh);
                char vline[448];
                snprintf(vline, sizeof(vline),
                    "[VisDraw] name=%s raw=%d stable=%d cacheVis=%d usedInv=%d method=%u "
                    "t=(%.3f,%.3f,%.3f) recA57=%u rec997=%u flagOff=0x%X "
                    "skVis=(%.2f,%.2f,%.2f) skInv=(%.2f,%.2f,%.2f)",
                    pd.name.empty() ? "?" : pd.name.c_str(),
                    visRaw ? 1 : 0, drawVisible ? 1 : 0, pd.isVisible ? 1 : 0,
                    (visibilityCheck && !drawVisible) ? 1 : 0,
                    vr.method, vr.submit, vr.render, vr.onscreen,
                    vr.a57, vr.a97, vr.flagOff,
                    col_skeleton[0], col_skeleton[1], col_skeleton[2],
                    col_skeleton_invisible[0], col_skeleton_invisible[1], col_skeleton_invisible[2]);
                DebugSystem::LogConsole(vline);
            }
        }

        dbgEspDrawn++;
        if (fullSkeletonUsable && !knockedStandingBones)
            dbgSkelDrawn++;
        else {
            dbgNoSkel++;
            if (dbgNoSkelNames.size() < 120) {
                if (!dbgNoSkelNames.empty()) dbgNoSkelNames += ", ";
                dbgNoSkelNames += pd.name.empty() ? "?" : pd.name;
            }
        }

        ImU32 boxColor = ToU32(colpick2_generic);
        ImU32 skeletonColor = ToU32(col_skeleton);

        if (visibilityCheck && !drawVisible)
        {
            boxColor = ToU32(col_invisible);
            skeletonColor = ToU32(col_skeleton_invisible);
        }

        if (isKnocked)
        {
            boxColor = ToU32(col_knocked);
            skeletonColor = ToU32(col_knocked);
        }

        const float boxHeight = boxBottom - boxTop;
        const float barStackGap = 2.0f;
        const int namePos = std::clamp(indexNamePos, 0, 1);
        const int distPos = std::clamp(indexDistPos, 0, 1);
        const int squadPos = std::clamp(indexSquadPos, 0, 1);
        const int heldPos = std::clamp(indexHeldPos, 0, 1);
        const int hBarPos = std::clamp(healthBarPos, 0, 3);
        const int sBarPos = std::clamp(shieldBarPos, 0, 3);

        auto ClampPct = [](double v)
        {
            if (v < 0.0) return 0.0;
            if (v > 1.0) return 1.0;
            return v;
        };

        auto GetBarStackOffset = [&](int pos, bool isHealth) -> float
        {
            if (isHealth) return 0.0f;
            if (showHealthBar && hBarPos == pos)
                return healthBarWidth + barStackGap;
            return 0.0f;
        };

        auto GetTextYOffset = [&](int textPos, float textHeight) -> float
        {
            float offset = 0.0f;
            if (textPos == 0)
            {
                offset = -textHeight - 2.0f;
                if (showHealthBar && hBarPos == 2)
                    offset -= healthBarWidth + healthBarOffset + 2.0f;
                if (showShieldBar && sBarPos == 2)
                    offset -= shieldBarWidth + shieldBarOffset + 2.0f;
            }
            else if (textPos == 1)
            {
                offset = boxHeight + 2.0f;
                if (showHealthBar && hBarPos == 3)
                    offset += healthBarWidth + healthBarOffset + 2.0f;
                if (showShieldBar && sBarPos == 3)
                    offset += shieldBarWidth + shieldBarOffset + 2.0f;
            }
            else if (textPos == 2)
            {
                offset = 2.0f;
            }
            else if (textPos == 3)
            {
                offset = boxHeight - textHeight - 2.0f;
            }
            return offset;
        };

        auto DrawDashedLine = [&](ImVec2 a, ImVec2 b, ImU32 col, float thickness, float dashLen, float gapLen)
        {
            ImVec2 delta = ImVec2(b.x - a.x, b.y - a.y);
            float len = sqrtf(delta.x * delta.x + delta.y * delta.y);
            if (len <= 0.001f)
                return;
            ImVec2 dir = ImVec2(delta.x / len, delta.y / len);
            float dist = 0.0f;
            while (dist < len)
            {
                float segEnd = (dist + dashLen < len) ? (dist + dashLen) : len;
                ImVec2 p0 = ImVec2(a.x + dir.x * dist, a.y + dir.y * dist);
                ImVec2 p1 = ImVec2(a.x + dir.x * segEnd, a.y + dir.y * segEnd);
                dl->AddLine(p0, p1, col, thickness);
                dist += dashLen + gapLen;
            }
        };

        if (visuals3)
        {
            ImVec2 start;
            if (indexSnapline == 0)
                start = ImVec2(Width * 0.5f, Height * 0.5f);
            else if (indexSnapline == 1)
                start = ImVec2(Width * 0.5f, 0.f);
            else
                start = ImVec2(Width * 0.5f, Height);

            ImVec2 end(feetScreen.x, feetScreen.y);
            dl->AddLine(start, end, ToU32(col_spanline), 1.0f);
        }

        ImU32 healthHighCol = ToU32(healthHighColor);
        ImU32 healthMedCol = ToU32(healthMediumColor);
        ImU32 healthLowCol = ToU32(healthLowColor);
        ImU32 healthBackCol = ToU32(healthBackColor);
        ImU32 shieldCol = ToU32(shieldColor);
        ImU32 shieldBackCol = ToU32(shieldBackColor);

        // Draw whenever we know a max HP (including knocked = empty bar).
        if (showHealthBar && pd.maxHealth > 1.0)
        {
            double pct = isKnocked ? 0.0 : ClampPct(pd.currentHealth / pd.maxHealth);
            ImU32 hpCol = healthHighCol;
            if (healthColorByValue)
            {
                if (isKnocked || pct <= 0.33)
                    hpCol = healthLowCol;
                else if (pct > 0.66)
                    hpCol = healthHighCol;
                else
                    hpCol = healthMedCol;
            }

            float barX, barY, barW, barH;
            bool isVertical = (hBarPos == 0 || hBarPos == 1);
            if (isVertical)
            {
                barW = healthBarWidth;
                barH = boxHeight;
                barY = boxTop;
                barX = (hBarPos == 0)
                    ? (boxLeft - healthBarOffset - healthBarWidth)
                    : (boxRight + healthBarOffset);

                if (healthBarBackground)
                    dl->AddRectFilled({ barX, barY }, { barX + barW, barY + barH }, healthBackCol);

                float fillH = barH * static_cast<float>(pct);
                dl->AddRectFilled({ barX, barY + barH - fillH }, { barX + barW, barY + barH }, hpCol);
                dl->AddRect({ barX, barY }, { barX + barW, barY + barH }, IM_COL32(0, 0, 0, 200), 0.f, 0, 1.0f);
            }
            else
            {
                barW = boxRight - boxLeft;
                barH = healthBarWidth;
                barX = boxLeft;
                barY = (hBarPos == 2)
                    ? (boxTop - healthBarOffset - healthBarWidth)
                    : (boxBottom + healthBarOffset);

                if (healthBarBackground)
                    dl->AddRectFilled({ barX, barY }, { barX + barW, barY + barH }, healthBackCol);

                float fillW = barW * static_cast<float>(pct);
                dl->AddRectFilled({ barX, barY }, { barX + fillW, barY + barH }, hpCol);
                dl->AddRect({ barX, barY }, { barX + barW, barY + barH }, IM_COL32(0, 0, 0, 200), 0.f, 0, 1.0f);
            }
        }

        if (showShieldBar && pd.maxArmor > 0.0)
        {
            double pct = ClampPct(pd.currentArmor / pd.maxArmor);
            float stackOffset = GetBarStackOffset(sBarPos, false);

            float barX, barY, barW, barH;
            bool isVertical = (sBarPos == 0 || sBarPos == 1);
            if (isVertical)
            {
                barW = shieldBarWidth;
                barH = boxHeight;
                barY = boxTop;
                barX = (sBarPos == 0)
                    ? (boxLeft - shieldBarOffset - shieldBarWidth - stackOffset)
                    : (boxRight + shieldBarOffset + stackOffset);

                if (shieldBarBackground)
                    dl->AddRectFilled({ barX, barY }, { barX + barW, barY + barH }, shieldBackCol);

                float fillH = barH * static_cast<float>(pct);
                dl->AddRectFilled({ barX, barY + barH - fillH }, { barX + barW, barY + barH }, shieldCol);
                dl->AddRect({ barX, barY }, { barX + barW, barY + barH }, IM_COL32(0, 0, 0, 200), 0.f, 0, 1.0f);
            }
            else
            {
                barW = boxRight - boxLeft;
                barH = shieldBarWidth;
                barX = boxLeft;
                barY = (sBarPos == 2)
                    ? (boxTop - shieldBarOffset - shieldBarWidth - stackOffset)
                    : (boxBottom + shieldBarOffset + stackOffset);

                if (shieldBarBackground)
                    dl->AddRectFilled({ barX, barY }, { barX + barW, barY + barH }, shieldBackCol);

                float fillW = barW * static_cast<float>(pct);
                dl->AddRectFilled({ barX, barY }, { barX + fillW, barY + barH }, shieldCol);
                dl->AddRect({ barX, barY }, { barX + barW, barY + barH }, IM_COL32(0, 0, 0, 200), 0.f, 0, 1.0f);
            }
        }

        if (visuals2)
        {
            if (showFilledBox)
                dl->AddRectFilled(pMin, pMax, ToU32(col_filledbox));

            if (showOutlineBox)
                dl->AddRect(
                    { pMin.x - 1.f, pMin.y - 1.f },
                    { pMax.x + 1.f, pMax.y + 1.f },
                    IM_COL32(0, 0, 0, 255),
                    0.f, 0, 1.5f
                );

            if (indexBox == 0)
            {
                dl->AddRect(pMin, pMax, boxColor, 0.f, 0, box_thickness);
            }
            else if (indexBox == 1)
            {
                float boxWidth = pMax.x - pMin.x;
                float boxHeight = pMax.y - pMin.y;

                float baseLength = boxWidth * 0.25f;
                float altLength = boxHeight * 0.25f;
                float cornerLength = (baseLength < altLength) ? baseLength : altLength;
                float t = box_thickness;

                dl->AddLine({ pMin.x, pMin.y }, { pMin.x + cornerLength, pMin.y }, boxColor, t);
                dl->AddLine({ pMin.x, pMin.y }, { pMin.x, pMin.y + cornerLength }, boxColor, t);

                dl->AddLine({ pMax.x, pMin.y }, { pMax.x - cornerLength, pMin.y }, boxColor, t);
                dl->AddLine({ pMax.x, pMin.y }, { pMax.x, pMin.y + cornerLength }, boxColor, t);

                dl->AddLine({ pMin.x, pMax.y }, { pMin.x + cornerLength, pMax.y }, boxColor, t);
                dl->AddLine({ pMin.x, pMax.y }, { pMin.x, pMax.y - cornerLength }, boxColor, t);

                dl->AddLine({ pMax.x, pMax.y }, { pMax.x - cornerLength, pMax.y }, boxColor, t);
                dl->AddLine({ pMax.x, pMax.y }, { pMax.x, pMax.y - cornerLength }, boxColor, t);
            }
            else if (indexBox == 2)
            {
                DrawDashedLine(pMin, ImVec2(pMax.x, pMin.y), boxColor, box_thickness, box_dash_length, box_dash_gap);
                DrawDashedLine(ImVec2(pMax.x, pMin.y), pMax, boxColor, box_thickness, box_dash_length, box_dash_gap);
                DrawDashedLine(pMax, ImVec2(pMin.x, pMax.y), boxColor, box_thickness, box_dash_length, box_dash_gap);
                DrawDashedLine(ImVec2(pMin.x, pMax.y), pMin, boxColor, box_thickness, box_dash_length, box_dash_gap);
            }
            else if (indexBox == 3)
            {
                float round = box_rounding;
                float maxRound = (std::min)(pMax.x - pMin.x, pMax.y - pMin.y) * 0.5f;
                if (round > maxRound) round = maxRound;
                dl->AddRect(pMin, pMax, boxColor, round, 0, box_thickness);
            }
        }

        if (headdot) {
            // Track real head when skeleton is good; otherwise box top.
            // Heavy screen-space EMA kills sprint bob without freezing on turns.
            Vector2 headDotTarget = headScreen;
            if (fullSkeletonUsable && !knockedStandingBones) {
                Vector3 skelHead = boneAt(Bones::Head);
                if (skelHead.IsZero() || !is_valid_bone_position(skelHead)) {
                    skelHead = boneAt(Bones::neck_02);
                    if (is_valid_bone_position(skelHead))
                        skelHead.z += 12.0f;
                }
                Vector2 skelHeadScreen{};
                if (is_valid_bone_position(skelHead) && vc.Project(skelHead, &skelHeadScreen)) {
                    const float dx = (float)(skelHeadScreen.x - headScreen.x);
                    const float dy = (float)(skelHeadScreen.y - headScreen.y);
                    // Reject detached / sideways garbage — stay near the box head.
                    if ((dx * dx + dy * dy) <= (height * 0.55f) * (height * 0.55f))
                        headDotTarget = skelHeadScreen;
                }
            }

            struct HeadDotSmooth {
                float x = 0.f, y = 0.f;
                bool init = false;
            };
            static std::unordered_map<uint64_t, HeadDotSmooth> s_headDotSmooth;
            auto& hs = s_headDotSmooth[pd.entity];
            Vector2 headDotScreen = headDotTarget;
            if (!hs.init) {
                hs = { (float)headDotTarget.x, (float)headDotTarget.y, true };
            } else {
                const float dx = (float)headDotTarget.x - hs.x;
                const float dy = (float)headDotTarget.y - hs.y;
                const float d = sqrtf(dx * dx + dy * dy);
                if (d > 90.0f) {
                    // Hard cut (teleport / first sight) — snap.
                    hs.x = (float)headDotTarget.x;
                    hs.y = (float)headDotTarget.y;
                } else {
                    // Strong damp on small motion (run bob); still follows turns quickly.
                    const float a = (d > 40.0f) ? 0.55f : 0.28f;
                    hs.x = hs.x * (1.f - a) + (float)headDotTarget.x * a;
                    hs.y = hs.y * (1.f - a) + (float)headDotTarget.y * a;
                }
                headDotScreen.x = hs.x;
                headDotScreen.y = hs.y;
            }
            if (s_headDotSmooth.size() > 256)
                s_headDotSmooth.clear();

            float headCircleRadius = height * 0.08f;
            dl->AddCircle(
                ImVec2((float)headDotScreen.x, (float)headDotScreen.y),
                (float)headCircleRadius,
                ToU32(col_headdot)
            );
        }

        if (visuals5 && fullSkeletonUsable && !knockedStandingBones)
        {
            // Only reject clear sideways junk when the ESP box itself is upright (not prone).
            Vector3 skelHead = boneAt(Bones::Head);
            Vector3 skelPelvis = boneAt(Bones::pelvis);
            Vector3 skelFoot = boneAt(Bones::foot_l);
            if (skelFoot.IsZero())
                skelFoot = boneAt(Bones::foot_r);
            bool skelScreenOk = true;
            const bool boxUpright = height > 28.f;
            if (boxUpright && !skelHead.IsZero() && !skelFoot.IsZero()) {
                Vector2 sH{}, sF{};
                if (vc.Project(skelHead, &sH) && vc.Project(skelFoot, &sF)) {
                    const float sv = fabsf((float)(sH.y - sF.y));
                    const float sh = fabsf((float)(sH.x - sF.x));
                    if (sv > 22.f && sh > sv * 1.7f)
                        skelScreenOk = false;
                }
            }
            if (!skelPelvis.IsZero() && skelPelvis.distance(baseLoc) > 240.0)
                skelScreenOk = false;

            if (skelScreenOk)
            {
            const float boneDt = ImGui::GetIO().DeltaTime;
            Vector2 skelHeadScreen{}, skelNeckScreen{};
            bool haveSkelHead = false, haveSkelNeck = false;

            for (const auto& chain : skeletonChains)
            {
                Vector2 prevScreen{};
                Vector3 prevWorld{};
                bool havePrev = false;

                for (Bones bone : chain)
                {
                    Vector3 w = boneAt(bone);
                    if (w.IsZero() || !is_valid_bone_position(w))
                        continue; // unresolved link: the next bone bridges the gap
                    // Reject limbs that drifted from the pawn root (stale bone decrypt).
                    if (w.distance(baseLoc) > 260.0)
                        continue;

                    Vector2 s{};
                    if (!vc.Project(SmoothBoneForRender(pd.entity, bone, w, boneDt), &s))
                        continue;

                    if (havePrev)
                    {
                        // Segment length sanity (stops tiny red squiggles from junk
                        // samples). Measured on the raw bones, not the smoothed ones, so
                        // a limb mid-catchup is never mistaken for a bad read.
                        const float seg = (float)prevWorld.distance(w);
                        if (seg >= 1.5f && seg <= 180.0f)
                        {
                            dl->AddLine(
                                ImVec2((float)prevScreen.x, (float)prevScreen.y),
                                ImVec2((float)s.x, (float)s.y),
                                skeletonColor,
                                skeleton_thickness
                            );
                        }
                    }

                    prevScreen = s;
                    prevWorld = w;
                    havePrev = true;

                    if (bone == Head) { skelHeadScreen = s; haveSkelHead = true; }
                    else if (bone == neck_02) { skelNeckScreen = s; haveSkelNeck = true; }
                }
            }

            // Head circle, in the skeleton's own colour. Radius is the projected
            // neck-to-head length, which already carries distance and perspective, so it
            // needs no distance term of its own; the box-height fraction is only there
            // for meshes whose neck did not resolve. Skipped when the separate head-dot
            // overlay is enabled, since that draws its own circle in the same place.
            if (!headdot && haveSkelHead)
            {
                float headRadius = height * 0.07f;
                if (haveSkelNeck)
                {
                    const float hdx = (float)(skelHeadScreen.x - skelNeckScreen.x);
                    const float hdy = (float)(skelHeadScreen.y - skelNeckScreen.y);
                    headRadius = sqrtf(hdx * hdx + hdy * hdy) * 0.9f;
                }
                if (headRadius < 1.5f) headRadius = 1.5f;
                dl->AddCircle(
                    ImVec2((float)skelHeadScreen.x, (float)skelHeadScreen.y),
                    headRadius,
                    skeletonColor,
                    0,
                    skeleton_thickness
                );
            }
            }
        }
        else if (visuals5 && pd.bones_valid && !posOnlyFallback && !isKnocked && distMeters >= kDistantMarkerMeters)
        {
            // Beyond this range the engine stops animating a full rig, so the bone read
            // collapses to a near-point and the pose genuinely cannot be recovered - it is
            // not a decode bug. Two players in one log failed coherence purely this way,
            // one from 413m and one from 392m, and neither ever appeared closer.
            //
            // A marker derived from the box keeps them reading as players instead of empty
            // rectangles. Safe only because it is distance-gated: at this range the box is
            // a handful of pixels tall, so nothing about the real pose is discernible and
            // nothing can be contradicted. Close-range failures deliberately fall through
            // to box-only rather than being handed a fabricated stance.
            //
            // A spine line and a head circle, nothing more - limbs or a shoulder bar would
            // be sub-pixel here and would only smear the shape.
            const float markerX = (boxLeft + boxRight) * 0.5f;
            if (boxBottom > boxTop)
            {
                dl->AddLine(ImVec2(markerX, boxBottom), ImVec2(markerX, boxTop),
                    skeletonColor, skeleton_thickness);
                if (!headdot)
                {
                    float markerHeadR = (boxBottom - boxTop) * 0.10f;
                    if (markerHeadR < 1.0f) markerHeadR = 1.0f;
                    dl->AddCircle(ImVec2(markerX, boxTop + markerHeadR), markerHeadR,
                        skeletonColor, 0, skeleton_thickness);
                }
            }
        }

        // fullSkeleton, not just bones_valid. bones_valid only means Head and Root are
        // cached, and those two survive a failed skeleton read by design so the box can
        // still be drawn. Without this gate the chams kept rendering whatever limbs were
        // left over from a pose the coherence check had already thrown out, which is
        // what painted a sideways scribble inside JimmyNeutron's upright box while the
        // skeleton ESP - which does check fullSkeleton - correctly drew nothing.
        if (drawChams && fullSkeletonUsable && pd.bones_valid && !pd.cachedBones.empty() && !isKnocked)
        {
            BoneVis::Draw(dl, pd, vc, height, distMeters, col_chams, col_chams2, boneShift);
        }

        float size = textFont->FontSize;
        float belowOffset = 0.f;
        float aboveOffset = 0.f;

        ImU32 nameColor = ToU32(col_username);
        ImU32 distColor = ToU32(col_distance);
        ImU32 squadColor = ToU32(col_squad);
        ImU32 flagsColor = ImGui::ColorConvertFloat4ToU32(ImVec4(
            Cfg::Scene::playerFlagsColor[0], Cfg::Scene::playerFlagsColor[1],
            Cfg::Scene::playerFlagsColor[2], Cfg::Scene::playerFlagsColor[3]));
        if (visibilityCheck && !drawVisible) {
            const ImU32 occ = ToU32(col_skeleton_invisible);
            nameColor = occ;
            distColor = occ;
            squadColor = occ;
            flagsColor = occ;
        }

        auto GetStackedY = [&](int pos, float textHeight) -> float
        {
            float base = boxTop + GetTextYOffset(pos, textHeight);
            if (pos == 0)
            {
                float y = base - aboveOffset;
                aboveOffset += textHeight + 2.f;
                return y;
            }

            float y = base + belowOffset;
            belowOffset += textHeight + 2.f;
            return y;
        };

        // Name and distance draw whatever the player's state. Being knocked used to
        // replace this entire block and then continue, which pulled the name, distance,
        // held item, squad and player flags off screen at exactly the moment they matter
        // most - you could see that somebody was down but not who, or what they were
        // holding. "Knocked" is now just another entry in the below-box stack.
        if (visuals1)
        {
            // Name and distance are two text runs, not one formatted string. Baking the
            // distance into the name meant it drew in the name colour, so Distance Color
            // did nothing at all in Name+Distance mode - the setting only ever reached
            // the standalone distance label below.
            const bool nameCarriesDistance = distanceWithName && visuals4;

            char lineName[160];
            snprintf(lineName, sizeof(lineName), XC("%s"), pd.name.c_str());
            char lineNameDist[32];
            lineNameDist[0] = '\0';
            if (nameCarriesDistance)
                snprintf(lineNameDist, sizeof(lineNameDist), XC(" [%dm]"), (int)distMeters);

            ImVec2 tName = textFont->CalcTextSizeA(size, FLT_MAX, 0.0f, lineName);
            ImVec2 tNameDist = nameCarriesDistance
                ? textFont->CalcTextSizeA(size, FLT_MAX, 0.0f, lineNameDist)
                : ImVec2(0.f, 0.f);
            const bool drawPlat = showPlatform && pd.platform != 0;
            const float badgeGap = 4.f;
            float platScale = platformIconScale;
            if (platScale < 0.5f)
                platScale = 0.5f;
            if (platScale > 3.f)
                platScale = 3.f;
            const float badgeW = drawPlat ? (size * 1.35f * platScale + badgeGap) : 0.f;
            const float runW = tName.x + tNameDist.x;
            float x = headScreen.x - ((runW + badgeW) * 0.5f);
            float y = GetStackedY(namePos, (std::max)(tName.y, tNameDist.y));

            dl->AddText(textFont, size, { x, y + 1 }, IM_COL32(0, 0, 0, 180), lineName);
            dl->AddText(textFont, size, { x, y }, nameColor, lineName);
            if (nameCarriesDistance)
            {
                dl->AddText(textFont, size, { x + tName.x, y + 1 }, IM_COL32(0, 0, 0, 180), lineNameDist);
                dl->AddText(textFont, size, { x + tName.x, y }, distColor, lineNameDist);
            }
            if (drawPlat)
                DrawPlayerPlatformBadge(dl, textFont, size, x + runW + badgeGap, y, pd.platform);
        }

        if (visuals4 && (!distanceWithName || !visuals1))
        {
            char lineDist[64];
            snprintf(lineDist, sizeof(lineDist), XC("[%dm]"), (int)distMeters);

            ImVec2 t2 = textFont->CalcTextSizeA(size, FLT_MAX, 0.0f, lineDist);
            float x = headScreen.x - (t2.x * 0.5f);
            float y = GetStackedY(distPos, t2.y);

            dl->AddText(textFont, size, { x, y + 1 }, IM_COL32(0, 0, 0, 180), lineDist);
            dl->AddText(textFont, size, { x, y }, distColor, lineDist);
        }

        if (isKnocked)
        {
            const char* msg = XC("Knocked");

            ImVec2 ts = textFont->CalcTextSizeA(size, FLT_MAX, 0.0f, msg);
            float x = headScreen.x - (ts.x * 0.5f);
            float y = boxBottom + 3.f + belowOffset;

            dl->AddText(textFont, size, { x, y + 1 }, IM_COL32(0, 0, 0, 180), msg);
            dl->AddText(textFont, size, { x, y }, IM_COL32(255, 200, 0, 255), msg);

            belowOffset += ts.y + 2.f;
        }

        // Engagement-critical: throwable ready to throw. Always drawn under player ESP
        // so you see Smoke / Triggers / ShowStopper / Trailblazer without needing
        // Held Item checked for guns.
        const bool throwableReady = IsHeldThrowableName(pd.heldItem);
        if (throwableReady)
        {
            char readyLine[96];
            snprintf(readyLine, sizeof(readyLine), XC("Ready: %s"), pd.heldItem.c_str());
            ImVec2 tReady = textFont->CalcTextSizeA(size, FLT_MAX, 0.0f, readyLine);
            float x = headScreen.x - (tReady.x * 0.5f);
            float y = boxBottom + 3.f + belowOffset;
            dl->AddText(textFont, size, { x, y + 1 }, IM_COL32(0, 0, 0, 200), readyLine);
            dl->AddText(textFont, size, { x, y }, IM_COL32(255, 140, 40, 255), readyLine);
            belowOffset += tReady.y + 2.f;
        }

        if (helditem)
        {
            // Throwable already shown as Ready: above — skip duplicate name here.
            if (!throwableReady && pd.heldItem != XC("None") && !pd.heldItem.empty())
            {
                std::string weaponText = pd.heldItem;
                if (weaponlevel && !pd.weaponLevel.empty())
                {
                    weaponText += XC(" [");
                    weaponText += pd.weaponLevel;
                    weaponText += XC("]");
                }

                ImVec2 tWeapon = textFont->CalcTextSizeA(size, FLT_MAX, 0.0f, weaponText.c_str());
                float x = headScreen.x - (tWeapon.x * 0.5f);
                float y = GetStackedY(heldPos, tWeapon.y);

                dl->AddText(textFont, size, { x, y + 1 }, IM_COL32(0, 0, 0, 180), weaponText.c_str());
                ImU32 weaponColor = ImGui::ColorConvertFloat4ToU32(ImVec4(col_held[0], col_held[1], col_held[2], col_held[3]));
                dl->AddText(textFont, size, { x, y }, weaponColor, weaponText.c_str());
            }
            else if (!throwableReady)
            {
                const char* noItem = XC("No Held Item");

                ImVec2 tWeapon = textFont->CalcTextSizeA(size, FLT_MAX, 0.0f, noItem);
                float x = headScreen.x - (tWeapon.x * 0.5f);
                float y = GetStackedY(heldPos, tWeapon.y);

                dl->AddText(textFont, size, { x, y + 1 }, IM_COL32(0, 0, 0, 180), noItem);
                ImU32 weaponColor = ImGui::ColorConvertFloat4ToU32(ImVec4(col_held[0], col_held[1], col_held[2], col_held[3]));
                dl->AddText(textFont, size, { x, y }, weaponColor, noItem);

            }
        }


        // Always one activity. CLAP sprint/zoom were dead or sticky; speed from
        // ComponentVelocity / worldLoc fills Sprinting/Walking/Idle every frame.
        if (Cfg::Scene::showPlayerFlags)
        {
            const char* stance = nullptr;
            const char* activity = nullptr;
            bool idle = false;

            if (Cfg::Scene::playerFlagsFilterMulti[6] && pd.characterStance == 1)
                stance = XC("Crouched");
            else if (Cfg::Scene::playerFlagsFilterMulti[7] && pd.characterStance == 2)
                stance = XC("Prone");

            if (Cfg::Scene::playerFlagsFilterMulti[4] && pd.isReloading)
                activity = XC("Reloading");
            else if (Cfg::Scene::playerFlagsFilterMulti[5] && pd.isFiring &&
                !pd.heldItem.empty() && pd.heldItem != XC("None"))
                activity = XC("Firing");
            else if (Cfg::Scene::playerFlagsFilterMulti[2] && pd.isDodgeRolling)
                activity = XC("Dodge");
            else if (Cfg::Scene::playerFlagsFilterMulti[3] && pd.isInJump)
                activity = XC("Jumping");
            else if (Cfg::Scene::playerFlagsFilterMulti[1] && pd.isSprinting)
                activity = XC("Sprinting");
            else if (Cfg::Scene::playerFlagsFilterMulti[0] && pd.isZooming)
                activity = XC("Aiming");
            else if (pd.horizSpeed >= 90.f)
                activity = XC("Walking");
            else {
                activity = XC("Idle");
                idle = true;
            }

            char flagsText[64];
            if (stance && activity && !idle)
                snprintf(flagsText, sizeof(flagsText), XC("%s + %s"), stance, activity);
            else if (stance && idle)
                snprintf(flagsText, sizeof(flagsText), XC("%s"), stance);
            else
                snprintf(flagsText, sizeof(flagsText), XC("%s"), activity);

            ImVec2 tFlags = textFont->CalcTextSizeA(size, FLT_MAX, 0.0f, flagsText);
            float xFlags = headScreen.x - (tFlags.x * 0.5f);
            float yFlags = boxBottom + 3.f + belowOffset;

            dl->AddText(textFont, size, { xFlags, yFlags + 1 }, IM_COL32(0, 0, 0, 180), flagsText);
            dl->AddText(textFont, size, { xFlags, yFlags }, flagsColor, flagsText);

            belowOffset += tFlags.y + 2.f;

            if (debugConsoleEnabled) {
                static uint64_t s_flagDrawMs = 0;
                const uint64_t nowFlag = GetTickCount64();
                if ((nowFlag - s_flagDrawMs) > 2000) {
                    s_flagDrawMs = nowFlag;
                    char fline[288];
                    snprintf(fline, sizeof(fline),
                        "[FlagDraw] name=%s activity=%s zoom=%d sprint=%d fire=%d reload=%d "
                        "stance=%u speed=%.0f",
                        pd.name.empty() ? "?" : pd.name.c_str(),
                        flagsText,
                        pd.isZooming ? 1 : 0, pd.isSprinting ? 1 : 0,
                        pd.isFiring ? 1 : 0, pd.isReloading ? 1 : 0,
                        (unsigned)pd.characterStance, pd.horizSpeed);
                    DebugSystem::LogConsole(fline);
                }
            }
        }

        if (squadname && !pd.squadname.empty() && pd.squadname != L"NULL")
        {
            std::string squad_utf8 = ws2s(pd.squadname);

            ImVec2 tSquad = textFont->CalcTextSizeA(size, FLT_MAX, 0.0f, squad_utf8.c_str());
            float x = headScreen.x - (tSquad.x * 0.5f);
            float y = GetStackedY(squadPos, tSquad.y);

            dl->AddText(textFont, size, { x, y + 1 }, IM_COL32(0, 0, 0, 180), squad_utf8.c_str());
            dl->AddText(textFont, size, { x, y }, squadColor, squad_utf8.c_str());
        }



        if (doProf) { tPlayerUs += qpcUs(tPlayerStart, qpcNow()); }
        }
        catch (...)
        {
            continue;
        }
    }

#ifdef DEBUG_CONSOLE
    EspReadyTick(dbgEspDrawn > 0);
#endif

    if (doProf) {
        int64_t tLoopEnd = qpcNow();
        double totalLoopUs = qpcUs(tLoopStart, tLoopEnd);
        profEntries.push_back({ XC("MainLoop"), totalLoopUs });
        profEntries.push_back({ XC("  Players"), tPlayerUs });
        profEntries.push_back({ XC("  World"), totalLoopUs - tPlayerUs });
    }

    if (Cfg::Scene::FovStacking && !worldStackItems.empty())
    {
        ImFont* textFont = f.custom[std::clamp(fontIndex, 0, 2)];
        if (textFont)
        {

            float size = textFont->FontSize;

            auto RenderWorldItem = [&](const WorldStackItem& item)
            {
                ImVec2 screen(item.screen.x, item.screen.y);

                if (item.type == WorldStackType::Loot)
                {
                    float nextY = screen.y - 10.f;

                    {
                        char nameLine[192];
                        int off = 0;

                        if (Cfg::Scene::Amount)
                            off += snprintf(nameLine + off, sizeof(nameLine) - off, XC("x%d "), item.amount);
                        if (Cfg::Scene::ShowName)
                            off += snprintf(nameLine + off, sizeof(nameLine) - off, XC("%s"), item.name);
                        if (Cfg::Scene::Price && item.price > 0)
                            off += snprintf(nameLine + off, sizeof(nameLine) - off, XC(" [%d$]"), item.price);
                        if (Cfg::Scene::WorldNameWithDistance)
                            off += snprintf(nameLine + off, sizeof(nameLine) - off, XC(" [%dm]"), (int)item.dist);

                        if (off > 0)
                        {
                            ImVec2 szName = textFont->CalcTextSizeA(size, FLT_MAX, 0, nameLine);
                            float xName = screen.x - szName.x * 0.5f;
                            dl->AddText(textFont, size, { xName + 1, nextY + 1 }, IM_COL32(0, 0, 0, 160), nameLine);
                            dl->AddText(textFont, size, { xName, nextY }, item.color, nameLine);
                            nextY += szName.y + 2.f;
                        }
                    }

                    if (Cfg::Scene::Weight && item.weight > 0.01)
                    {
                        char wLine[64];
                        snprintf(wLine, sizeof(wLine), XC("%.1f kg"), item.weight);
                        ImVec2 szW = textFont->CalcTextSizeA(size, FLT_MAX, 0, wLine);
                        float xW = screen.x - szW.x * 0.5f;
                        dl->AddText(textFont, size, { xW + 1, nextY + 1 }, IM_COL32(0, 0, 0, 160), wLine);
                        dl->AddText(textFont, size, { xW, nextY }, IM_COL32(200, 200, 255, 255), wLine);
                        nextY += szW.y + 2.f;
                    }

                    float circleY = nextY + 4.f;
                    if (Cfg::Scene::WorldShowCircle)
                    {
                        float circleR = 3.3f;
                        dl->AddCircleFilled(ImVec2(screen.x, circleY), circleR, item.color);
                        dl->AddCircle(ImVec2(screen.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                    }
                }
                else if (item.type == WorldStackType::Container)
                {
                    char contLine[96];
                    if (Cfg::Scene::WorldNameWithDistance)
                        snprintf(contLine, sizeof(contLine), XC("%s [%dm]"), item.name, (int)item.dist);
                    else
                        snprintf(contLine, sizeof(contLine), XC("%s"), item.name);
                    ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, contLine);
                    float x1 = screen.x - sz1.x * 0.5f;
                    float y1 = screen.y - sz1.y - 12.f;
                    dl->AddText(textFont, size, { x1 + 1, y1 + 1 }, IM_COL32(0, 0, 0, 160), contLine);
                    dl->AddText(textFont, size, { x1, y1 }, item.color, contLine);

                    float y2 = y1 + sz1.y + 2.f;
                    if (!Cfg::Scene::WorldNameWithDistance)
                    {
                        char distStr[32];
                        snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)item.dist);
                        ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                        float x2 = screen.x - sz2.x * 0.5f;
                        dl->AddText(textFont, size, { x2 + 1, y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                        dl->AddText(textFont, size, { x2, y2 }, IM_COL32(180, 255, 140, 255), distStr);
                        y2 += sz2.y + 2.f;
                    }

                    const char* state = item.opened ? XC("Opened") : XC("Closed");
                    ImU32 colorState = item.opened ? IM_COL32(255, 120, 120, 255) : IM_COL32(120, 220, 255, 255);
                    ImVec2 sz3 = textFont->CalcTextSizeA(size, FLT_MAX, 0, state);
                    float x3 = screen.x - sz3.x * 0.5f;
                    float y3 = y2;
                    dl->AddText(textFont, size, { x3 + 1, y3 + 1 }, IM_COL32(0, 0, 0, 160), state);
                    dl->AddText(textFont, size, { x3, y3 }, colorState, state);

                    float circleY = y3 + sz3.y + 6.f;
                    if (Cfg::Scene::WorldShowCircle)
                    {
                        float circleR = 3.3f;
                        dl->AddCircleFilled(ImVec2(screen.x, circleY), circleR, item.color);
                        dl->AddCircle(ImVec2(screen.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                    }
                }
                else if (item.type == WorldStackType::Salvage)
                {
                    char salvLine[96];
                    if (Cfg::Scene::WorldNameWithDistance)
                        snprintf(salvLine, sizeof(salvLine), XC("%s [%dm]"), item.name, (int)item.dist);
                    else
                        snprintf(salvLine, sizeof(salvLine), XC("%s"), item.name);
                    ImVec2 sz1 = textFont->CalcTextSizeA(size, FLT_MAX, 0, salvLine);
                    float x1 = screen.x - sz1.x * 0.5f;
                    float y1 = screen.y - sz1.y - 12.f;
                    dl->AddText(textFont, size, { x1 + 1, y1 + 1 }, IM_COL32(0, 0, 0, 160), salvLine);
                    dl->AddText(textFont, size, { x1, y1 }, item.color, salvLine);

                    float circleY = y1 + sz1.y + 6.f;
                    if (!Cfg::Scene::WorldNameWithDistance)
                    {
                        char distStr[32];
                        snprintf(distStr, sizeof(distStr), XC("[%dm]"), (int)item.dist);
                        ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                        float x2 = screen.x - sz2.x * 0.5f;
                        float y2 = y1 + sz1.y + 2.f;
                        dl->AddText(textFont, size, { x2 + 1, y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                        dl->AddText(textFont, size, { x2, y2 }, IM_COL32(180, 255, 140, 255), distStr);
                        circleY = y2 + sz2.y + 6.f;
                    }

                    if (Cfg::Scene::WorldShowCircle)
                    {
                        float circleR = 3.3f;
                        dl->AddCircleFilled(ImVec2(screen.x, circleY), circleR, item.color);
                        dl->AddCircle(ImVec2(screen.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                    }
                }
            };

            auto RenderStackLabel = [&](const Vector2& pos, ImU32 color, const char* name, int count, float minDist)
            {
                char nameLine[160];
                if (Cfg::Scene::WorldNameWithDistance)
                    snprintf(nameLine, sizeof(nameLine), XC("%dx %s [%dm+]"), count, name, (int)minDist);
                else
                    snprintf(nameLine, sizeof(nameLine), XC("%dx %s"), count, name);
                ImVec2 szName = textFont->CalcTextSizeA(size, FLT_MAX, 0, nameLine);
                float x1 = pos.x - szName.x * 0.5f;
                float y1 = pos.y - szName.y - 12.f;
                dl->AddText(textFont, size, { x1 + 1, y1 + 1 }, IM_COL32(0, 0, 0, 160), nameLine);
                dl->AddText(textFont, size, { x1, y1 }, color, nameLine);

                float circleY = y1 + szName.y + 6.f;
                if (!Cfg::Scene::WorldNameWithDistance)
                {
                    char distStr[32];
                    snprintf(distStr, sizeof(distStr), XC("[%dm+]"), (int)minDist);
                    ImVec2 sz2 = textFont->CalcTextSizeA(size, FLT_MAX, 0, distStr);
                    float x2 = pos.x - sz2.x * 0.5f;
                    float y2 = y1 + szName.y + 2.f;
                    dl->AddText(textFont, size, { x2 + 1, y2 + 1 }, IM_COL32(0, 0, 0, 160), distStr);
                    dl->AddText(textFont, size, { x2, y2 }, IM_COL32(180, 255, 140, 255), distStr);
                    circleY = y2 + sz2.y + 6.f;
                }

                if (Cfg::Scene::WorldShowCircle)
                {
                    float circleR = 3.3f;
                    dl->AddCircleFilled(ImVec2(pos.x, circleY), circleR, color);
                    dl->AddCircle(ImVec2(pos.x, circleY), circleR, IM_COL32(0, 0, 0, 200), 16, 1.0f);
                }
            };

            auto HashName = [](const char* s) -> uint32_t
            {
                uint32_t h = 2166136261u;
                for (const char* p = s; p && *p; ++p) {
                    unsigned char lc = static_cast<unsigned char>(std::tolower(static_cast<unsigned char>(*p)));
                    h ^= lc;
                    h *= 16777619u;
                }
                return h;
            };

            std::vector<const WorldStackItem*> nearItems;
            std::vector<const WorldStackItem*> farItems;
            nearItems.reserve(worldStackItems.size());
            farItems.reserve(worldStackItems.size());

            for (const auto& item : worldStackItems)
            {
                if (item.dist >= Cfg::Scene::FovStackMinDistance)
                    farItems.push_back(&item);
                else
                    nearItems.push_back(&item);
            }

            for (const auto* item : nearItems)
                RenderWorldItem(*item);

            if (!farItems.empty())
            {
                float angleRad = Cfg::Scene::FovStackAngle * 3.14159f / 180.0f;
                float cosThreshold = cosf(angleRad);

                std::vector<bool> stacked(farItems.size(), false);
                for (size_t i = 0; i < farItems.size(); ++i)
                {
                    if (stacked[i])
                        continue;

                    const WorldStackItem* base = farItems[i];
                    uint32_t baseId = HashName(base->name) ^ (static_cast<uint32_t>(base->type) * 0x9e3779b1u);

                    Vector3 baseDir = base->world - cameraLocation;
                    float baseLen = sqrtf(baseDir.x * baseDir.x + baseDir.y * baseDir.y + baseDir.z * baseDir.z);
                    if (baseLen > 0.001f)
                    {
                        baseDir.x /= baseLen;
                        baseDir.y /= baseLen;
                        baseDir.z /= baseLen;
                    }

                    std::vector<const WorldStackItem*> stackItems;
                    stackItems.push_back(base);
                    stacked[i] = true;

                    float minDist = base->dist;
                    Vector2 displayPos = base->screen;

                    for (size_t j = i + 1; j < farItems.size(); ++j)
                    {
                        if (stacked[j])
                            continue;

                        const WorldStackItem* other = farItems[j];
                        uint32_t otherId = HashName(other->name) ^ (static_cast<uint32_t>(other->type) * 0x9e3779b1u);
                        if (otherId != baseId)
                            continue;

                        Vector3 otherDir = other->world - cameraLocation;
                        float otherLen = sqrtf(otherDir.x * otherDir.x + otherDir.y * otherDir.y + otherDir.z * otherDir.z);
                        if (otherLen > 0.001f)
                        {
                            otherDir.x /= otherLen;
                            otherDir.y /= otherLen;
                            otherDir.z /= otherLen;
                        }

                        float dot = baseDir.x * otherDir.x + baseDir.y * otherDir.y + baseDir.z * otherDir.z;
                        if (dot >= cosThreshold)
                        {
                            stackItems.push_back(other);
                            stacked[j] = true;
                            if (other->dist < minDist)
                            {
                                minDist = other->dist;
                                displayPos = other->screen;
                            }
                        }
                    }

                    if (stackItems.size() == 1)
                    {
                        RenderWorldItem(*stackItems[0]);
                    }
                    else
                    {
                        RenderStackLabel(displayPos, base->color, base->name, (int)stackItems.size(), minDist);
                    }
                }
            }


        }
    }

    if (aimTargetFound)
    {
        if (AimCfg.lines)
        {
            Vector2 boneScreen{};
            if (vc.Project(originalBonePos, &boneScreen))
            {
                ImU32 lineColor = ImGui::ColorConvertFloat4ToU32(
                    ImVec4(col_line[0], col_line[1], col_line[2], col_line[3])
                );

                dl->AddLine(
                    ImVec2(centerX, centerY),
                    ImVec2(boneScreen.x, boneScreen.y),
                    lineColor,
                    1.5f
                );
            }
        }

        if (AimCfg.targetting)
        {
            float distMeters = (bestAimBone - cameraLocation).Length() / 100.f;

            const char* prefix = XC("Targetting ");
            const char* fromTxt = XC(" from ");
            char distStr[32];
            snprintf(distStr, sizeof(distStr), XC("%dm"), (int)distMeters);

            ImFont* textFont = f.custom[1];
            if (textFont)
            {

                float fontSize = textFont->FontSize;
                float y = Height * 0.55f;

                ImVec2 szPrefix = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, prefix);
                ImVec2 szName = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, bestTargetName.c_str());
                ImVec2 szFrom = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, fromTxt);
                ImVec2 szDist = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, distStr);

                float totalWidth = szPrefix.x + szName.x + szFrom.x + szDist.x;
                float startX = (Width * 0.5f) - (totalWidth * 0.5f);

                auto drawText = [&](const char* txt, ImU32 col, float& x)
                    {
                        ImVec2 sz = textFont->CalcTextSizeA(fontSize, FLT_MAX, 0, txt);
                        dl->AddText(textFont, fontSize, ImVec2(x + 1, y + 1), IM_COL32(0, 0, 0, 200), txt);
                        dl->AddText(textFont, fontSize, ImVec2(x, y), col, txt);
                        x += sz.x;
                    };

                drawText(prefix, IM_COL32(255, 255, 255, 255), startX);
                drawText(bestTargetName.c_str(), IM_COL32(120, 255, 120, 255), startX);
                drawText(fromTxt, IM_COL32(255, 255, 255, 255), startX);
                drawText(distStr, IM_COL32(120, 255, 120, 255), startX);


            }
        }
    }


    if (doProf) tA = qpcNow();
    if (aimTargetFound)
    {
        if (!AimCfg.visibleonly || TargetVisible)
        {
            ExecuteAssist(cameraLocation, bestAimScreen, bestAimBone, localPlayerController);
        }
    }

    ExecuteAssistARC(cameraLocation, cameraRotation, cameraFOV, aspectRatio, dl, localPlayerController, localProjectileSpeed, localMaxProjectileSpeed, localProjectileGravity, localWorldGravity, localCache);
    if (doProf) {
        tB = qpcNow();
        profEntries.push_back({ XC("Aimbot"), qpcUs(tA, tB) });
        profEntries.push_back({ XC("TotalFrame"), qpcUs(tFrameStart, qpcNow()) });
    }

    size_t cachedItemCount = localCache.size();

    if (debugConsoleEnabled) {
        static uint64_t s_lastRenderLog = 0;
        uint64_t nowRl = GetNowMs();
        if ((nowRl - s_lastRenderLog) > 2000) {
            s_lastRenderLog = nowRl;
            char line[576];
            snprintf(line, sizeof(line),
                "[Render] cache=%zu realPlayers=%d espDrawn=%d skel=%d noSkel=%d "
                "bonesValid=%d fullFlag=%d w2sFail=%d teamSkip=%d selfSkip=%d frozenSkip=%d boneUnresolvedSkip=%d "
                "posFallback=%d visualsb=%d visOffSkip=%d platOk=%d showPlat=%d "
                "noSkelReason(freshness=%d,w2sFail=%d,other=%d) "
                "otherSkip(baseLoc=%d,dist=%d,boxExtent=%d,offScreenX=%d,invisible=%d) "
                "cam=(%.0f,%.0f,%.0f) fov=%.1f",
                cachedItemCount, dbgRealPlayers, dbgEspDrawn, dbgSkelDrawn, dbgNoSkel,
                dbgBonesValid, dbgFullSkelFlag, dbgW2sFail, dbgTeamSkip, dbgSelfSkip, dbgFrozenSkip, dbgBoneUnresolvedSkip,
                dbgPosFallbackDrawn, visualsb ? 1 : 0, dbgSkipVisualsOff,
                dbgPlatOk, showPlatform ? 1 : 0,
                dbgBoneUnresolvedSkip, dbgW2sFail, dbgOtherSkip,
                dbgSkipBaseLoc, dbgSkipDistance, dbgSkipBoxExtent, dbgSkipOffScreenX, dbgSkipInvisible,
                cameraLocation.x, cameraLocation.y, cameraLocation.z, cameraFOV);
            DebugSystem::LogConsole(line);
            if (!dbgSkipNames.empty()) {
                snprintf(line, sizeof(line), "[Render] skipped: %s", dbgSkipNames.c_str());
                DebugSystem::LogConsole(line);
            }
            if (dbgNoSkel > 0 && !dbgNoSkelNames.empty()) {
                snprintf(line, sizeof(line), "[Render] noSkel names: %s", dbgNoSkelNames.c_str());
                DebugSystem::LogConsole(line);
            }
        }
    }

    // [BoxDiag]: smallest ESP box built this frame, once per second and independent of
    // the [Render] throttle. Camera pitch plus the entity's elevation angle relative to
    // the camera are what separate a legitimately small box (steep look-down at someone
    // far below) from a bad read, and boxExtentSkip should stay at 0.
    if (debugConsoleEnabled && dbgMinBoxH < FLT_MAX) {
        static uint64_t s_lastBoxDiag = 0;
        uint64_t nowBoxDiag = GetTickCount64();
        if ((nowBoxDiag - s_lastBoxDiag) > 1000) {
            s_lastBoxDiag = nowBoxDiag;
            char bline[288];
            snprintf(bline, sizeof(bline),
                "[BoxDiag] minBoxH=%.2f name=%s dist=%.1fm camPitch=%.2f entityElev=%.2f "
                "boxExtentSkip=%d",
                dbgMinBoxH, dbgMinBoxName.c_str(), dbgMinBoxDist,
                (double)cameraRotation.Pitch, dbgMinBoxElev, dbgSkipBoxExtent);
            DebugSystem::LogConsole(bline);
        }
    }

    // [W2SDiag]: behindCam for people off-frustum is normal (Debug 42 was ~all of a
    // 5k-line file). Keep two unique names per second and prefer non-behindCam, which
    // is the only class that would actually mean a broken view matrix.
    if (debugConsoleEnabled && dbgW2sFail >= 8 && !w2sDiagBuffer.empty()) {
        static uint64_t s_w2sDiagWindowStart = 0;
        static int s_w2sDiagLoggedThisWindow = 0;
        static std::unordered_set<std::string> s_w2sDiagNamesThisWindow;
        uint64_t nowW2sDiagMs = GetTickCount64();
        if ((nowW2sDiagMs - s_w2sDiagWindowStart) > 1000) {
            s_w2sDiagWindowStart = nowW2sDiagMs;
            s_w2sDiagLoggedThisWindow = 0;
            s_w2sDiagNamesThisWindow.clear();
        }
        auto logW2s = [&](const W2SDiagEntry& e) {
            if (s_w2sDiagLoggedThisWindow >= 2)
                return;
            if (!s_w2sDiagNamesThisWindow.insert(e.name).second)
                return;
            ++s_w2sDiagLoggedThisWindow;
            char wline[320];
            snprintf(wline, sizeof(wline),
                "[W2SDiag] tick=%llu name=%s worldPos=(%.0f,%.0f,%.0f) camDist=%.1f "
                "projected=(%.1f,%.1f,%.2f) reason=%s",
                (unsigned long long)s_diagFrameTick, e.name.c_str(),
                e.worldPos.x, e.worldPos.y, e.worldPos.z, e.camDist,
                e.hasProjected ? e.projected.x : -1.f, e.hasProjected ? e.projected.y : -1.f,
                e.depth, e.reason);
            DebugSystem::LogConsole(wline);
        };
        for (const auto& e : w2sDiagBuffer) {
            if (e.reason && strcmp(e.reason, "behindCam") != 0)
                logW2s(e);
        }
        for (const auto& e : w2sDiagBuffer)
            logW2s(e);
    }

    float yawDeg = cameraRotation.Yaw;
    if (showRadar)
        DrawStaticRadar(localRelativeLocation, yawDeg);

    if (offarrows)
    {
        ImVec2 screenCenter(Width * 0.5f, Height * 0.5f);
        DrawEnemyArrows(localArrows, screenCenter, AimCfg.fov, Width, Height);
    }

    if (doProf) {
        static double s_profPeak[12] = {};
        static uint64_t s_profPeakTime[12] = {};
        uint64_t profNow = GetNowMs();

        const float profY0 = 200.0f;
        const float profX = 12.0f;
        const float pad = 8.0f;
        const float lineH = 16.0f;
        ImFont* profFont = (f.custom.size() > 0) ? f.custom[0] : nullptr;
        if (profFont) {
            float fs = profFont->FontSize;
            char profBuf[128];
            float y = profY0;

            const float boxW = 200.0f;
            const float boxH = (float)(profEntries.size() + 1) * lineH + pad * 2.0f;
            ImU32 bgCol = IM_COL32(18, 20, 24, 220);
            ImU32 borderCol = IM_COL32(255, 255, 255, 25);
            dl->AddRectFilled(ImVec2(profX, y), ImVec2(profX + boxW, y + boxH), bgCol, 4.0f);
            dl->AddRect(ImVec2(profX, y), ImVec2(profX + boxW, y + boxH), borderCol, 4.0f, 0, 1.0f);
            y += pad;

            snprintf(profBuf, sizeof(profBuf), XC("Frame timing  %zu items"), cachedItemCount);
            dl->AddText(profFont, fs, ImVec2(profX + pad + 1, y + 1), IM_COL32(0, 0, 0, 180), profBuf);
            dl->AddText(profFont, fs, ImVec2(profX + pad, y), IM_COL32(200, 210, 220, 255), profBuf);
            y += lineH;

            for (size_t pi = 0; pi < profEntries.size() && pi < 12; ++pi) {
                if (profEntries[pi].us > s_profPeak[pi] || (profNow - s_profPeakTime[pi]) > 3000) {
                    s_profPeak[pi] = profEntries[pi].us;
                    s_profPeakTime[pi] = profNow;
                }
                ImU32 col = (profEntries[pi].us > 5000.0) ? IM_COL32(255, 90, 90, 255) :
                            (profEntries[pi].us > 1000.0) ? IM_COL32(255, 200, 80, 255) :
                                                            IM_COL32(120, 220, 140, 255);
                snprintf(profBuf, sizeof(profBuf), XC("%-12s %6.0f us   peak %6.0f"),
                    profEntries[pi].name, profEntries[pi].us, s_profPeak[pi]);
                dl->AddText(profFont, fs, ImVec2(profX + pad + 1, y + 1), IM_COL32(0, 0, 0, 200), profBuf);
                dl->AddText(profFont, fs, ImVec2(profX + pad, y), col, profBuf);
                y += lineH;
            }
        }
    }

    }
    catch (const std::exception& e)
    {

    }
    catch (...)
    {

    }
}

// Runs FrameSync off the Present thread so overlay FPS can track the game.
inline void LaunchFrameSyncWorker()
{
    CreateThread(nullptr, 0, [](LPVOID) -> DWORD {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_BELOW_NORMAL);
        for (;;) {
            try {
                FrameSync::Tick();
            } catch (...) {
            }
            Sleep(25);
        }
        return 0;
    }, nullptr, 0, nullptr);
}
