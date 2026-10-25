#include <Cheat.hpp>
#include "menu.hpp"
#include "settings/Settings.hpp"

static CContainer* Section(CObject* parent, const char* title, const char* persistId)
{
	auto* box = parent->AddObject<CContainer>(title);
	box->SetPersistId(persistId);
	return box;
}

void Menu::InitWarningsTab(CTab* tab)
{
	auto easy = []() { return warningConfigMode == 0; };
	auto adv = []() { return warningConfigMode == 1; };

	if (auto Box = Section(tab, X("General"), X("warn.general")))
	{
		Box->AddObject<CCheckbox>(X("Enable Warnings"), &warningsEnabled,
			X("Master switch. Off clears every pending and visible warning"));
		Box->AddObject<CRadio>(X("Mode"), &warningConfigMode,
			std::vector<std::string>{ X("Easy"), X("Advanced") },
			X("Easy shows presets. Advanced shows every warning slider"));
		Box->AddObject<CDropdown>(X("Preset"), &warningPreset,
			std::vector<std::string>{
				X("Balanced"), X("Close Range"), X("Quiet"), X("Full Awareness"), X("Custom")
			},
			X("Applies once when you pick Balanced, Close Range, Quiet, or Full Awareness. Custom means your Advanced values no longer match a preset"));
	}

	if (auto Box = Section(tab, X("Easy Setup"), X("warn.easy")))
	{
		Box->SetVisibleCondition(easy);
		auto d0 = Box->AddObject<CLabel>(X("Balanced: approaching threats at mid range. Recommended."));
		d0->SetVisibleCondition([]() { return warningConfigMode == 0 && warningPreset == 0; });
		auto d1 = Box->AddObject<CLabel>(X("Close Range: shorter distance, faster alerts, still requires approaching."));
		d1->SetVisibleCondition([]() { return warningConfigMode == 0 && warningPreset == 1; });
		auto d2 = Box->AddObject<CLabel>(X("Quiet: closing-fast only, 4-way labels, fewer banners."));
		d2->SetVisibleCondition([]() { return warningConfigMode == 0 && warningPreset == 2; });
		auto d3 = Box->AddObject<CLabel>(X("Full Awareness: includes proximity-only alerts. Noisier."));
		d3->SetVisibleCondition([]() { return warningConfigMode == 0 && warningPreset == 3; });
		auto d4 = Box->AddObject<CLabel>(X("Custom: Advanced values that no longer match a built-in preset."));
		d4->SetVisibleCondition([]() { return warningConfigMode == 0 && warningPreset == 4; });
		Box->AddObject<CCheckbox>(X("Direction Indicator"), &directionIndicatorEnabled,
			X("Points toward the current highest-priority warning"));
		Box->AddObject<CCheckbox>(X("Show Distance"), &nearbyEnemyShowDistance,
			X("Show distance on warning cards"));
		Box->AddObject<CSliderFloat>(X("Interface Scale"), &warningInterfaceScale, 0.5f, 2.0f, X("{:.2f}"));
		Box->AddObject<CLabel>(X("Alerts use movement and cached combat state. Confirmed line of sight and confirmed enemy aiming are not currently available."));
	}

	if (auto Box = Section(tab, X("Advanced General"), X("warn.adv.general")))
	{
		Box->SetVisibleCondition(adv);
		Box->AddObject<CDropdown>(X("Minimum Confidence"), &warningMinConfidence,
			std::vector<std::string>{ X("Unused"), X("Nearby"), X("Approaching"), X("Closing Fast"), X("Possible Engagement") },
			X("Alerts below this rank are ignored. Nearby is distance-only"));
		Box->AddObject<CCheckbox>(X("Show Direction Text"), &warningShowDirectionText);
		Box->AddObject<CRadio>(X("Direction Detail"), &warningDirectionDetail,
			std::vector<std::string>{ X("4-way"), X("8-way") },
			X("Changes labels only. Does not reset or delay an active warning"));
		Box->AddObject<CCheckbox>(X("Show Distance"), &nearbyEnemyShowDistance,
			X("Show distance on warning cards"));
		Box->AddObject<CSliderInt>(X("Max Visible"), &warningMaxVisible, 1, 5, X("{}"),
			X("Highest-priority banners shown at once"));
	}

	if (auto Box = Section(tab, X("Approaching"), X("warn.adv.approach")))
	{
		Box->SetVisibleCondition(adv);
		Box->AddObject<CCheckbox>(X("Approaching Filter"), &approachingAlertsEnabled,
			X("Uses enemy horizontal velocity toward you. Stationary enemies do not alert when you run at them"));
		Box->AddObject<CSliderFloat>(X("Min Closing Speed"), &approachingMinimumSpeed, 0.2f, 8.0f, X("{:.1f} m/s"),
			X("Enemy speed toward you, meters per second, from cached velocity / 100"));
		Box->AddObject<CSliderFloat>(X("Fast Closing"), &approachingFastSpeed, 0.5f, 12.0f, X("{:.1f} m/s"));
		Box->AddObject<CSliderFloat>(X("Sustain"), &approachingMinimumDuration, 0.10f, 1.5f, X("{:.2f}s"),
			X("Closing speed must hold this long before Approaching becomes active"));
		Box->AddObject<CSliderFloat>(X("Max Distance"), &approachingMaxDistance, 20.f, 200.f, X("{:.0f}m"));
		Box->AddObject<CSliderFloat>(X("Clear Speed"), &approachingClearSpeed, 0.f, 4.f, X("{:.1f} m/s"));
		Box->AddObject<CSliderFloat>(X("Clear Debounce"), &approachingClearDebounce, 0.10f, 1.0f, X("{:.2f}s"));
	}

	if (auto Box = Section(tab, X("Nearby / Multiple"), X("warn.adv.near")))
	{
		Box->SetVisibleCondition(adv);
		Box->AddObject<CCheckbox>(X("Legacy Nearby"), &nearbyEnemyEnabled,
			X("Distance-only alerts with no approaching requirement. Noisier"));
		Box->AddObject<CSliderFloat>(X("Nearby Distance"), &nearbyEnemyDistance, 10.f, 150.f, X("{:.0f}m"));
		Box->AddObject<CCheckbox>(X("Multiple Enemies"), &multipleEnemiesEnabled,
			X("Count is all qualifying threats. Direction is the current highest-priority threat"));
		Box->AddObject<CSliderInt>(X("Enemy Count"), &multipleEnemyCount, 2, 6, X("{}"));
		Box->AddObject<CSliderFloat>(X("Multiple Distance"), &multipleEnemyDistance, 10.f, 150.f, X("{:.0f}m"));
		Box->AddObject<CDropdown>(X("Multiple Min Confidence"), &multipleMinConfidence,
			std::vector<std::string>{ X("Unused"), X("Nearby"), X("Approaching"), X("Closing Fast"), X("Possible Engagement") });
	}

	if (auto Box = Section(tab, X("Behind / Direction"), X("warn.adv.behind")))
	{
		Box->SetVisibleCondition(adv);
		Box->AddObject<CCheckbox>(X("Enemy Behind You"), &enemyBehindEnabled,
			X("Extra card only when a different qualifying threat is behind you"));
		Box->AddObject<CSliderFloat>(X("Behind Distance"), &enemyBehindDistance, 10.f, 200.f, X("{:.0f}m"));
		Box->AddObject<CSliderFloat>(X("Behind Angle"), &enemyBehindAngle, 90.f, 170.f, X("{:.0f}"),
			X("Degrees off look direction. 90 = rear hemisphere, 120 = rear-focused, 170 = almost directly behind"));
		Box->AddObject<CCheckbox>(X("Direction Indicator"), &directionIndicatorEnabled,
			X("Points toward the current highest-priority warning. Hidden while that warning is delayed"));
		Box->AddObject<CSliderFloat>(X("Indicator Scale"), &directionIndicatorScale, 0.5f, 2.0f, X("{:.2f}"));
		Box->AddObject<CCheckbox>(X("Indicator Pulse"), &directionIndicatorPulse);
	}

	if (auto Box = Section(tab, X("Possible Engagement"), X("warn.adv.poss")))
	{
		Box->SetVisibleCondition(adv);
		Box->AddObject<CCheckbox>(X("Possible Engagement"), &possibleEngagementEnabled,
			X("Raises Approaching or Closing Fast when the enemy is zooming or firing. Does not mean they see you or are aiming at you"));
		Box->AddObject<CCheckbox>(X("Use Zooming"), &possibleEngagementUseZooming,
			X("Cached ADS state. Not confirmed aiming"));
		Box->AddObject<CCheckbox>(X("Use Firing"), &possibleEngagementUseFiring,
			X("Cached firing state. Not confirmed firing at you"));
	}

	if (auto Box = Section(tab, X("Appearance"), X("warn.adv.appear")))
	{
		Box->SetVisibleCondition(adv);
		Box->AddObject<CSliderFloat>(X("Interface Scale"), &warningInterfaceScale, 0.5f, 2.0f, X("{:.2f}"));
		Box->AddObject<CCheckbox>(X("Progress Bar"), &warningProgressBar);
		Box->AddObject<CColorPicker>(X("Text"), &Menu::colWarningText, true);
		Box->AddObject<CColorPicker>(X("Background"), &Menu::colWarningBg, true);
		Box->AddObject<CColorPicker>(X("Accent"), &Menu::colWarningAccent, true);
	}

	if (auto Box = Section(tab, X("Timing"), X("warn.adv.timing")))
	{
		Box->SetVisibleCondition(adv);
		Box->AddObject<CSliderFloat>(X("Fade Duration"), &warningFadeDuration, 0.05f, 2.f, X("{:.2f}s"));
		Box->AddObject<CCheckbox>(X("Alert Delay"), &warningDelayJitter,
			X("Random wait before a new warning appears"));
		Box->AddObject<CSliderFloat>(X("Delay Min"), &warningDelayMin, 0.f, 1.f, X("{:.2f}s"));
		Box->AddObject<CSliderFloat>(X("Delay Max"), &warningDelayMax, 0.f, 1.5f, X("{:.2f}s"));
	}
}
