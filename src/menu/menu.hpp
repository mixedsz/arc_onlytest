#pragma once

#include <Cheat.hpp>
#include <memory>

class CTab;
class CListbox;

namespace Menu
{
	void Init();
	void Tick();
	void Shutdown();

	void InitCombatTab(CTab* tab);
	void InitVisualsTab(CTab* tab);
	void InitWorldTab(CTab* tab);
	void InitRadarTab(CTab* tab);
	void InitHudTab(CTab* tab);
	void InitWarningsTab(CTab* tab);
	void InitMiscTab(CTab* tab);
	void InitConfigsTab(CTab* tab);

	inline bool isOpen = false;
	inline std::unique_ptr<CWindow> window;

	inline MenuKeybind aimKeybind{};
	inline MenuKeybind arcAimKeybind{};
	inline MenuKeybind arcWeakspotKeybind{};
	inline MenuKeybind zoomKeybind{};
	inline MenuKeybind combatModeKeybind{};

	inline Color colFov{};
	inline Color colLine{};
	inline Color colArcFov{};
	inline Color colArcLine{};
	inline Color colSkeleton{};
	inline Color colSkeletonInvisible{};
	inline Color colHeadDot{};
	inline Color colBox{};
	inline Color colFilled{};
	inline Color colSnapline{};
	inline Color colUsername{};
	inline Color colDistance{};
	inline Color colSquad{};
	inline Color colHeld{};
	inline Color colMenu{};
	inline Color colInvisible{};
	inline Color colKnocked{};
	inline Color colHealthHigh{};
	inline Color colHealthMed{};
	inline Color colHealthLow{};
	inline Color colHealthBack{};
	inline Color colShield{};
	inline Color colItems{};
	inline Color colArcs{};
	inline Color colContainers{};
	inline Color colCorpses{};
	inline Color colRaiderCache{};
	inline Color colSalvage{};
	inline Color colGrenade{};
	inline Color colMines{};
	inline Color colTraps{};
	inline Color colExtractions{};
	inline Color colArcProbes{};
	inline Color colFlashlight{};
	inline Color colPlayerFlags{};
	inline Color colBounds{};
	inline Color colChams{};
	inline Color colChams2{};
	inline Color colCrosshair{};
	inline Color colCrosshairOutline{};
	inline Color colCrosshairAccent{};
	inline Color colHud{};
	inline Color colWarningText{};
	inline Color colWarningBg{};
	inline Color colWarningAccent{};

	// seedBinds pulls keybind keys/modes from settings into the widgets. Only
	// correct at init and after a config load - see the note in the definition.
	void SyncFromSettings(bool seedBinds = false);
	void SyncToSettings();
}
