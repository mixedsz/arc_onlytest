#include <Cheat.hpp>
#include "menu.hpp"
#include "settings/Settings.hpp"
#include "settings/Types.h"
#include "main.hpp"
#include "app/AppWindow.hpp"
#include "gui/gui_colors.h"
#include "memory/interface/ctx.hpp"

static Color FromFloat4(const float c[4])
{
	return Color(
		static_cast<int>(std::clamp(c[0], 0.f, 1.f) * 255.f),
		static_cast<int>(std::clamp(c[1], 0.f, 1.f) * 255.f),
		static_cast<int>(std::clamp(c[2], 0.f, 1.f) * 255.f),
		static_cast<int>(std::clamp(c[3], 0.f, 1.f) * 255.f));
}

static void ToFloat4(const Color& col, float c[4])
{
	c[0] = col.r / 255.f;
	c[1] = col.g / 255.f;
	c[2] = col.b / 255.f;
	c[3] = col.a / 255.f;
}

static int s_aimFeelApplied = -1;
static int s_warnPresetSeen = -1;

void Menu::SyncFromSettings(bool seedBinds)
{
	// Keybind widgets own their value once the menu is up; settings are the
	// mirror. Pulling them back every sync used to overwrite a key the user had
	// just captured, because capture happens during Render on frames that are
	// mostly not sync frames - roughly six rebinds in seven were discarded.
	// Seeding is therefore limited to init and config load.
	if (seedBinds)
	{
		aimKeybind.key = AimCfg.aimKey;
		aimKeybind.mode = AimCfg.aimKeyMode;
		arcAimKeybind.key = ArcAimCfg.activationKey;
		arcAimKeybind.mode = ArcAimCfg.activationKeyMode;
		arcWeakspotKeybind.key = ArcAimCfg.weakspotKey;
		arcWeakspotKeybind.mode = ArcAimCfg.weakspotKeyMode;
		zoomKeybind.key = zoomChangerKey;
		zoomKeybind.mode = zoomChangerKeyMode;
		combatModeKeybind.key = combatModeKey;
		combatModeKeybind.mode = combatModeKeyMode;
	}

	colFov = FromFloat4(col_fov);
	colLine = FromFloat4(col_line);
	colArcFov = FromFloat4(ArcAimCfg.col_fov);
	colArcLine = FromFloat4(ArcAimCfg.col_line);
	colSkeleton = FromFloat4(col_skeleton);
	colSkeletonInvisible = FromFloat4(col_skeleton_invisible);
	colHeadDot = FromFloat4(col_headdot);
	colBox = FromFloat4(colpick2_generic);
	colFilled = FromFloat4(col_filledbox);
	colSnapline = FromFloat4(col_spanline);
	colUsername = FromFloat4(col_username);
	colDistance = FromFloat4(col_distance);
	colSquad = FromFloat4(col_squad);
	colHeld = FromFloat4(col_held);
	colInvisible = FromFloat4(col_invisible);
	colKnocked = FromFloat4(col_knocked);
	colHealthHigh = FromFloat4(healthHighColor);
	colHealthMed = FromFloat4(healthMediumColor);
	colHealthLow = FromFloat4(healthLowColor);
	colHealthBack = FromFloat4(healthBackColor);
	colShield = FromFloat4(shieldColor);
	colItems = FromFloat4(Cfg::Scene::ItemsColor);
	colArcs = FromFloat4(Cfg::Scene::ARCsColor);
	colContainers = FromFloat4(Cfg::Scene::ContainersColor);
	colCorpses = FromFloat4(Cfg::Scene::CorpsesColor);
	colRaiderCache = FromFloat4(Cfg::Scene::RaiderCacheColor);
	colSalvage = FromFloat4(Cfg::Scene::SalvageColor);
	colGrenade = FromFloat4(Cfg::Scene::GrenadeColor);
	colMines = FromFloat4(Cfg::Scene::MinesColor);
	colTraps = FromFloat4(Cfg::Scene::TrapsColor);
	colExtractions = FromFloat4(Cfg::Scene::ExtractionsColor);
	colArcProbes = FromFloat4(Cfg::Scene::ArcProbesColor);
	colFlashlight = FromFloat4(Cfg::Scene::flashlightColor);
	colPlayerFlags = FromFloat4(Cfg::Scene::playerFlagsColor);
	colBounds = FromFloat4(Cfg::Scene::BoundsColor);
	colChams = FromFloat4(col_chams);
	colChams2 = FromFloat4(col_chams2);
	colCrosshair = FromFloat4(col_crosshair);
	colCrosshairOutline = FromFloat4(col_crosshair_outline);
	colCrosshairAccent = FromFloat4(col_crosshair_accent);
	colHud = FromFloat4(col_hud);
	colWarningText = FromFloat4(col_warning_text);
	colWarningBg = FromFloat4(col_warning_bg);
	colWarningAccent = FromFloat4(col_warning_accent);
	colMenu = Color(
		static_cast<int>(ui::colors::main.x * 255.f),
		static_cast<int>(ui::colors::main.y * 255.f),
		static_cast<int>(ui::colors::main.z * 255.f),
		255);

	if (distanceWithName)
		uiEspDistanceMode = 2;
	else if (visuals4)
		uiEspDistanceMode = 1;
	else
		uiEspDistanceMode = 0;

	warningConfigMode = std::clamp(warningConfigMode, 0, 1);
	warningPreset = std::clamp(warningPreset, 0, 4);
	if (warningPreset >= 0 && warningPreset <= 3 && !WarningPresetMatches(warningPreset))
		warningPreset = 4;
	s_warnPresetSeen = warningPreset;
}

void Menu::SyncToSettings()
{
	AimCfg.aimKey = aimKeybind.key;
	AimCfg.aimKeyMode = aimKeybind.mode;
	ArcAimCfg.activationKey = arcAimKeybind.key;
	ArcAimCfg.activationKeyMode = arcAimKeybind.mode;
	ArcAimCfg.weakspotKey = arcWeakspotKeybind.key;
	ArcAimCfg.weakspotKeyMode = arcWeakspotKeybind.mode;
	zoomChangerKey = zoomKeybind.key;
	zoomChangerKeyMode = zoomKeybind.mode;
	combatModeKey = combatModeKeybind.key;
	combatModeKeyMode = combatModeKeybind.mode;

	ToFloat4(colFov, col_fov);
	ToFloat4(colLine, col_line);
	ToFloat4(colArcFov, ArcAimCfg.col_fov);
	ToFloat4(colArcLine, ArcAimCfg.col_line);
	ToFloat4(colSkeleton, col_skeleton);
	ToFloat4(colSkeletonInvisible, col_skeleton_invisible);
	ToFloat4(colHeadDot, col_headdot);
	ToFloat4(colBox, colpick2_generic);
	ToFloat4(colFilled, col_filledbox);
	ToFloat4(colSnapline, col_spanline);
	ToFloat4(colUsername, col_username);
	ToFloat4(colDistance, col_distance);
	ToFloat4(colSquad, col_squad);
	ToFloat4(colHeld, col_held);
	ToFloat4(colInvisible, col_invisible);
	ToFloat4(colKnocked, col_knocked);
	ToFloat4(colHealthHigh, healthHighColor);
	ToFloat4(colHealthMed, healthMediumColor);
	ToFloat4(colHealthLow, healthLowColor);
	ToFloat4(colHealthBack, healthBackColor);
	ToFloat4(colShield, shieldColor);
	ToFloat4(colItems, Cfg::Scene::ItemsColor);
	ToFloat4(colArcs, Cfg::Scene::ARCsColor);
	ToFloat4(colContainers, Cfg::Scene::ContainersColor);
	ToFloat4(colCorpses, Cfg::Scene::CorpsesColor);
	ToFloat4(colRaiderCache, Cfg::Scene::RaiderCacheColor);
	ToFloat4(colFlashlight, Cfg::Scene::flashlightColor);
	ToFloat4(colPlayerFlags, Cfg::Scene::playerFlagsColor);
	ToFloat4(colBounds, Cfg::Scene::BoundsColor);
	ToFloat4(colChams, col_chams);
	ToFloat4(colChams2, col_chams2);
	ToFloat4(colSalvage, Cfg::Scene::SalvageColor);
	ToFloat4(colGrenade, Cfg::Scene::GrenadeColor);
	ToFloat4(colMines, Cfg::Scene::MinesColor);
	ToFloat4(colTraps, Cfg::Scene::TrapsColor);
	ToFloat4(colExtractions, Cfg::Scene::ExtractionsColor);
	ToFloat4(colArcProbes, Cfg::Scene::ArcProbesColor);
	ToFloat4(colCrosshair, col_crosshair);
	ToFloat4(colCrosshairOutline, col_crosshair_outline);
	ToFloat4(colCrosshairAccent, col_crosshair_accent);
	ToFloat4(colHud, col_hud);
	ToFloat4(colWarningText, col_warning_text);
	ToFloat4(colWarningBg, col_warning_bg);
	ToFloat4(colWarningAccent, col_warning_accent);

	ui::colors::main = ImVec4(colMenu.r / 255.f, colMenu.g / 255.f, colMenu.b / 255.f, 1.f);
	ui::colors::ApplyBrandTheme();
	Style::accentColor = colMenu;
	Style::overrideAccent = true;
	Style::Update();

	visuals4 = uiEspDistanceMode >= 1;
	distanceWithName = uiEspDistanceMode == 2;

	if (s_aimFeelApplied < 0)
		s_aimFeelApplied = AimFeelPreset;
	else if (AimFeelPreset != s_aimFeelApplied) {
		s_aimFeelApplied = AimFeelPreset;
		if (AimFeelPreset >= 0 && AimFeelPreset <= 2)
			ApplyAimFeelPreset(AimFeelPreset);
	}

	if (s_warnPresetSeen < 0)
		s_warnPresetSeen = warningPreset;
	else if (warningPreset != s_warnPresetSeen)
	{
		s_warnPresetSeen = warningPreset;
		if (warningPreset >= 0 && warningPreset <= 3)
			ApplyWarningPreset(warningPreset);
	}
	warningMinConfidence = std::clamp(warningMinConfidence, 1, 4);
	multipleMinConfidence = std::clamp(multipleMinConfidence, 1, 4);
	if (warningPreset >= 0 && warningPreset <= 3 && !WarningPresetMatches(warningPreset))
	{
		warningPreset = 4;
		s_warnPresetSeen = 4;
	}
}

void Menu::Init()
{
	Style::Update();
	cfg_load();
	SyncFromSettings(true);
	Style::accentColor = colMenu;
	Style::overrideAccent = true;
	Style::Update();

	Vector2 basePosition = {120.0f, 120.0f};
	Vector2 baseSize = {960.0f, 650.0f};

	window = std::make_unique<CWindow>(X("Syndikate Arc"), basePosition, baseSize, X("v2.0  •  ONLINE"));
	window->SetOpened(false);

	if (auto Aim = window->AddObject<CTab>(X("Aim")))
	{
		Aim->SetIcon(ICON_FA_CROSSHAIRS);
		InitCombatTab(Aim);
	}
	if (auto Esp = window->AddObject<CTab>(X("ESP")))
	{
		Esp->SetIcon(ICON_FA_EYE);
		InitVisualsTab(Esp);
	}
	if (auto World = window->AddObject<CTab>(X("World")))
	{
		World->SetIcon(ICON_FA_GLOBE);
		InitWorldTab(World);
	}
	if (auto Radar = window->AddObject<CTab>(X("Radar")))
	{
		Radar->SetIcon(ICON_FA_SATELLITE_DISH);
		InitRadarTab(Radar);
	}
	if (auto HudTab = window->AddObject<CTab>(X("HUD")))
	{
		HudTab->SetIcon(ICON_FA_DISPLAY);
		InitHudTab(HudTab);
	}
	if (auto WarningsTab = window->AddObject<CTab>(X("Warnings")))
	{
		WarningsTab->SetIcon(ICON_FA_TRIANGLE_EXCLAMATION);
		InitWarningsTab(WarningsTab);
	}
	if (auto SettingsTab = window->AddObject<CTab>(X("Settings")))
	{
		SettingsTab->SetIcon(ICON_FA_GEAR);
		InitMiscTab(SettingsTab);
	}
	if (auto Configs = window->AddObject<CTab>(X("Configs")))
	{
		Configs->SetIcon(ICON_FA_FLOPPY_DISK);
		Configs->SetPinBottom(true);
		InitConfigsTab(Configs);
	}

	g_cfgPendingMenuSync = false;
}

// Tabs are addressed by name hash rather than index, so stepping through them
// means rebuilding the ordered list from the window's children each time.
static void CycleTab(int step)
{
	if (!Menu::window)
		return;

	std::vector<CTab*> tabs;
	for (auto* child : Menu::window->GetChildren())
	{
		if (auto* tab = dynamic_cast<CTab*>(child))
			tabs.push_back(tab);
	}

	const int count = static_cast<int>(tabs.size());
	if (count == 0)
		return;

	const std::size_t current = Menu::window->GetTabHash();
	int index = 0;
	for (int i = 0; i < count; ++i)
	{
		if (tabs[i]->GetTabHash() == current)
		{
			index = i;
			break;
		}
	}

	index = ((index + step) % count + count) % count;
	Menu::window->SetTabHash(tabs[index]->GetTabHash());
}

void Menu::Tick()
{
	if (!window)
		return;

	if (Input::IsKeyPressed(window->GetKey()) || (padEnabled && Gamepad::MenuComboPressed()))
		window->SetOpened(!window->GetOpened());

	if (window->GetOpened() && padEnabled && padTabCycle && Gamepad::Connected())
	{
		int step = 0;
		if (Input::IsKeyPressed(GP_RB))
			step = 1;
		else if (Input::IsKeyPressed(GP_LB))
			step = -1;

		if (step != 0)
			CycleTab(step);
	}

	window->SetTld((ctx && ctx->process_id) ? X("v2.0  •  ONLINE") : X("v2.0  •  OFFLINE"));

	const bool wantOpen = window->GetOpened();
	const bool justOpened = wantOpen && !isOpen;
	if (wantOpen != isOpen) {
		isOpen = wantOpen;
		panel_visible = isOpen;
		AppWindow::SetInteractive(isOpen);
	} else {
		isOpen = wantOpen;
		panel_visible = isOpen;
	}

	static bool s_lastStreamProof = true;
	if (streamproof != s_lastStreamProof) {
		s_lastStreamProof = streamproof;
		AppWindow::SetStreamProof(streamproof);
	}

	// Fully closed — skip draw + color sync.
	if (!isOpen && window->GetWindowAlpha() <= 0.0f)
		return;

	const bool dragging = window->GetDragging();
	// Settings sync is heavy (many color conversions) — not every Present.
	static ULONGLONG s_lastMenuSync = 0;
	const ULONGLONG now = GetTickCount64();

	// Config load must win over stale widget values (Save/Load used to get overwritten).
	if (g_cfgPendingMenuSync) {
		SyncFromSettings(true);
		g_cfgPendingMenuSync = false;
		s_lastMenuSync = now;
	}

	// The widget mirrors own their values while the menu is up, exactly as the
	// keybinds already do - see the note at the top of SyncFromSettings.
	//
	// This used to pull settings back into the mirrors on the same 50ms timer that
	// pushed them out, and it ran *before* Render. Any edit made on one of the
	// frames in between was therefore reverted at the next sync boundary before
	// SyncToSettings ever saw it, so a click only stuck if it happened to land on a
	// sync frame - roughly one frame in seven. That is the glitchy segmented
	// control (pill highlights, snaps back) and a colour that reverts to its
	// previous value on release. Settings are now only pulled at the points where
	// something outside the widgets actually changed them: menu open, config load,
	// and config/share import.
	if (justOpened)
		SyncFromSettings();

	Render::StartDrawing(ImGui::GetBackgroundDrawList());
	window->Render();
	Render::EndDrawing();

	const bool syncDue = justOpened || (now - s_lastMenuSync) >= 50;
	if (g_cfgPendingMenuSync) {
		SyncFromSettings(true);
		g_cfgPendingMenuSync = false;
		s_lastMenuSync = now;
	} else if (!dragging && syncDue) {
		SyncToSettings();
		s_lastMenuSync = now;
	}
}

void Menu::Shutdown()
{
	window.reset();
}
