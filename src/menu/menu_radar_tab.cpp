#include <Cheat.hpp>
#include "menu.hpp"
#include "settings/Settings.hpp"

static CContainer* Section(CObject* parent, const char* title, const char* persistId)
{
	auto* box = parent->AddObject<CContainer>(title);
	box->SetPersistId(persistId);
	return box;
}

void Menu::InitRadarTab(CTab* tab)
{
	if (auto Box = Section(tab, X("Toggle"), X("radar.toggle")))
	{
		Box->AddObject<CCheckbox>(X("Enable Radar"), &showRadar);
	}
	if (auto Box = Section(tab, X("Display"), X("radar.display")))
	{
		Box->AddObject<CCheckbox>(X("Show Name"), &radarShowName);
		Box->AddObject<CCheckbox>(X("Show Distance"), &radarShowDistance);
	}
	if (auto Box = Section(tab, X("Position & Size"), X("radar.position")))
	{
		Box->AddObject<CSliderFloat>(X("Radar Size"), &radarSize, 50.f, 300.f, X("{:.0f}"));
		Box->AddObject<CSliderFloat>(X("Position X"), &radarPosX, 0.f, 1920.f, X("{:.0f}"));
		Box->AddObject<CSliderFloat>(X("Position Y"), &radarPosY, 0.f, 1080.f, X("{:.0f}"));
		Box->AddObject<CButton>(X("Drag to Position"), []() {
			radarDragPosition = !radarDragPosition;
		}, X("Click the in-game radar and drag it to place it visually"));
	}
}
