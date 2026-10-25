#include <Cheat.hpp>
#include "menu.hpp"
#include "settings/Settings.hpp"
#include "game/HudRender.hpp"

static CContainer* Section(CObject* parent, const char* title, const char* persistId)
{
	auto* box = parent->AddObject<CContainer>(title);
	box->SetPersistId(persistId);
	return box;
}

static bool XhOn() { return crosshairStyle != 0; }

static bool XhUsesGap()
{
	switch (crosshairStyle)
	{
	case 1: case 4: case 5: case 6: case 7: case 8: case 9:
	case 12: case 14: case 16: case 17: case 18: case 19:
	case 21: case 22: case 23: case 24: case 27: case 28: case 31: case 32:
		return true;
	default:
		return false;
	}
}

static bool XhUsesRing()
{
	switch (crosshairStyle)
	{
	case 3: case 13: case 14: case 15: case 20: case 23: case 25: case 26: case 28:
		return true;
	default:
		return false;
	}
}

static bool XhUsesBracket()
{
	return crosshairStyle == 12 || crosshairStyle == 21 || crosshairStyle == 27;
}

static bool XhUsesChevron()
{
	return crosshairStyle == 8 || crosshairStyle == 9;
}

static bool XhUsesSegments()
{
	return crosshairStyle == 13 || crosshairStyle == 20;
}

static bool XhUsesDotSize()
{
	if (crosshairCenterDot)
		return true;
	switch (crosshairStyle)
	{
	case 15: case 16: case 17: case 23: case 26: case 29: case 30:
		return true;
	default:
		return false;
	}
}

static bool XhUsesRounded()
{
	switch (crosshairStyle)
	{
	case 2: case 3: case 13: case 15: case 16: case 17: case 20: case 26: case 30:
		return false;
	default:
		return XhOn();
	}
}

class CCrosshairPreview : public CObject
{
public:
	CCrosshairPreview()
	{
		SetName(X("Preview"));
	}

	void Render() override
	{
		const float h = 132.0f;
		SetSize(Vector2{ GetSize().x, h });
		const Vector2 pos = GetPosition();
		const Vector2 size = GetSize();

		Render::PushClipRect(pos, size, true);

		const float cell = 8.0f;
		const int nx = (int)(size.x / cell) + 1;
		const int ny = (int)(size.y / cell) + 1;
		const Color dark(36, 38, 44);
		const Color light(200, 202, 208);
		for (int y = 0; y < ny; ++y)
		{
			for (int x = 0; x < nx; ++x)
			{
				const Color& c = ((x + y) & 1) ? light : dark;
				Render::AddRectFilled(pos + Vector2{ (float)x * cell, (float)y * cell }, Vector2{ cell, cell }, c);
			}
		}

		if (crosshairStyle == 0)
		{
			Render::AddText(X("Crosshair Disabled"), pos + size * 0.5f, Style::dimmedText, TEXT_FLAG_NONE, { 0.5f, 0.5f });
		}
		else if (Render::drawList)
		{
			const float cx = pos.x + size.x * 0.5f;
			const float cy = pos.y + size.y * 0.5f;
			const float fit = (std::min)(size.x, size.y) * 0.5f * 0.82f;
			Hud::DrawCrosshairAt(Render::drawList, cx, cy, fit);
			if (crosshairPreviewCenterMark)
			{
				const Color mark(255, 80, 200);
				Render::AddLine(Vector2{ cx - 5.0f, cy }, Vector2{ cx + 5.0f, cy }, mark, 1.0f);
				Render::AddLine(Vector2{ cx, cy - 5.0f }, Vector2{ cx, cy + 5.0f }, mark, 1.0f);
			}
		}

		Render::PopClipRect();
		Render::AddRect(pos, size, Color(255, 255, 255, 28), 4.0f, 1.0f);
	}
};

void Menu::InitHudTab(CTab* tab)
{
	if (auto Box = Section(tab, X("Crosshair - Preset"), X("hud.xhair.preset")))
	{
		Box->AddObject<CDropdown>(X("Style"), &crosshairStyle,
			std::vector<std::string>{
				X("Disabled"), X("Cross"), X("Dot"), X("Circle"), X("Cross + Dot"),
				X("T Shape"), X("Inverted T"), X("X Cross"), X("Chevron Up"), X("Chevron Down"),
				X("Diamond"), X("Hollow Square"), X("Corner Brackets"), X("Segmented Circle"),
				X("Cross + Circle"), X("Dot + Ring"), X("Triple Dot Horizontal"), X("Triple Dot Vertical"),
				X("Horizontal Line"), X("Vertical Line"), X("Shotgun Ring"), X("Four Corners"),
				X("Cross + Diamond"), X("Cross + Ring + Dot"), X("Open X"), X("Double Circle"),
				X("Circle + Four Dots"), X("Tactical Brackets"), X("Small Cross + Outer Ring"),
				X("Diamond + Center Dot"), X("Four Direction Dots"), X("Cross + Diagonal Accents"),
				X("Minimal Center Plus")
			});
		Box->AddObject<CCrosshairPreview>();
		Box->AddObject<CColorPicker>(X("Color"), &Menu::colCrosshair, true);
		Box->AddObject<CCheckbox>(X("Outline"), &crosshairOutline,
			X("Border so the reticle stays readable on bright ground"));
		auto oc = Box->AddObject<CColorPicker>(X("Outline Color"), &Menu::colCrosshairOutline, true);
		oc->SetVisibleCondition([]() { return XhOn() && crosshairOutline; });
		Box->AddObject<CCheckbox>(X("Use Accent Color"), &crosshairUseAccent,
			X("Second color on rings, extra dots, and diagonal accents"));
		auto ac = Box->AddObject<CColorPicker>(X("Accent Color"), &Menu::colCrosshairAccent, true);
		ac->SetVisibleCondition([]() { return XhOn() && crosshairUseAccent; });
		Box->AddObject<CCheckbox>(X("Center Dot"), &crosshairCenterDot,
			X("Adds a center disc on styles that do not already have one"));
		Box->AddObject<CCheckbox>(X("Preview Center Mark"), &crosshairPreviewCenterMark,
			X("Magenta marker in the preview only. Not drawn in-game and not saved."));
		Box->AddObject<CConfirmationButton>(X("Reset Crosshair Defaults"),
			[]() {
				ResetCrosshairDefaults();
				Menu::SyncFromSettings();
			},
			[]() {},
			X("Reset all crosshair settings? Display HUD options are not changed."));
	}

	if (auto Box = Section(tab, X("Crosshair - Geometry"), X("hud.xhair.geom")))
	{
		Box->SetVisibleCondition([]() { return XhOn(); });
		Box->AddObject<CSliderFloat>(X("Size"), &crosshairSize, 2.f, 40.f, X("{:.0f}"));
		auto gap = Box->AddObject<CSliderFloat>(X("Gap"), &crosshairGap, 0.f, 20.f, X("{:.0f}"));
		gap->SetVisibleCondition([]() { return XhUsesGap(); });
		Box->AddObject<CSliderFloat>(X("Thickness"), &crosshairThickness, 1.f, 6.f, X("{:.1f}"));
		Box->AddObject<CSliderFloat>(X("Rotation"), &crosshairRotation, -180.f, 180.f, X("{:.0f}"));
		Box->AddObject<CSliderFloat>(X("Horizontal Scale"), &crosshairHorizontalScale, 0.50f, 2.00f, X("{:.2f}"));
		Box->AddObject<CSliderFloat>(X("Vertical Scale"), &crosshairVerticalScale, 0.50f, 2.00f, X("{:.2f}"));
		auto ds = Box->AddObject<CSliderFloat>(X("Dot Size"), &crosshairDotSize, 1.f, 8.f, X("{:.1f}"));
		ds->SetVisibleCondition([]() { return XhUsesDotSize(); });
		auto rs = Box->AddObject<CSliderFloat>(X("Ring Scale"), &crosshairRingScale, 0.50f, 2.00f, X("{:.2f}"));
		rs->SetVisibleCondition([]() { return XhUsesRing(); });
	}

	if (auto Box = Section(tab, X("Crosshair - Style Options"), X("hud.xhair.opts")))
	{
		Box->SetVisibleCondition([]() { return XhOn(); });
		auto rd = Box->AddObject<CCheckbox>(X("Rounded Endpoints"), &crosshairRounded,
			X("Filled caps on line ends. Off for Batch 1 styles at default rotation."));
		rd->SetVisibleCondition([]() { return XhUsesRounded(); });
		auto bl = Box->AddObject<CSliderFloat>(X("Bracket Length"), &crosshairBracketLength, 2.f, 30.f, X("{:.0f}"));
		bl->SetVisibleCondition([]() { return XhUsesBracket(); });
		auto ca = Box->AddObject<CSliderFloat>(X("Chevron Angle"), &crosshairChevronAngle, 20.f, 80.f, X("{:.0f}"));
		ca->SetVisibleCondition([]() { return XhUsesChevron(); });
		auto sc = Box->AddObject<CSliderInt>(X("Segment Count"), &crosshairSegmentCount, 3, 12, X("{}"));
		sc->SetVisibleCondition([]() { return XhUsesSegments(); });
		auto sg = Box->AddObject<CSliderFloat>(X("Segment Gap"), &crosshairSegmentGap, 2.f, 35.f, X("{:.0f}"));
		sg->SetVisibleCondition([]() { return XhUsesSegments(); });
	}

	if (auto Box = Section(tab, X("Crosshair - Animation"), X("hud.xhair.anim")))
	{
		Box->SetVisibleCondition([]() { return XhOn(); });
		Box->AddObject<CCheckbox>(X("Pulse"), &crosshairPulse,
			X("Scales the reticle with time. Does not read game state."));
		auto ps = Box->AddObject<CSliderFloat>(X("Pulse Speed"), &crosshairPulseSpeed, 0.25f, 5.0f, X("{:.2f}"));
		ps->SetVisibleCondition([]() { return crosshairPulse; });
		auto pa = Box->AddObject<CSliderFloat>(X("Pulse Amount"), &crosshairPulseAmount, 0.f, 10.f, X("{:.1f}"));
		pa->SetVisibleCondition([]() { return crosshairPulse; });
	}

	if (auto Box = Section(tab, X("Display"), X("hud.display")))
	{
		Box->AddObject<CCheckbox>(X("Overlay FPS"), &hudFpsCounter,
			X("Framerate of this overlay, not the game's"));
		Box->AddObject<CCheckbox>(X("CPU Usage"), &hudCpuUsage);
		Box->AddObject<CCheckbox>(X("GPU Usage"), &hudGpuUsage);
		Box->AddObject<CCheckbox>(X("Screen Resolution"), &hudResolution);
		Box->AddObject<CCheckbox>(X("Current Time"), &hudClock);
		Box->AddObject<CCheckbox>(X("24 Hour Clock"), &hudClock24h);
		Box->AddObject<CRadio>(X("Corner"), &hudCorner,
			std::vector<std::string>{ X("Top L"), X("Top R"), X("Bot L"), X("Bot R") });
		Box->AddObject<CSliderFloat>(X("Scale"), &hudScale, 0.5f, 2.0f, X("{:.2f}"));
		Box->AddObject<CCheckbox>(X("Background Panel"), &hudBackground);
		Box->AddObject<CColorPicker>(X("Color"), &Menu::colHud, true);
	}
}
