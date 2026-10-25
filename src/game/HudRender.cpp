#include <Cheat.hpp>
#include "HudRender.hpp"
#include "main.hpp"
#include "settings/Settings.hpp"

#include <cfloat>
#include <cmath>
#include <ctime>
#include <pdh.h>
#include <pdhmsg.h>
#include <vector>

#pragma comment(lib, "pdh.lib")

namespace
{
	ImU32 ToU32(const float c[4])
	{
		return IM_COL32(
			(int)(std::clamp(c[0], 0.0f, 1.0f) * 255.0f),
			(int)(std::clamp(c[1], 0.0f, 1.0f) * 255.0f),
			(int)(std::clamp(c[2], 0.0f, 1.0f) * 255.0f),
			(int)(std::clamp(c[3], 0.0f, 1.0f) * 255.0f));
	}

	ULARGE_INTEGER FileTimeToU64(const FILETIME& ft)
	{
		ULARGE_INTEGER u;
		u.LowPart = ft.dwLowDateTime;
		u.HighPart = ft.dwHighDateTime;
		return u;
	}

	float g_fpsSmoothed = 0.0f;
	float g_fpsDisplay = 0.0f;
	float g_fpsAccum = 0.0f;
	float g_cpuDisplay = 0.0f;
	float g_gpuDisplay = -1.0f;

	void UpdateFps(float dt)
	{
		if (!(dt > 0.0f) || !(dt < 1.0f))
			return;

		const float inst = 1.0f / dt;
		g_fpsSmoothed = (g_fpsSmoothed <= 0.0f) ? inst : g_fpsSmoothed + (inst - g_fpsSmoothed) * 0.08f;

		g_fpsAccum += dt;
		if (g_fpsAccum >= 0.25f)
		{
			g_fpsAccum = 0.0f;
			g_fpsDisplay = g_fpsSmoothed;
		}
	}

	void UpdateCpu(uint64_t nowMs)
	{
		static uint64_t s_lastMs = 0;
		static ULARGE_INTEGER s_idle{}, s_kernel{}, s_user{};
		static bool s_have = false;

		if (s_have && (nowMs - s_lastMs) < 400)
			return;

		FILETIME idle{}, kernel{}, user{};
		if (!GetSystemTimes(&idle, &kernel, &user))
			return;

		const ULARGE_INTEGER i = FileTimeToU64(idle);
		const ULARGE_INTEGER k = FileTimeToU64(kernel);
		const ULARGE_INTEGER u = FileTimeToU64(user);

		if (s_have)
		{
			const ULONGLONG di = i.QuadPart - s_idle.QuadPart;
			const ULONGLONG dk = k.QuadPart - s_kernel.QuadPart;
			const ULONGLONG du = u.QuadPart - s_user.QuadPart;
			const ULONGLONG total = dk + du;
			if (total > 0)
				g_cpuDisplay = (float)std::clamp(100.0 * (1.0 - (double)di / (double)total), 0.0, 100.0);
		}

		s_idle = i;
		s_kernel = k;
		s_user = u;
		s_have = true;
		s_lastMs = nowMs;
	}

	void UpdateGpu(uint64_t nowMs)
	{
		static PDH_HQUERY s_query = nullptr;
		static PDH_HCOUNTER s_counter = nullptr;
		static int s_state = 0; // 0 untried, 1 live, -1 failed
		static uint64_t s_lastMs = 0;
		static std::vector<uint8_t> s_buf;

		if (s_state < 0)
			return;
		if (s_state > 0 && (nowMs - s_lastMs) < 500)
			return;

		if (s_state == 0)
		{
			if (PdhOpenQueryW(nullptr, 0, &s_query) != ERROR_SUCCESS)
			{
				s_state = -1;
				return;
			}
			if (PdhAddEnglishCounterW(s_query, L"\\GPU Engine(*)\\Utilization Percentage", 0, &s_counter) != ERROR_SUCCESS)
			{
				PdhCloseQuery(s_query);
				s_query = nullptr;
				s_state = -1;
				return;
			}
			PdhCollectQueryData(s_query);
			s_state = 1;
			s_lastMs = nowMs;
			return;
		}

		if (PdhCollectQueryData(s_query) != ERROR_SUCCESS)
			return;

		DWORD bufSize = 0;
		DWORD itemCount = 0;
		PDH_STATUS st = PdhGetFormattedCounterArrayW(s_counter, PDH_FMT_DOUBLE, &bufSize, &itemCount, nullptr);
		if (st != PDH_MORE_DATA || bufSize == 0)
			return;

		s_buf.resize(bufSize);
		auto* items = reinterpret_cast<PDH_FMT_COUNTERVALUE_ITEM_W*>(s_buf.data());
		itemCount = 0;
		if (PdhGetFormattedCounterArrayW(s_counter, PDH_FMT_DOUBLE, &bufSize, &itemCount, items) != ERROR_SUCCESS)
			return;

		double sum3d = 0.0;
		double maxAny = 0.0;
		int n3d = 0;
		for (DWORD i = 0; i < itemCount; ++i)
		{
			if (items[i].FmtValue.CStatus != ERROR_SUCCESS)
				continue;
			const double v = items[i].FmtValue.doubleValue;
			maxAny = (std::max)(maxAny, v);
			const wchar_t* name = items[i].szName;
			if (name && (wcsstr(name, L"engtype_3D") || wcsstr(name, L"engtype_Graphics")))
			{
				sum3d += v;
				++n3d;
			}
		}

		const double raw = (n3d > 0) ? sum3d : maxAny;
		g_gpuDisplay = (float)std::clamp(raw, 0.0, 100.0);
		s_lastMs = nowMs;
	}

	float Sane(float v, float fallback)
	{
		return std::isfinite(v) ? v : fallback;
	}

	constexpr float kPi = 3.14159265358979323846f;
	constexpr float kTau = 6.28318530717958647692f;

	struct XhairXform
	{
		ImVec2 center{};
		float hs = 1.f, vs = 1.f, ang = 0.f, fit = 1.f;
		float thick = 1.5f, ot = 1.f;
		bool outline = true, rounded = false;
		ImU32 col = 0, accent = 0, outCol = 0;

		ImVec2 xf(float x, float y) const
		{
			x *= hs;
			y *= vs;
			const float c = cosf(ang), s = sinf(ang);
			const float rx = x * c - y * s;
			const float ry = x * s + y * c;
			return ImVec2(center.x + rx * fit, center.y + ry * fit);
		}

		float stroke(bool outlinePass) const
		{
			return outlinePass ? (thick + 2.0f * ot) : thick;
		}
	};

	void StrokeLine(ImDrawList* dl, const XhairXform& x, bool outlinePass, float x0, float y0, float x1, float y1, ImU32 col)
	{
		if (outlinePass && !x.outline)
			return;
		const ImVec2 a = x.xf(x0, y0);
		const ImVec2 b = x.xf(x1, y1);
		const ImU32 c = outlinePass ? x.outCol : col;
		const float th = x.stroke(outlinePass);
		dl->AddLine(a, b, c, th);
		if (x.rounded)
		{
			const float r = th * 0.5f;
			dl->AddCircleFilled(a, r, c, 12);
			dl->AddCircleFilled(b, r, c, 12);
		}
	}

	void StrokePoly(ImDrawList* dl, ImVec2* pts, int n, bool closed, ImU32 col, float th)
	{
		if (n < 2)
			return;
		dl->AddPolyline(pts, n, col, closed ? ImDrawFlags_Closed : 0, th);
	}

	void StrokeRing(ImDrawList* dl, const XhairXform& x, bool outlinePass, float r, ImU32 col)
	{
		if (outlinePass && !x.outline)
			return;
		constexpr int N = 48;
		ImVec2 p[N];
		for (int i = 0; i < N; ++i)
		{
			const float a = kTau * (float)i / (float)N;
			p[i] = x.xf(cosf(a) * r, sinf(a) * r);
		}
		StrokePoly(dl, p, N, true, outlinePass ? x.outCol : col, x.stroke(outlinePass));
	}

	void StrokeArcSegs(ImDrawList* dl, const XhairXform& x, bool outlinePass, float r, int segs, float gapDeg, ImU32 col)
	{
		if (outlinePass && !x.outline)
			return;
		segs = std::clamp(segs, 3, 12);
		float gap = gapDeg * (kPi / 180.0f);
		float totalGap = gap * (float)segs;
		float remain = kTau - totalGap;
		if (remain < 0.4f)
		{
			gap = (kTau - 0.4f) / (float)segs;
			remain = kTau - gap * (float)segs;
		}
		const float arc = remain / (float)segs;
		constexpr int S = 16;
		ImVec2 pts[S];
		const ImU32 c = outlinePass ? x.outCol : col;
		const float th = x.stroke(outlinePass);
		for (int i = 0; i < segs; ++i)
		{
			const float a0 = (float)i * (arc + gap) + gap * 0.5f;
			for (int s = 0; s < S; ++s)
			{
				const float t = a0 + arc * ((float)s / (float)(S - 1));
				pts[s] = x.xf(cosf(t) * r, sinf(t) * r);
			}
			StrokePoly(dl, pts, S, false, c, th);
		}
	}

	void FillDisc(ImDrawList* dl, const XhairXform& x, bool outlinePass, float lx, float ly, float r, ImU32 col)
	{
		const ImVec2 p = x.xf(lx, ly);
		const float rr = r * x.fit;
		if (outlinePass)
		{
			if (!x.outline)
				return;
			dl->AddCircleFilled(p, rr + 1.5f * x.ot * x.fit, x.outCol, 16);
		}
		else
		{
			dl->AddCircleFilled(p, (std::max)(0.5f, rr), col, 16);
		}
	}

	void DrawCrossArms(ImDrawList* dl, const XhairXform& x, bool outlinePass, float size, float gap, ImU32 col)
	{
		StrokeLine(dl, x, outlinePass, -gap - size, 0.f, -gap, 0.f, col);
		StrokeLine(dl, x, outlinePass, gap, 0.f, gap + size, 0.f, col);
		StrokeLine(dl, x, outlinePass, 0.f, -gap - size, 0.f, -gap, col);
		StrokeLine(dl, x, outlinePass, 0.f, gap, 0.f, gap + size, col);
	}

	void DrawXArms(ImDrawList* dl, const XhairXform& x, bool outlinePass, float size, float gap, ImU32 col)
	{
		const float s = size * 0.70710678f;
		const float g = gap * 0.70710678f;
		StrokeLine(dl, x, outlinePass, -g - s, -g - s, -g, -g, col);
		StrokeLine(dl, x, outlinePass, g, g, g + s, g + s, col);
		StrokeLine(dl, x, outlinePass, -g - s, g + s, -g, g, col);
		StrokeLine(dl, x, outlinePass, g, -g, g + s, -g - s, col);
	}

	void DrawDiamond(ImDrawList* dl, const XhairXform& x, bool outlinePass, float size, ImU32 col)
	{
		StrokeLine(dl, x, outlinePass, 0.f, -size, size, 0.f, col);
		StrokeLine(dl, x, outlinePass, size, 0.f, 0.f, size, col);
		StrokeLine(dl, x, outlinePass, 0.f, size, -size, 0.f, col);
		StrokeLine(dl, x, outlinePass, -size, 0.f, 0.f, -size, col);
	}

	void DrawSquare(ImDrawList* dl, const XhairXform& x, bool outlinePass, float size, ImU32 col)
	{
		StrokeLine(dl, x, outlinePass, -size, -size, size, -size, col);
		StrokeLine(dl, x, outlinePass, size, -size, size, size, col);
		StrokeLine(dl, x, outlinePass, size, size, -size, size, col);
		StrokeLine(dl, x, outlinePass, -size, size, -size, -size, col);
	}

	void DrawBrackets(ImDrawList* dl, const XhairXform& x, bool outlinePass, float box, float hLen, float vLen, ImU32 col)
	{
		const float hx[2] = { -box, box };
		const float hy[2] = { -box, box };
		const float hSign[2] = { 1.f, -1.f };
		const float vSign[2] = { 1.f, -1.f };
		for (int i = 0; i < 2; ++i)
		{
			for (int j = 0; j < 2; ++j)
			{
				const float px = hx[i];
				const float py = hy[j];
				StrokeLine(dl, x, outlinePass, px, py, px + hSign[i] * hLen, py, col);
				StrokeLine(dl, x, outlinePass, px, py, px, py + vSign[j] * vLen, col);
			}
		}
	}

	void DrawChevron(ImDrawList* dl, const XhairXform& x, bool outlinePass, float size, float gap, float angleDeg, bool up, ImU32 col)
	{
		const float a = std::clamp(angleDeg, 20.0f, 80.0f) * (kPi / 180.0f);
		const float peakY = up ? (-gap - size * 0.15f) : (gap + size * 0.15f);
		const float dir = up ? 1.f : -1.f;
		const float ax = size * cosf(a);
		const float ay = size * sinf(a) * dir;
		StrokeLine(dl, x, outlinePass, 0.f, peakY, -ax, peakY + ay, col);
		StrokeLine(dl, x, outlinePass, 0.f, peakY, ax, peakY + ay, col);
	}

	void DrawLegacyStyles14(ImDrawList* dl, int style, float cx, float cy, float size, float gap, float thick, bool outline, ImU32 col, ImU32 outCol, float ot)
	{
		const float armY = (thick < 2.0f) ? (floorf(cy) + 0.5f) : cy;
		const float armX = (thick < 2.0f) ? (floorf(cx) + 0.5f) : cx;
		const bool wantCross = (style == 1 || style == 4);
		const bool wantDot = (style == 2 || style == 4);
		const bool wantCircle = (style == 3);
		const float lineOt = thick + 2.0f * ot;

		if (wantCross)
		{
			const ImVec2 a0(cx - gap - size, armY), a1(cx - gap, armY);
			const ImVec2 b0(cx + gap, armY), b1(cx + gap + size, armY);
			const ImVec2 c0(armX, cy - gap - size), c1(armX, cy - gap);
			const ImVec2 d0(armX, cy + gap), d1(armX, cy + gap + size);
			if (outline)
			{
				dl->AddLine(a0, a1, outCol, lineOt);
				dl->AddLine(b0, b1, outCol, lineOt);
				dl->AddLine(c0, c1, outCol, lineOt);
				dl->AddLine(d0, d1, outCol, lineOt);
			}
			dl->AddLine(a0, a1, col, thick);
			dl->AddLine(b0, b1, col, thick);
			dl->AddLine(c0, c1, col, thick);
			dl->AddLine(d0, d1, col, thick);
		}

		if (wantCircle)
		{
			if (outline)
				dl->AddCircle(ImVec2(cx, cy), size, outCol, 0, lineOt);
			dl->AddCircle(ImVec2(cx, cy), size, col, 0, thick);
		}

		if (wantDot)
		{
			const float r = (std::max)(1.0f, thick);
			if (outline)
				dl->AddCircleFilled(ImVec2(cx, cy), r + 1.5f * ot, outCol, 16);
			dl->AddCircleFilled(ImVec2(cx, cy), r, col, 16);
		}
	}

	bool StyleHasInherentCenterDot(int style)
	{
		return style == 2 || style == 4 || style == 15 || style == 23 || style == 29;
	}

	void DrawXhairStyle(ImDrawList* dl, const XhairXform& x, bool outlinePass, int style,
		float size, float gap, float ringR, float bracket, float chev, int segs, float segGap, float dotR, ImU32 primary, ImU32 accent)
	{
		switch (style)
		{
		case 1:
			DrawCrossArms(dl, x, outlinePass, size, gap, primary);
			break;
		case 2:
			FillDisc(dl, x, outlinePass, 0.f, 0.f, (std::max)(1.0f, x.thick), primary);
			break;
		case 3:
			StrokeRing(dl, x, outlinePass, ringR, accent);
			break;
		case 4:
			DrawCrossArms(dl, x, outlinePass, size, gap, primary);
			FillDisc(dl, x, outlinePass, 0.f, 0.f, (std::max)(1.0f, x.thick), primary);
			break;
		case 5:
			StrokeLine(dl, x, outlinePass, -size, -gap, size, -gap, primary);
			StrokeLine(dl, x, outlinePass, 0.f, -gap, 0.f, size, primary);
			break;
		case 6:
			StrokeLine(dl, x, outlinePass, -size, gap, size, gap, primary);
			StrokeLine(dl, x, outlinePass, 0.f, -size, 0.f, gap, primary);
			break;
		case 7:
			DrawXArms(dl, x, outlinePass, size, gap, primary);
			break;
		case 8:
			DrawChevron(dl, x, outlinePass, size, gap, chev, true, primary);
			break;
		case 9:
			DrawChevron(dl, x, outlinePass, size, gap, chev, false, primary);
			break;
		case 10:
			DrawDiamond(dl, x, outlinePass, size, primary);
			break;
		case 11:
			DrawSquare(dl, x, outlinePass, size, primary);
			break;
		case 12:
			DrawBrackets(dl, x, outlinePass, size + gap, bracket, bracket, primary);
			break;
		case 13:
			StrokeArcSegs(dl, x, outlinePass, ringR, segs, segGap, accent);
			break;
		case 14:
			DrawCrossArms(dl, x, outlinePass, size, gap, primary);
			StrokeRing(dl, x, outlinePass, ringR, accent);
			break;
		case 15:
			StrokeRing(dl, x, outlinePass, ringR, accent);
			FillDisc(dl, x, outlinePass, 0.f, 0.f, dotR, primary);
			break;
		case 16:
			FillDisc(dl, x, outlinePass, -(gap + size * 0.5f), 0.f, dotR, accent);
			FillDisc(dl, x, outlinePass, 0.f, 0.f, dotR, primary);
			FillDisc(dl, x, outlinePass, gap + size * 0.5f, 0.f, dotR, accent);
			break;
		case 17:
			FillDisc(dl, x, outlinePass, 0.f, -(gap + size * 0.5f), dotR, accent);
			FillDisc(dl, x, outlinePass, 0.f, 0.f, dotR, primary);
			FillDisc(dl, x, outlinePass, 0.f, gap + size * 0.5f, dotR, accent);
			break;
		case 18:
			StrokeLine(dl, x, outlinePass, -gap - size, 0.f, -gap, 0.f, primary);
			StrokeLine(dl, x, outlinePass, gap, 0.f, gap + size, 0.f, primary);
			break;
		case 19:
			StrokeLine(dl, x, outlinePass, 0.f, -gap - size, 0.f, -gap, primary);
			StrokeLine(dl, x, outlinePass, 0.f, gap, 0.f, gap + size, primary);
			break;
		case 20:
		{
			const int n = std::clamp(segs, 3, 12);
			const float tick = (std::max)(2.0f, size * 0.22f);
			for (int i = 0; i < n; ++i)
			{
				const float a = kTau * (float)i / (float)n;
				const float c = cosf(a), s = sinf(a);
				StrokeLine(dl, x, outlinePass, c * (ringR - tick), s * (ringR - tick), c * ringR, s * ringR, accent);
			}
			break;
		}
		case 21:
		{
			const float box = size + gap;
			const float len = (std::max)(2.0f, bracket * 0.45f);
			StrokeLine(dl, x, outlinePass, -box, -box, -box + len, -box, primary);
			StrokeLine(dl, x, outlinePass, box - len, -box, box, -box, primary);
			StrokeLine(dl, x, outlinePass, -box, box, -box + len, box, primary);
			StrokeLine(dl, x, outlinePass, box - len, box, box, box, primary);
			break;
		}
		case 22:
			DrawCrossArms(dl, x, outlinePass, size, gap, primary);
			DrawDiamond(dl, x, outlinePass, size * 0.55f, accent);
			break;
		case 23:
			DrawCrossArms(dl, x, outlinePass, size, gap, primary);
			StrokeRing(dl, x, outlinePass, ringR, accent);
			FillDisc(dl, x, outlinePass, 0.f, 0.f, dotR, primary);
			break;
		case 24:
			DrawXArms(dl, x, outlinePass, size, (std::max)(gap, size * 0.28f), primary);
			break;
		case 25:
			StrokeRing(dl, x, outlinePass, ringR * 0.62f, primary);
			StrokeRing(dl, x, outlinePass, ringR, accent);
			break;
		case 26:
			StrokeRing(dl, x, outlinePass, ringR, primary);
			FillDisc(dl, x, outlinePass, 0.f, -ringR, dotR, accent);
			FillDisc(dl, x, outlinePass, 0.f, ringR, dotR, accent);
			FillDisc(dl, x, outlinePass, -ringR, 0.f, dotR, accent);
			FillDisc(dl, x, outlinePass, ringR, 0.f, dotR, accent);
			break;
		case 27:
			DrawBrackets(dl, x, outlinePass, size + gap, bracket, bracket * 0.55f, primary);
			break;
		case 28:
			DrawCrossArms(dl, x, outlinePass, (std::max)(2.0f, size * 0.45f), (std::max)(0.0f, gap * 0.5f), primary);
			StrokeRing(dl, x, outlinePass, ringR, accent);
			break;
		case 29:
			DrawDiamond(dl, x, outlinePass, size, primary);
			FillDisc(dl, x, outlinePass, 0.f, 0.f, dotR, accent);
			break;
		case 30:
		{
			const float d = gap + size * 0.5f;
			FillDisc(dl, x, outlinePass, 0.f, -d, dotR, accent);
			FillDisc(dl, x, outlinePass, 0.f, d, dotR, accent);
			FillDisc(dl, x, outlinePass, -d, 0.f, dotR, accent);
			FillDisc(dl, x, outlinePass, d, 0.f, dotR, accent);
			break;
		}
		case 31:
		{
			DrawCrossArms(dl, x, outlinePass, size, gap, primary);
			const float s = size * 0.38f;
			const float o = size * 0.72f;
			StrokeLine(dl, x, outlinePass, -o, -o, -o + s, -o + s, accent);
			StrokeLine(dl, x, outlinePass, o - s, o - s, o, o, accent);
			StrokeLine(dl, x, outlinePass, -o, o, -o + s, o - s, accent);
			StrokeLine(dl, x, outlinePass, o - s, -o + s, o, -o, accent);
			break;
		}
		case 32:
		{
			const float arm = (std::max)(2.0f, size * 0.28f);
			const float g = (std::max)(1.0f, gap * 0.35f);
			DrawCrossArms(dl, x, outlinePass, arm, g, primary);
			break;
		}
		default:
			break;
		}
	}

	void DrawCrosshairAtImpl(ImDrawList* dl, float cx, float cy, float fitRadius)
	{
		if (!dl)
			return;
		if (!std::isfinite(cx) || !std::isfinite(cy))
			return;

		const int style = std::clamp((int)Sane((float)crosshairStyle, 0.f), 0, 32);
		if (style == 0)
			return;

		if (col_crosshair[3] <= 0.0f)
			return;

		float size = std::clamp(Sane(crosshairSize, 10.0f), 2.0f, 40.0f);
		const float gap = std::clamp(Sane(crosshairGap, 4.0f), 0.0f, 20.0f);
		const float thick = std::clamp(Sane(crosshairThickness, 1.5f), 1.0f, 6.0f);
		const float rot = std::clamp(Sane(crosshairRotation, 0.0f), -180.0f, 180.0f);
		const float hs = std::clamp(Sane(crosshairHorizontalScale, 1.0f), 0.50f, 2.00f);
		const float vs = std::clamp(Sane(crosshairVerticalScale, 1.0f), 0.50f, 2.00f);
		const float ot = std::clamp(Sane(crosshairOutlineThickness, 1.0f), 0.5f, 4.0f);
		const float ringScale = std::clamp(Sane(crosshairRingScale, 1.0f), 0.50f, 2.00f);
		const float bracket = std::clamp(Sane(crosshairBracketLength, 8.0f), 2.0f, 30.0f);
		const float chev = std::clamp(Sane(crosshairChevronAngle, 45.0f), 20.0f, 80.0f);
		const int segs = std::clamp((int)Sane((float)crosshairSegmentCount, 4.f), 3, 12);
		const float segGap = std::clamp(Sane(crosshairSegmentGap, 12.0f), 2.0f, 35.0f);
		const float dotR = std::clamp(Sane(crosshairDotSize, 2.0f), 1.0f, 8.0f);
		const float pulseSpd = std::clamp(Sane(crosshairPulseSpeed, 1.5f), 0.25f, 5.0f);
		const float pulseAmt = std::clamp(Sane(crosshairPulseAmount, 2.0f), 0.0f, 10.0f);
		const bool outline = crosshairOutline;
		const bool rounded = crosshairRounded;
		const bool centerDot = crosshairCenterDot;
		const bool useAccent = crosshairUseAccent;
		const bool pulseOn = crosshairPulse && pulseAmt > 0.0f;

		if (pulseOn)
			size += sinf((float)ImGui::GetTime() * pulseSpd * kTau) * pulseAmt;

		size = std::clamp(size, 1.0f, 60.0f);

		const ImU32 col = ToU32(col_crosshair);
		const ImU32 outCol = ToU32(col_crosshair_outline);
		const ImU32 accent = useAccent ? ToU32(col_crosshair_accent) : col;

		const bool identity =
			style >= 1 && style <= 4 &&
			fabsf(rot) < 0.01f &&
			fabsf(hs - 1.0f) < 0.001f &&
			fabsf(vs - 1.0f) < 0.001f &&
			!rounded &&
			!useAccent &&
			!pulseOn &&
			(!centerDot || StyleHasInherentCenterDot(style)) &&
			!(fitRadius > 0.0f);

		if (identity)
		{
			DrawLegacyStyles14(dl, style, cx, cy, std::clamp(Sane(crosshairSize, 10.0f), 2.0f, 40.0f),
				gap, thick, outline, col, outCol, ot);
			return;
		}

		float extent = size * (std::max)(hs, vs) + gap + 4.0f;
		extent = (std::max)(extent, size * ringScale * (std::max)(hs, vs));
		extent = (std::max)(extent, bracket + size + gap);
		extent += pulseOn ? pulseAmt : 0.0f;
		extent += dotR;
		extent *= 1.15f;

		float fit = 1.0f;
		if (fitRadius > 0.0f && extent > fitRadius && extent > 0.01f)
			fit = fitRadius / extent;

		XhairXform x;
		x.center = ImVec2(cx, cy);
		x.hs = hs;
		x.vs = vs;
		x.ang = rot * (kPi / 180.0f);
		x.fit = fit;
		x.thick = thick;
		x.ot = ot;
		x.outline = outline;
		x.rounded = rounded;
		x.col = col;
		x.accent = accent;
		x.outCol = outCol;

		const float ringR = size * ringScale;
		const int firstPass = outline ? 0 : 1;
		for (int pass = firstPass; pass < 2; ++pass)
		{
			const bool outlinePass = (pass == 0);
			DrawXhairStyle(dl, x, outlinePass, style, size, gap, ringR, bracket, chev, segs, segGap, dotR, col, accent);
			if (centerDot && !StyleHasInherentCenterDot(style))
				FillDisc(dl, x, outlinePass, 0.f, 0.f, dotR, col);
		}
	}

	const char* ClockText()
	{
		static char s_buf[40] = {};
		static uint64_t s_lastMs = 0;
		static bool s_last24h = true;

		const uint64_t now = GetTickCount64();
		if (s_buf[0] != '\0' && (now - s_lastMs) < 1000 && s_last24h == hudClock24h)
			return s_buf;

		s_lastMs = now;
		s_last24h = hudClock24h;

		const time_t t = time(nullptr);
		tm lt{};
		if (localtime_s(&lt, &t) != 0)
		{
			s_buf[0] = '\0';
			return s_buf;
		}

		const char* fmt = hudClock24h ? XC("%H:%M:%S") : XC("%I:%M:%S %p");
		if (strftime(s_buf, sizeof(s_buf), fmt, &lt) == 0)
			s_buf[0] = '\0';
		return s_buf;
	}

	void DrawInfoPanel(ImDrawList* dl, float vw, float vh)
	{
		const bool wantStats = hudFpsCounter || hudCpuUsage || hudGpuUsage;
		if (!wantStats && !hudResolution && !hudClock)
			return;
		if (f.custom.empty())
			return;

		ImFont* font = f.custom[(size_t)std::clamp(fontIndex, 0, (int)f.custom.size() - 1)];
		if (!font)
			return;

		const float scale = std::clamp(hudScale, 0.5f, 2.0f);
		const float fs = font->FontSize * scale;
		if (fs <= 0.0f)
			return;

		const uint64_t nowMs = GetTickCount64();
		if (hudFpsCounter)
			UpdateFps(ImGui::GetIO().DeltaTime);
		if (hudCpuUsage)
			UpdateCpu(nowMs);
		if (hudGpuUsage)
			UpdateGpu(nowMs);

		char rows[3][96];
		int rowCount = 0;

		if (wantStats)
		{
			char* out = rows[rowCount];
			int n = 0;
			const int cap = (int)sizeof(rows[0]);
			if (hudFpsCounter)
				n += snprintf(out + n, (size_t)(std::max)(0, cap - n), XC("Overlay FPS  %.0f"), g_fpsDisplay);
			if (hudCpuUsage)
				n += snprintf(out + n, (size_t)(std::max)(0, cap - n), n ? XC("   CPU  %.0f%%") : XC("CPU  %.0f%%"), g_cpuDisplay);
			if (hudGpuUsage)
			{
				if (g_gpuDisplay < 0.0f)
					n += snprintf(out + n, (size_t)(std::max)(0, cap - n), n ? XC("   GPU  --") : XC("GPU  --"));
				else
					n += snprintf(out + n, (size_t)(std::max)(0, cap - n), n ? XC("   GPU  %.0f%%") : XC("GPU  %.0f%%"), g_gpuDisplay);
			}
			(void)n;
			++rowCount;
		}
		if (hudResolution)
			snprintf(rows[rowCount++], sizeof(rows[0]), XC("%d x %d"), (int)vw, (int)vh);
		if (hudClock)
			snprintf(rows[rowCount++], sizeof(rows[0]), XC("%s"), ClockText());

		float maxW = 0.0f;
		float lineH = 0.0f;
		for (int i = 0; i < rowCount; ++i)
		{
			const ImVec2 sz = font->CalcTextSizeA(fs, FLT_MAX, 0.0f, rows[i]);
			maxW = (std::max)(maxW, sz.x);
			lineH = (std::max)(lineH, sz.y);
		}
		if (maxW <= 0.0f || lineH <= 0.0f)
			return;

		const float padX = 8.0f * scale;
		const float padY = 6.0f * scale;
		const float rowGap = 2.0f * scale;
		const float panelW = maxW + padX * 2.0f;
		const float panelH = (float)rowCount * lineH + (float)(rowCount - 1) * rowGap + padY * 2.0f;
		const float margin = 12.0f;

		float px = margin;
		float py = margin;
		switch (std::clamp(hudCorner, 0, 3))
		{
		case 0: px = margin;                    py = margin;                    break;
		case 1: px = vw - margin - panelW;      py = margin;                    break;
		case 2: px = margin;                    py = vh - margin - panelH;      break;
		default: px = vw - margin - panelW;     py = vh - margin - panelH;      break;
		}

		px = std::clamp(px, 0.0f, (std::max)(0.0f, vw - panelW));
		py = std::clamp(py, 0.0f, (std::max)(0.0f, vh - panelH));

		if (hudBackground)
		{
			dl->AddRectFilled(ImVec2(px, py), ImVec2(px + panelW, py + panelH), IM_COL32(18, 20, 24, 190), 4.0f);
			dl->AddRect(ImVec2(px, py), ImVec2(px + panelW, py + panelH), IM_COL32(255, 255, 255, 25), 4.0f, 0, 1.0f);
		}

		const ImU32 col = ToU32(col_hud);
		float ty = py + padY;
		for (int i = 0; i < rowCount; ++i)
		{
			dl->AddText(font, fs, ImVec2(px + padX + 1.0f, ty + 1.0f), IM_COL32(0, 0, 0, 180), rows[i]);
			dl->AddText(font, fs, ImVec2(px + padX, ty), col, rows[i]);
			ty += lineH + rowGap;
		}
	}
}

void Hud::DrawCrosshairAt(ImDrawList* dl, float cx, float cy, float fitRadius)
{
	DrawCrosshairAtImpl(dl, cx, cy, fitRadius);
}

void Hud::Draw(ImDrawList* dl)
{
	if (!dl)
		return;

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float vw = display.x;
	const float vh = display.y;
	if (vw < 64.0f || vh < 64.0f)
		return;

	Hud::DrawCrosshairAt(dl, vw * 0.5f, vh * 0.5f, 0.0f);
	DrawInfoPanel(dl, vw, vh);
}
