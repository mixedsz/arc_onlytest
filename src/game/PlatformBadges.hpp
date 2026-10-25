#pragma once

#include "imgui.h"
#include "app/RenderDevice.hpp"
#include "assets/platform_badges_png.hpp"
#include "draws/Textures.h"

#include <vector>

inline ImTextureID GetPlatformBadgeAtlas()
{
	if (test::PlatformBadgeAtlas)
		return (ImTextureID)test::PlatformBadgeAtlas;
	if (!test::Device9)
		return nullptr;

	static std::vector<uint8_t> png;
	if (png.empty())
		png.assign(kPlatformBadgePng, kPlatformBadgePng + kPlatformBadgePngSize);

	const ImTextureID tex = CreateIconTextureDX9(test::Device9, &png);
	test::PlatformBadgeAtlas = (LPDIRECT3DTEXTURE9)tex;
	return tex;
}

inline bool DrawPlatformBadgeIcon(ImDrawList* dl, float x, float y, float size, uint8_t plat)
{
	if (!dl || size < 4.f)
		return false;

	int slot = -1;
	if (plat == 1)
		slot = 0;
	else if (plat == 3)
		slot = 1;
	else if (plat == 2)
		slot = 2;
	if (slot < 0)
		return false;

	const ImTextureID tex = GetPlatformBadgeAtlas();
	if (!tex)
		return false;

	const float u0 = (float)slot / 3.f;
	const float u1 = (float)(slot + 1) / 3.f;
	const float rounding = size * 0.18f;
	dl->AddImageRounded(tex, ImVec2(x, y), ImVec2(x + size, y + size),
		ImVec2(u0, 0.f), ImVec2(u1, 1.f), IM_COL32(255, 255, 255, 255), rounding);
	return true;
}
