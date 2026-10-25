#pragma once

struct ImDrawList;

// Overlay HUD - crosshair and a corner info panel. Reads no game memory, so it is
// driven from the render loop in main.cpp rather than from FrameDispatch: that keeps
// it alive in the lobby and immune to the early returns inside the ESP path.
namespace Hud
{
	void Draw(ImDrawList* dl);
	// Shared live/preview drawer. (cx, cy) is the logical reticle origin in draw-list
	// space. fitRadius > 0 uniformly scales the local reticle to fit; it does not
	// write settings. Style 0 draws nothing (preview text is the menu's job).
	void DrawCrosshairAt(ImDrawList* dl, float cx, float cy, float fitRadius = 0.f);
}
