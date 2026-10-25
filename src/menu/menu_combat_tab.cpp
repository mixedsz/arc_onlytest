#include <Cheat.hpp>
#include "menu.hpp"
#include "settings/Settings.hpp"

static CContainer* Section(CObject* parent, const char* title, const char* persistId)
{
	auto* box = parent->AddObject<CContainer>(title);
	box->SetPersistId(persistId);
	return box;
}

void Menu::InitCombatTab(CTab* tab)
{
	if (auto Arcs = tab->AddObject<CSubtab>(X("ARCs")))
	{
		if (auto Box = Section(Arcs, X("Aimbot"), X("aim.arc.aimbot")))
		{
			Box->AddObject<CCheckbox>(X("Enable Aimbot"), &ArcAimCfg.enable);
			Box->AddObject<CCheckbox>(X("Weakspot Aim"), &ArcAimCfg.weakspotMode);
			Box->AddObject<CCheckbox>(X("Target Lock"), &ArcAimCfg.targetLock);
			Box->AddObject<CCheckbox>(X("Only Visible Targets"), &ArcAimCfg.visibleonly,
				X("Requires ARC Visibility Check under ESP → ARCs."));
			Box->AddObject<CCheckbox>(X("Prediction"), &ArcAimCfg.prediction);
			Box->AddObject<CKeybind>(X("ARC Aim Key"), &Menu::arcAimKeybind);
			Box->AddObject<CKeybind>(X("Weakspot Key"), &Menu::arcWeakspotKeybind);
		}

		if (auto Box = Section(Arcs, X("Targeting"), X("aim.arc.targeting")))
		{
			Box->AddObject<CSliderFloat>(X("Max Distance"), &ArcAimCfg.maxDistance, 50.f, 500.f, X("{:.0f}"));
		}

		if (auto Box = Section(Arcs, X("FOV"), X("aim.arc.fov")))
		{
			Box->AddObject<CSliderFloat>(X("FOV"), &ArcAimCfg.fov, 0.f, 300.f, X("{:.0f}"));
			auto drawFov = Box->AddObject<CCheckbox>(X("Draw FOV Circle"), &ArcAimCfg.drawFov);
			if (auto popup = drawFov->AddObject<CPopup>())
				popup->AddObject<CColorPicker>(X("FOV Color"), &Menu::colArcFov, true);
			Box->AddObject<CSliderFloat>(X("Smoothness"), &ArcAimCfg.smooth, 1.f, 30.f, X("{:.1f}"));
		}

		if (auto Box = Section(Arcs, X("Advanced"), X("aim.arc.advanced")))
		{
			Box->AddObject<CSliderFloat>(X("Aim Update Rate"), &ArcAimCfg.maxHz, 30.f, 360.f, X("{:.0f}Hz"));
			auto line = Box->AddObject<CCheckbox>(X("Target Line"), &ArcAimCfg.lines);
			if (auto popup = line->AddObject<CPopup>())
				popup->AddObject<CColorPicker>(X("Line Color"), &Menu::colArcLine, true);
			Box->AddObject<CCheckbox>(X("Target Info"), &ArcAimCfg.showTargetInfo);
		}
	}
}
