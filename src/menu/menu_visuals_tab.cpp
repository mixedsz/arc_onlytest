#include <Cheat.hpp>
#include "menu.hpp"
#include "settings/Settings.hpp"

static CContainer* Section(CObject* parent, const char* title, const char* persistId)
{
	auto* box = parent->AddObject<CContainer>(title);
	box->SetPersistId(persistId);
	return box;
}

static CCheckbox* FilteredToggle(CContainer* box, const char* label, bool* value, std::string* search)
{
	auto* cb = box->AddObject<CCheckbox>(label, value);
	cb->SetVisibleCondition([search, name = std::string(label)]() {
		return UI::MatchesSearch(name, search ? *search : std::string{});
	});
	return cb;
}

void Menu::InitVisualsTab(CTab* tab)
{
	if (auto Players = tab->AddObject<CSubtab>(X("Players")))
	{
		if (auto Box = Section(Players, X("General"), X("esp.players.general")))
		{
			Box->AddObject<CCheckbox>(X("Enemy ESP"), &visualsb);
			Box->AddObject<CSliderFloat>(X("Max Distance"), &distance_vis, 10.f, kEspDistanceUnlimited, X("{:.0f}"));
		}

		if (auto Box = Section(Players, X("Box & Skeleton"), X("esp.players.box")))
		{
			Box->AddObject<CCheckbox>(X("Visibility Check"), &visibilityCheck,
				X("Tints occluded players with the Occluded colors below. ARCs have their own toggle under ESP → ARCs."));

			Box->AddObject<CCheckbox>(X("Skeleton"), &visuals5);
			Box->AddObject<CColorPicker>(X("Skeleton Visible"), &Menu::colSkeleton, true);
			Box->AddObject<CColorPicker>(X("Skeleton Occluded"), &Menu::colSkeletonInvisible, true);

			auto head = Box->AddObject<CCheckbox>(X("Head Dot"), &headdot);
			head->AddObject<CColorPicker>(X("Head Dot Color"), &Menu::colHeadDot, true);

			auto box2d = Box->AddObject<CCheckbox>(X("2D Box"), &visuals2);
			if (auto popup = box2d->AddObject<CPopup>())
			{
				popup->AddObject<CDropdown>(X("Box Type"), &indexBox,
					std::vector<std::string>{ X("FULL"), X("CORNERS"), X("DASHED"), X("ROUNDED") });
			}
			Box->AddObject<CColorPicker>(X("Box Visible"), &Menu::colBox, true);
			Box->AddObject<CColorPicker>(X("Box Occluded"), &Menu::colInvisible, true);
			Box->AddObject<CCheckbox>(X("Outlined Box"), &showOutlineBox);
			auto filled = Box->AddObject<CCheckbox>(X("Filled Box"), &showFilledBox);
			filled->AddObject<CColorPicker>(X("Fill Color"), &Menu::colFilled, true);
			Box->AddObject<CSliderFloat>(X("Skeleton Thickness"), &skeleton_thickness, 0.5f, 5.f, X("{:.1f}"));
			Box->AddObject<CSliderFloat>(X("Box Thickness"), &box_thickness, 0.5f, 5.f, X("{:.1f}"));
		}

		if (auto Box = Section(Players, X("Info Labels"), X("esp.players.labels")))
		{
			auto name = Box->AddObject<CCheckbox>(X("Username"), &visuals1);
			name->AddObject<CColorPicker>(X("Name Color"), &Menu::colUsername, true);
			Box->AddObject<CCheckbox>(X("Platform"), &showPlatform);
			Box->AddObject<CSliderFloat>(X("Platform Icon Size"), &platformIconScale, 0.5f, 3.f, X("{:.1f}"));

			Box->AddObject<CRadio>(X("Distance"), &uiEspDistanceMode,
				std::vector<std::string>{ X("Off"), X("Distance Only"), X("Name+Distance") });
			Box->AddObject<CColorPicker>(X("Distance Color"), &Menu::colDistance, true);

			auto squad = Box->AddObject<CCheckbox>(X("Team ID"), &squadname);
			squad->AddObject<CColorPicker>(X("Team ID Color"), &Menu::colSquad, true);

			auto held = Box->AddObject<CCheckbox>(X("Held Item"), &helditem);
			held->AddObject<CColorPicker>(X("Held Color"), &Menu::colHeld, true);

			Box->AddObject<CCheckbox>(X("Weapon Level"), &weaponlevel);

			auto flags = Box->AddObject<CCheckbox>(X("Player Flags"), &Cfg::Scene::showPlayerFlags);
			flags->AddObject<CColorPicker>(X("Flags Color"), &Menu::colPlayerFlags, true);
			if (auto popup = flags->AddObject<CPopup>())
			{
				popup->AddObject<CCheckbox>(X("Aiming"), &Cfg::Scene::playerFlagsFilterMulti[0]);
				popup->AddObject<CCheckbox>(X("Sprinting"), &Cfg::Scene::playerFlagsFilterMulti[1]);
				popup->AddObject<CCheckbox>(X("Dodge"), &Cfg::Scene::playerFlagsFilterMulti[2]);
				popup->AddObject<CCheckbox>(X("Jumping"), &Cfg::Scene::playerFlagsFilterMulti[3]);
				popup->AddObject<CCheckbox>(X("Reloading"), &Cfg::Scene::playerFlagsFilterMulti[4]);
				popup->AddObject<CCheckbox>(X("Firing"), &Cfg::Scene::playerFlagsFilterMulti[5]);
				popup->AddObject<CCheckbox>(X("Crouched"), &Cfg::Scene::playerFlagsFilterMulti[6]);
				popup->AddObject<CCheckbox>(X("Prone"), &Cfg::Scene::playerFlagsFilterMulti[7]);
			}
		}

		if (auto Box = Section(Players, X("Bars"), X("esp.players.bars")))
		{
			auto hp = Box->AddObject<CCheckbox>(X("Health Bar"), &showHealthBar);
			if (auto popup = hp->AddObject<CPopup>())
			{
				popup->AddObject<CColorPicker>(X("High"), &Menu::colHealthHigh, true);
				popup->AddObject<CColorPicker>(X("Medium"), &Menu::colHealthMed, true);
				popup->AddObject<CColorPicker>(X("Low"), &Menu::colHealthLow, true);
				popup->AddObject<CColorPicker>(X("Background"), &Menu::colHealthBack, true);
				popup->AddObject<CCheckbox>(X("Color By Value"), &healthColorByValue);
				popup->AddObject<CDropdown>(X("Position"), &healthBarPos,
					std::vector<std::string>{ X("LEFT"), X("RIGHT"), X("TOP"), X("BOTTOM") });
			}
			auto shield = Box->AddObject<CCheckbox>(X("Shield Bar"), &showShieldBar);
			shield->AddObject<CColorPicker>(X("Shield Color"), &Menu::colShield, true);
			if (auto popup = shield->AddObject<CPopup>())
			{
				popup->AddObject<CDropdown>(X("Position"), &shieldBarPos,
					std::vector<std::string>{ X("LEFT"), X("RIGHT"), X("TOP"), X("BOTTOM") });
			}
		}

		if (auto Box = Section(Players, X("Effects"), X("esp.players.effects")))
		{
			auto chams = Box->AddObject<CCheckbox>(X("Chams"), &drawChams);
			chams->AddObject<CColorPicker>(X("Chams Color"), &Menu::colChams, true);
			if (auto popup = chams->AddObject<CPopup>())
			{
				popup->AddObject<CColorPicker>(X("Rim / Glow Color"), &Menu::colChams2, true);
				popup->AddObject<CDropdown>(X("Mode"), &chamsMode,
					std::vector<std::string>{ X("Glow"), X("Solid"), X("Wireframe"), X("Rim"), X("Volumetric") });
				popup->AddObject<CSliderFloat>(X("Glow Width"), &chamsGlowWidth, 1.f, 40.f, X("{:.0f}"));
				popup->AddObject<CSliderFloat>(X("Fill Opacity"), &chamsFillOpacity, 0.1f, 1.f, X("{:.2f}"));
			}
			auto snap = Box->AddObject<CCheckbox>(X("Snapline"), &visuals3);
			snap->AddObject<CColorPicker>(X("Snapline Color"), &Menu::colSnapline, true);
			if (auto popup = snap->AddObject<CPopup>())
			{
				popup->AddObject<CDropdown>(X("Snapline Type"), &indexSnapline,
					std::vector<std::string>{ X("CENTER"), X("TOP"), X("BOTTOM") });
			}
			Box->AddObject<CCheckbox>(X("Out-of-FOV Arrows"), &offarrows);
		}
	}

	if (auto Arcs = tab->AddObject<CSubtab>(X("ARCs")))
	{
		if (auto Box = Section(Arcs, X("ARC ESP"), X("esp.ai.general")))
		{
			auto arcs = Box->AddObject<CCheckbox>(X("ARC ESP"), &Cfg::Scene::ARCsESP);
			arcs->AddObject<CColorPicker>(X("ARC Color"), &Menu::colArcs, true);
			Box->AddObject<CSliderFloat>(X("Max Distance"), &Cfg::Scene::MaxDistanceArcs, 50.f, 500.f, X("{:.0f}"));
			Box->AddObject<CCheckbox>(X("ARC Health Bar"), &Cfg::Scene::ARCsShowHealthBar);
			Box->AddObject<CCheckbox>(X("ARC Visibility Check"), &arcVisibilityCheck,
				X("Tints occluded ARCs with the Occluded color. Independent of player Visibility Check."));
			Box->AddObject<CColorPicker>(X("Occluded"), &Menu::colInvisible, true);
			Box->AddObject<CCheckbox>(X("Show Destroyed ARCs"), &Cfg::Scene::ARCsShowDestroyed);
		}

		if (auto Actions = Section(Arcs, X("Quick Actions"), X("world.ai.actions")))
		{
			Actions->SetCollapsible(false);
			if (auto row = Actions->AddObject<CGrid>(3, 8.0f))
			{
				row->AddObject<CButton>(X("Select All"), []() {
					for (int i = 0; i < 21; ++i)
						Cfg::Scene::ArcTypeFilterMulti[i] = true;
				});
				row->AddObject<CButton>(X("Deselect All"), []() {
					for (int i = 0; i < 21; ++i)
						Cfg::Scene::ArcTypeFilterMulti[i] = false;
				});
				row->AddObject<CButton>(X("Only Elite"), []() {
					for (int i = 0; i < 21; ++i)
						Cfg::Scene::ArcTypeFilterMulti[i] = false;
					Cfg::Scene::ArcTypeFilterMulti[14] = true;
					Cfg::Scene::ArcTypeFilterMulti[15] = true;
					Cfg::Scene::ArcTypeFilterMulti[16] = true;
				});
			}
			Actions->AddObject<CTextInput>(X("Search ARC types"), &uiArcTypeSearch, false, true);
		}

		if (auto Box = Section(Arcs, X("Basic Threats"), X("world.ai.basic")))
		{
			FilteredToggle(Box, X("Wasp"), &Cfg::Scene::ArcTypeFilterMulti[0], &uiArcTypeSearch);
			FilteredToggle(Box, X("Hornet"), &Cfg::Scene::ArcTypeFilterMulti[1], &uiArcTypeSearch);
			FilteredToggle(Box, X("Tick"), &Cfg::Scene::ArcTypeFilterMulti[2], &uiArcTypeSearch);
		}
		if (auto Box = Section(Arcs, X("Combat Units"), X("world.ai.combat")))
		{
			FilteredToggle(Box, X("Turret"), &Cfg::Scene::ArcTypeFilterMulti[3], &uiArcTypeSearch);
			FilteredToggle(Box, X("Bastion"), &Cfg::Scene::ArcTypeFilterMulti[4], &uiArcTypeSearch);
			FilteredToggle(Box, X("Rocketeer"), &Cfg::Scene::ArcTypeFilterMulti[5], &uiArcTypeSearch);
			FilteredToggle(Box, X("Shredder"), &Cfg::Scene::ArcTypeFilterMulti[13], &uiArcTypeSearch);
		}
		if (auto Box = Section(Arcs, X("Special Units"), X("world.ai.special")))
		{
			FilteredToggle(Box, X("Fireball"), &Cfg::Scene::ArcTypeFilterMulti[6], &uiArcTypeSearch);
			FilteredToggle(Box, X("Pop"), &Cfg::Scene::ArcTypeFilterMulti[7], &uiArcTypeSearch);
			FilteredToggle(Box, X("Leaper"), &Cfg::Scene::ArcTypeFilterMulti[8], &uiArcTypeSearch);
			FilteredToggle(Box, X("Spotter"), &Cfg::Scene::ArcTypeFilterMulti[9], &uiArcTypeSearch);
			FilteredToggle(Box, X("Bombardier"), &Cfg::Scene::ArcTypeFilterMulti[10], &uiArcTypeSearch);
			FilteredToggle(Box, X("Surveyor"), &Cfg::Scene::ArcTypeFilterMulti[11], &uiArcTypeSearch);
			FilteredToggle(Box, X("Snitch"), &Cfg::Scene::ArcTypeFilterMulti[12], &uiArcTypeSearch);
			FilteredToggle(Box, X("Firefly"), &Cfg::Scene::ArcTypeFilterMulti[17], &uiArcTypeSearch);
			FilteredToggle(Box, X("Comet"), &Cfg::Scene::ArcTypeFilterMulti[18], &uiArcTypeSearch);
			FilteredToggle(Box, X("Vaporizer"), &Cfg::Scene::ArcTypeFilterMulti[19], &uiArcTypeSearch);
			FilteredToggle(Box, X("Arc Assessor"), &Cfg::Scene::ArcTypeFilterMulti[20], &uiArcTypeSearch);
		}
		if (auto Box = Section(Arcs, X("Elite"), X("world.ai.elite")))
		{
			FilteredToggle(Box, X("The Queen"), &Cfg::Scene::ArcTypeFilterMulti[14], &uiArcTypeSearch);
			FilteredToggle(Box, X("Sentinel"), &Cfg::Scene::ArcTypeFilterMulti[15], &uiArcTypeSearch);
			FilteredToggle(Box, X("Matriarch"), &Cfg::Scene::ArcTypeFilterMulti[16], &uiArcTypeSearch);
		}
	}

	if (auto Global = tab->AddObject<CSubtab>(X("Global")))
	{
		if (auto Box = Section(Global, X("Global"), X("esp.global")))
		{
			Box->AddObject<CColorPicker>(X("Knocked / DBNO"), &Menu::colKnocked, true);
			Box->AddObject<CDropdown>(X("Font Size"), &fontIndex,
				std::vector<std::string>{ X("Small"), X("Medium"), X("Large") });
		}
	}
}
