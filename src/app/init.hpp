#pragma once

#include <Windows.h>

#include "menu/string_encryption.hpp"
#include "gui/gui.h"
#include "gui/gui_colors.h"

namespace global {
	inline volatile LONG bStopExecution = 0;
}

inline void ApplyLocalBrandData()
{
	const ImVec4 defaultColor(
		168.0f / 255.0f,
		85.0f / 255.0f,
		247.0f / 255.0f,
		1.0f);

	g_brandInfo.brand_name = X("Syndikate Arc");
	g_brandInfo.secondary_text = X("Syndikate Arc");
	g_brandInfo.color = defaultColor;
	g_brandInfo.loaded = true;

	ui::colors::main = defaultColor;
	ui::colors::ApplyBrandTheme();
}

inline bool LocalInit()
{
	ApplyLocalBrandData();
	return true;
}
