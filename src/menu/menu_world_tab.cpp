#include <Cheat.hpp>
#include "menu.hpp"
#include "settings/Settings.hpp"
#include "settings/Types.h"

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

// Options come from Types.h so the dropdown order and the bit each entry maps
// to cannot drift apart from the resolver that assigns them.
static void ThreatTypeFilter(CContainer* box, const char* label, ThreatCategory cat,
	int* mask, const char* tooltip)
{
	int count = 0;
	const char* const* names = ThreatFilterNames(cat, count);

	std::vector<std::string> items;
	items.reserve((size_t)count);
	for (int i = 0; i < count; ++i)
		items.emplace_back(names[i]);

	box->AddObject<CMultiDropdown>(label, mask, items, tooltip);
}

void Menu::InitWorldTab(CTab* tab)
{
	if (auto Loot = tab->AddObject<CSubtab>(X("Loot")))
	{
		if (auto Box = Section(Loot, X("Loot"), X("world.loot")))
		{
			auto items = Box->AddObject<CCheckbox>(X("Ground Items"), &Cfg::Scene::ItemsESP);
			items->AddObject<CColorPicker>(X("Items Color"), &Menu::colItems, true);
			Box->AddObject<CCheckbox>(X("Item Icons"), &Cfg::Scene::Icon);
			Box->AddObject<CCheckbox>(X("Item Amount"), &Cfg::Scene::Amount);
			Box->AddObject<CCheckbox>(X("Item Price"), &Cfg::Scene::Price);
			Box->AddObject<CCheckbox>(X("Item Weight"), &Cfg::Scene::Weight);
			auto priceFilter = Box->AddObject<CCheckbox>(X("Price Filter"), &Cfg::Scene::Filterbyprice);
			if (auto popup = priceFilter->AddObject<CPopup>())
			{
				popup->AddObject<CDropdown>(X("Price Policy"), &Cfg::Scene::PricePolicy,
					std::vector<std::string>{ X("Min Price"), X("Max Price") });
				popup->AddObject<CSliderInt>(X("Min Price"), &Cfg::Scene::MinPrice, 0, 50000, X("{}"));
			}
			Box->AddObject<CSliderFloat>(X("Max Distance"), &Cfg::Scene::MaxDistanceItems, 10.f, 500.f, X("{:.0f}"));
			Box->AddObject<CCheckbox>(X("World Circles"), &Cfg::Scene::WorldShowCircle);
			auto bounds = Box->AddObject<CCheckbox>(X("Bounds ESP"), &Cfg::Scene::BoundsESP);
			bounds->AddObject<CColorPicker>(X("Bounds Color"), &Menu::colBounds, true);
			Box->AddObject<CCheckbox>(X("FOV Stacking"), &Cfg::Scene::FovStacking);
		}
	}

	if (auto Cont = tab->AddObject<CSubtab>(X("Containers")))
	{
		if (auto Box = Section(Cont, X("Containers"), X("world.containers")))
		{
			auto corpses = Box->AddObject<CCheckbox>(X("Player Corpses"), &Cfg::Scene::CorpsesESP);
			corpses->AddObject<CColorPicker>(X("Corpse Color"), &Menu::colCorpses, true);
			Box->AddObject<CSliderFloat>(X("Corpse Distance"), &Cfg::Scene::MaxDistanceCorpses, 50.f, 500.f, X("{:.0f}"));

			auto cache = Box->AddObject<CCheckbox>(X("Raider Cache"), &Cfg::Scene::RaiderCacheESP);
			cache->AddObject<CColorPicker>(X("Cache Color"), &Menu::colRaiderCache, true);
			Box->AddObject<CSliderFloat>(X("Cache Distance"), &Cfg::Scene::MaxDistanceRaiderCache, 50.f, 500.f, X("{:.0f}"));

			auto containers = Box->AddObject<CCheckbox>(X("Container ESP"), &Cfg::Scene::ContainersESP);
			containers->AddObject<CColorPicker>(X("Container Color"), &Menu::colContainers, true);
			Box->AddObject<CSliderFloat>(X("Max Distance"), &Cfg::Scene::MaxDistanceContainers, 50.f, 500.f, X("{:.0f}"));
			Box->AddObject<CCheckbox>(X("Show Opened Containers"), &Cfg::Scene::ContainersShowOpened);
			Box->AddObject<CCheckbox>(X("Ammo Boxes"), &Cfg::Scene::ContainerTypeFilterMulti[SceneTypes::Container::AmmoBox]);
			Box->AddObject<CCheckbox>(X("Android Containers"), &Cfg::Scene::ContainerTypeFilterMulti[SceneTypes::Container::AndroidContainer]);
		}
	}

	if (auto Salv = tab->AddObject<CSubtab>(X("Resources")))
	{
		if (auto Box = Section(Salv, X("Resources"), X("world.resources")))
		{
			auto salvage = Box->AddObject<CCheckbox>(X("Salvage ESP"), &Cfg::Scene::SalvageESP);
			salvage->AddObject<CColorPicker>(X("Salvage Color"), &Menu::colSalvage, true);
			Box->AddObject<CSliderFloat>(X("Max Distance"), &Cfg::Scene::MaxDistanceSalvage, 50.f, 500.f, X("{:.0f}"));
			Box->AddObject<CTextInput>(X("Search resources"), &uiSalvageSearch, false, true);
		}
		if (auto Box = Section(Salv, X("Plants"), X("world.resources.plants")))
		{
			FilteredToggle(Box, X("Agave"), &Cfg::Scene::SalvageTypeFilterMulti[SceneTypes::Salvage::Agave], &uiSalvageSearch);
			FilteredToggle(Box, X("Great Mullein"), &Cfg::Scene::SalvageTypeFilterMulti[SceneTypes::Salvage::GreatMullein], &uiSalvageSearch);
			FilteredToggle(Box, X("Lemon"), &Cfg::Scene::SalvageTypeFilterMulti[SceneTypes::Salvage::Lemon], &uiSalvageSearch);
			FilteredToggle(Box, X("Olive"), &Cfg::Scene::SalvageTypeFilterMulti[SceneTypes::Salvage::Olive], &uiSalvageSearch);
			FilteredToggle(Box, X("Apricot"), &Cfg::Scene::SalvageTypeFilterMulti[SceneTypes::Salvage::Apricot], &uiSalvageSearch);
		}
		if (auto Box = Section(Salv, X("Fungi"), X("world.resources.fungi")))
		{
			FilteredToggle(Box, X("Mushroom"), &Cfg::Scene::SalvageTypeFilterMulti[SceneTypes::Salvage::Mushroom], &uiSalvageSearch);
			FilteredToggle(Box, X("Mushroom Cluster"), &Cfg::Scene::SalvageTypeFilterMulti[SceneTypes::Salvage::MushroomCluster], &uiSalvageSearch);
		}
		if (auto Box = Section(Salv, X("Materials"), X("world.resources.materials")))
		{
			FilteredToggle(Box, X("Moss Rock"), &Cfg::Scene::SalvageTypeFilterMulti[SceneTypes::Salvage::MossRock], &uiSalvageSearch);
			FilteredToggle(Box, X("Fertilizer"), &Cfg::Scene::SalvageTypeFilterMulti[SceneTypes::Salvage::Fertilizer], &uiSalvageSearch);
		}
	}

	if (auto Dep = tab->AddObject<CSubtab>(X("Threats")))
	{
		if (auto Box = Section(Dep, X("Threats"), X("world.threats")))
		{
			auto grenades = Box->AddObject<CCheckbox>(X("Grenades"), &Cfg::Scene::GrenadeESP);
			grenades->SetBadge(Style::danger);
			grenades->AddObject<CColorPicker>(X("Grenade Color"), &Menu::colGrenade, true);
			Box->AddObject<CSliderFloat>(X("Grenade Distance"), &Cfg::Scene::MaxDistanceGrenade, 0.f, 200.f, X("{:.0f}"));
			ThreatTypeFilter(Box, X("Grenade Types"), ThreatCategory::Grenade,
				&Cfg::Scene::GrenadeTypeMask,
				X("Which grenades to show. Other/Unknown covers any grenade this build does not recognise by name"));

			auto mines = Box->AddObject<CCheckbox>(X("Mines"), &Cfg::Scene::MinesESP);
			mines->SetBadge(Color(249, 115, 22));
			mines->AddObject<CColorPicker>(X("Mines Color"), &Menu::colMines, true);
			Box->AddObject<CSliderFloat>(X("Mines Distance"), &Cfg::Scene::MaxDistanceMines, 0.f, 200.f, X("{:.0f}"));
			ThreatTypeFilter(Box, X("Mine Types"), ThreatCategory::Mine,
				&Cfg::Scene::MineTypeMask,
				X("Which mines to show. Other/Unknown covers any mine this build does not recognise by name"));

			auto traps = Box->AddObject<CCheckbox>(X("Traps"), &Cfg::Scene::TrapsESP);
			traps->SetBadge(Style::warning);
			traps->AddObject<CColorPicker>(X("Traps Color"), &Menu::colTraps, true);
			Box->AddObject<CSliderFloat>(X("Traps Distance"), &Cfg::Scene::MaxDistanceTraps, 0.f, 200.f, X("{:.0f}"));
			ThreatTypeFilter(Box, X("Trap Types"), ThreatCategory::Trap,
				&Cfg::Scene::TrapTypeMask,
				X("Which traps to show. Other/Unknown covers any trap this build does not recognise by name"));

			auto probes = Box->AddObject<CCheckbox>(X("Arc Probes"), &Cfg::Scene::ArcProbesESP);
			probes->SetBadge(Color(59, 130, 246));
			probes->AddObject<CColorPicker>(X("Probe Color"), &Menu::colArcProbes, true);
			Box->AddObject<CSliderFloat>(X("Probe Distance"), &Cfg::Scene::MaxDistanceArcProbes, 50.f, 500.f, X("{:.0f}"));
		}
	}

	if (auto Ext = tab->AddObject<CSubtab>(X("Extracts")))
	{
		if (auto Box = Section(Ext, X("Extracts"), X("world.extracts")))
		{
			auto extractions = Box->AddObject<CCheckbox>(X("Extract ESP"), &Cfg::Scene::ExtractionsESP);
			extractions->AddObject<CColorPicker>(X("Extraction Color"), &Menu::colExtractions, true);
			Box->AddObject<CCheckbox>(X("Time Left"), &Cfg::Scene::ExtractionTimeLeft);
			Box->AddObject<CCheckbox>(X("Show Icon"), &Cfg::Scene::showExtractionIcon);
		}
		if (auto Box = Section(Ext, X("Fast"), X("world.extracts.fast")))
		{
			Box->AddObject<CCheckbox>(X("Elevator"), &Cfg::Scene::ExtractionTypeFilterMulti[0]);
			Box->AddObject<CCheckbox>(X("Hatch"), &Cfg::Scene::ExtractionTypeFilterMulti[3]);
			Box->AddObject<CCheckbox>(X("Fan"), &Cfg::Scene::ExtractionTypeFilterMulti[1]);
		}
		if (auto Box = Section(Ext, X("Standard"), X("world.extracts.standard")))
		{
			Box->AddObject<CCheckbox>(X("Metro"), &Cfg::Scene::ExtractionTypeFilterMulti[5]);
			Box->AddObject<CCheckbox>(X("Gaspipe"), &Cfg::Scene::ExtractionTypeFilterMulti[2]);
		}
		if (auto Box = Section(Ext, X("Priority"), X("world.extracts.priority")))
		{
			Box->AddObject<CCheckbox>(X("Major"), &Cfg::Scene::ExtractionTypeFilterMulti[4]);
			Box->AddObject<CCheckbox>(X("Minor"), &Cfg::Scene::ExtractionTypeFilterMulti[6]);
		}
		if (auto Box = Section(Ext, X("Unknown"), X("world.extracts.unknown")))
		{
			Box->AddObject<CCheckbox>(X("Base"), &Cfg::Scene::ExtractionTypeFilterMulti[7]);
			Box->AddObject<CCheckbox>(X("Other / Unknown"), &Cfg::Scene::ExtractionTypeFilterMulti[8]);
		}
	}
}
