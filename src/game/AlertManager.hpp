#pragma once

struct ImDrawList;

// Cached-data warning banners and a bearing-based direction indicator.
// Driven from the present thread in main.cpp after Hud::Draw. Reads no game
// memory: PlayerBuffer is acquired, value-only Threat records are copied, the
// buffer is released, then evaluation and drawing run on that extract.
namespace Alerts
{
	void Tick(ImDrawList* dl, float deltaTime);
	void Reset();
}
