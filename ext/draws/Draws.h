#pragma once

#include <unordered_map>
#include <vector>
#include <utility>
#include <mutex>
#include <algorithm>
#include <cstdio>
#include <cfloat>
#include <cmath>
#include <string>
#include "settings/Settings.hpp"
#include "gui/gui_colors.h"
#include "game/WorldCache.hpp"

void DrawNormalBox(int x, int y, int w, int h, ImColor color, int thickness, bool Outlined)
{
	ImDrawList* p = ImGui::GetBackgroundDrawList();

	int outlineThickness = thickness + 0.5;
	ImColor blackColor = ImColor(0, 0, 0, 255);

	if (Outlined)
	{
		p->AddLine(ImVec2(x - 1, y - 1), ImVec2(x + w + 1, y - 1), blackColor, outlineThickness);
		p->AddLine(ImVec2(x - 1, y - 1), ImVec2(x - 1, y + h + 1), blackColor, outlineThickness);
		p->AddLine(ImVec2(x + w + 1, y - 1), ImVec2(x + w + 1, y + h + 1), blackColor, outlineThickness);
		p->AddLine(ImVec2(x - 1, y + h + 1), ImVec2(x + w + 1, y + h + 1), blackColor, outlineThickness);
	}

	p->AddLine(ImVec2(x, y), ImVec2(x + w, y), color, thickness);

	p->AddLine(ImVec2(x, y), ImVec2(x, y + h), color, thickness);

	p->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + h), color, thickness);

	p->AddLine(ImVec2(x, y + h), ImVec2(x + w, y + h), color, thickness);
}



void Draw3DBox(ImVec2 topFrontLeft, ImVec2 topFrontRight, ImVec2 topBackLeft, ImVec2 topBackRight,
	ImVec2 bottomFrontLeft, ImVec2 bottomFrontRight, ImVec2 bottomBackLeft, ImVec2 bottomBackRight,
	ImColor color, int thickness)
{
	ImDrawList* p = ImGui::GetBackgroundDrawList();


	p->AddLine(topFrontLeft, topFrontRight, color, thickness);
	p->AddLine(topFrontRight, topBackRight, color, thickness);
	p->AddLine(topBackRight, topBackLeft, color, thickness);
	p->AddLine(topBackLeft, topFrontLeft, color, thickness);


	p->AddLine(bottomFrontLeft, bottomFrontRight, color, thickness);
	p->AddLine(bottomFrontRight, bottomBackRight, color, thickness);
	p->AddLine(bottomBackRight, bottomBackLeft, color, thickness);
	p->AddLine(bottomBackLeft, bottomFrontLeft, color, thickness);


	p->AddLine(topFrontLeft, bottomFrontLeft, color, thickness);
	p->AddLine(topFrontRight, bottomFrontRight, color, thickness);
	p->AddLine(topBackLeft, bottomBackLeft, color, thickness);
	p->AddLine(topBackRight, bottomBackRight, color, thickness);
}

void DrawBox3D(int x, int y, int w, int h, int depth, ImColor color, int thickness)
{
	ImVec2 topFrontLeft(x, y);
	ImVec2 topFrontRight(x + w, y);
	ImVec2 topBackLeft(x - depth, y - depth);
	ImVec2 topBackRight(x + w - depth, y - depth);

	ImVec2 bottomFrontLeft(x, y + h);
	ImVec2 bottomFrontRight(x + w, y + h);
	ImVec2 bottomBackLeft(x - depth, y + h - depth);
	ImVec2 bottomBackRight(x + w - depth, y + h - depth);


	Draw3DBox(topFrontLeft, topFrontRight, topBackLeft, topBackRight,
		bottomFrontLeft, bottomFrontRight, bottomBackLeft, bottomBackRight,
		color, thickness);
}


void DrawFilledRect(int x, int y, int w, int h, const ImColor color)
{
	ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), color, 0, 0);
}


void DrawCornerBox(int x, int y, int w, int h, ImColor Color, int thickness, bool Outlined)
{
	float lineW = (w / 6.f);
	float lineH = (h / 6.f);
	ImDrawList* p = ImGui::GetBackgroundDrawList();

	int outlineThickness = thickness + 0.5;
	ImColor blackColor = ImColor(0, 0, 0, 255);

	if (Outlined)
	{
		p->AddLine(ImVec2(x - 1, y - 1), ImVec2(x - 1, y + lineH + 1), blackColor, outlineThickness);
		p->AddLine(ImVec2(x - 1, y - 1), ImVec2(x + lineW + 1, y - 1), blackColor, outlineThickness);

		p->AddLine(ImVec2(x + w - lineW - 1, y - 1), ImVec2(x + w + 1, y - 1), blackColor, outlineThickness);
		p->AddLine(ImVec2(x + w + 1, y - 1), ImVec2(x + w + 1, y + lineH + 1), blackColor, outlineThickness);


		p->AddLine(ImVec2(x - 1, y + h - lineH - 1), ImVec2(x - 1, y + h + 1), blackColor, outlineThickness);
		p->AddLine(ImVec2(x - 1, y + h + 1), ImVec2(x + lineW + 1, y + h + 1), blackColor, outlineThickness);


		p->AddLine(ImVec2(x + w - lineW - 1, y + h + 1), ImVec2(x + w + 1, y + h + 1), blackColor, outlineThickness);
		p->AddLine(ImVec2(x + w + 1, y + h - lineH - 1), ImVec2(x + w + 1, y + h + 1), blackColor, outlineThickness);
	}



	p->AddLine(ImVec2(x, y), ImVec2(x, y + lineH), Color, thickness);
	p->AddLine(ImVec2(x, y), ImVec2(x + lineW, y), Color, thickness);


	p->AddLine(ImVec2(x + w - lineW, y), ImVec2(x + w, y), Color, thickness);
	p->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + lineH), Color, thickness);


	p->AddLine(ImVec2(x, y + h - lineH), ImVec2(x, y + h), Color, thickness);
	p->AddLine(ImVec2(x, y + h), ImVec2(x + lineW, y + h), Color, thickness);


	p->AddLine(ImVec2(x + w - lineW, y + h), ImVec2(x + w, y + h), Color, thickness);
	p->AddLine(ImVec2(x + w, y + h - lineH), ImVec2(x + w, y + h), Color, thickness);
}

void AddOutlinedText(ImDrawList* drawList, const ImFont* font, float fontSize, const ImVec2& position, const ImColor& textColor, const ImColor& outlineColor, const char* text)
{
	const float outlineOffset = 1.0f;
	drawList->AddText(font, fontSize, ImVec2(position.x - outlineOffset, position.y - outlineOffset), outlineColor, text);
	drawList->AddText(font, fontSize, ImVec2(position.x + outlineOffset, position.y - outlineOffset), outlineColor, text);
	drawList->AddText(font, fontSize, ImVec2(position.x - outlineOffset, position.y + outlineOffset), outlineColor, text);
	drawList->AddText(font, fontSize, ImVec2(position.x + outlineOffset, position.y + outlineOffset), outlineColor, text);

	drawList->AddText(font, fontSize, position, textColor, text);
}


ImColor GetRainbowColor(float speed) {

	float hue = fmod(ImGui::GetTime() * speed, 1.0f);
	float r, g, b;
	ImGui::ColorConvertHSVtoRGB(hue, 1.0f, 1.0f, r, g, b);
	return ImColor(r, g, b, 1.0f);
}


static bool draggingRadar = false;
static ImVec2 dragOffsetRadar{};
static std::unordered_map<uint64_t, ImVec2> prevRadarPos;

void DrawStaticRadar(const Vector3& localPos, float viewAngleDeg)
{
	constexpr float scaleFactor = 1.20f;
	const float radarRadius = radarSize * scaleFactor;
	ImVec2 radarCenter = { radarPosX + radarRadius, radarPosY + radarRadius };

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	if (!dl) return;

	{
		ImU32 bgFill = ImGui::ColorConvertFloat4ToU32(ImVec4(0.07f, 0.07f, 0.09f, 0.92f));
		ImU32 ringCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
			ui::colors::main.x, ui::colors::main.y, ui::colors::main.z, 0.55f));
		dl->AddCircleFilled(radarCenter, radarRadius, bgFill, 48);
		dl->AddCircle(radarCenter, radarRadius, ringCol, 48, 1.8f);
	}

	const ImU32 crossCol = IM_COL32(60, 50, 90, 120);
	const float step = radarRadius * 0.2f;
	float fadeStart = radarRadius * 0.75f, fadeEnd = radarRadius;

	for (float x = -radarRadius; x < radarRadius; x += 4.0f) {
		float dist = std::abs(x);
		int alpha = (dist >= fadeStart)
			? (int)(200 * (1.0f - ((dist - fadeStart) / (fadeEnd - fadeStart))))
			: 200;
		dl->AddLine({ radarCenter.x + x, radarCenter.y },
			{ radarCenter.x + x + 4.f, radarCenter.y },
			IM_COL32(0, 0, 0, alpha), 1.0f);
	}

	for (float y = -radarRadius; y < radarRadius; y += 4.0f) {
		float dist = std::abs(y);
		int alpha = (dist >= fadeStart)
			? (int)(200 * (1.0f - ((dist - fadeStart) / (fadeEnd - fadeStart))))
			: 200;
		dl->AddLine({ radarCenter.x, radarCenter.y + y },
			{ radarCenter.x, radarCenter.y + y + 4.f },
			IM_COL32(0, 0, 0, alpha), 1.0f);
	}

	for (float x = radarCenter.x - radarRadius + step; x < radarCenter.x + radarRadius - step; x += step)
		dl->AddLine({ x, radarCenter.y - 4 }, { x, radarCenter.y + 4 }, crossCol, 1.0f);

	for (float y = radarCenter.y - radarRadius + step; y < radarCenter.y + radarRadius - step; y += step)
		dl->AddLine({ radarCenter.x - 4, y }, { radarCenter.x + 4, y }, crossCol, 1.0f);

	float len = radarRadius * 0.04f, wid = radarRadius * 0.025f;
	{
		ImU32 triCol = ImGui::ColorConvertFloat4ToU32(ImVec4(
			ui::colors::main_light.x, ui::colors::main_light.y, ui::colors::main_light.z, 0.95f));
		dl->AddTriangleFilled(
			{ radarCenter.x, radarCenter.y - len },
			{ radarCenter.x - wid, radarCenter.y + len * 0.6f },
			{ radarCenter.x + wid, radarCenter.y + len * 0.6f },
			triCol);
	}

	if (!NT::Store::Scene) return;
	auto* _radarBuf = NT::Store::Scene->AcquirePlayerBuffer();
	struct _RBufG { NT::Store::CScene::PlayerBuffer*& b; ~_RBufG() { if (b) { b->Release(); b = nullptr; } } } _rbg{_radarBuf};
	static const std::vector<PlayerDynamicData> _emptyRadar;
	const auto& cache = _radarBuf ? _radarBuf->data : _emptyRadar;

	const float worldScale = radarRadius / 250.0f;
	const float yawRad = DEG2RAD(viewAngleDeg);
	const float cosA = cosf(yawRad);
	const float sinA = sinf(yawRad);

	ImFont* textFont = ImGui::GetFont();
	const float labelSize = (std::max)(10.0f, radarRadius * 0.085f);

	for (const auto& pd : cache)
	{
		if (pd.npc || pd.drone || pd.loot || pd.container || pd.extraction ||
			pd.corpse || pd.raidercache || pd.salvage || pd.grenade ||
			pd.mine || pd.trap || pd.arcprobe)
			continue;
		if (pd.is_team)
			continue;

		float dx = (pd.worldLoc.x - localPos.x) / 100.0f;
		float dy = (pd.worldLoc.y - localPos.y) / 100.0f;
		const float distM = hypotf(dx, dy);

		float forward = cosA * dx + sinA * dy;
		float right   = -sinA * dx + cosA * dy;

		float sx = radarCenter.x + right * worldScale;
		float sy = radarCenter.y - forward * worldScale;

		float distPx = hypotf(sx - radarCenter.x, sy - radarCenter.y);
		if (distPx > radarRadius) {
			float f = radarRadius / distPx;
			sx = radarCenter.x + (sx - radarCenter.x) * f;
			sy = radarCenter.y + (sy - radarCenter.y) * f;
		}

		ImVec2 newPos{ sx, sy };
		ImVec2 finalPos = newPos;
		auto it = prevRadarPos.find(pd.entity);
		if (it != prevRadarPos.end()) {
			ImVec2 oldPos = it->second;
			finalPos.x = oldPos.x + (newPos.x - oldPos.x) * 0.4f;
			finalPos.y = oldPos.y + (newPos.y - oldPos.y) * 0.4f;
		}
		prevRadarPos[pd.entity] = finalPos;

		ImU32 dotCol = ImGui::ColorConvertFloat4ToU32(ImVec4(0.95f, 0.30f, 0.35f, 1.0f));
		float enemySize = radarRadius * 0.012f + 1.5f;

		dl->AddCircleFilled(finalPos, enemySize, dotCol, 6);

		if ((radarShowName || radarShowDistance) && textFont) {
			char label[96];
			label[0] = '\0';

			if (radarShowName) {
				std::string shortName = pd.name.empty() ? "?" : pd.name;
				if (shortName.size() > 16)
					shortName = shortName.substr(0, 13) + "...";

				if (radarShowDistance)
					snprintf(label, sizeof(label), "%s [%dm]", shortName.c_str(), (int)distM);
				else
					snprintf(label, sizeof(label), "%s", shortName.c_str());
			}
			else if (radarShowDistance) {
				snprintf(label, sizeof(label), "%dm", (int)distM);
			}

			if (label[0]) {
				ImVec2 textSize = textFont->CalcTextSizeA(labelSize, FLT_MAX, 0.0f, label);
				ImVec2 textPos{ finalPos.x + enemySize + 3.0f, finalPos.y - textSize.y * 0.5f };

				if (textPos.x + textSize.x > radarCenter.x + radarRadius + 40.0f)
					textPos.x = finalPos.x - enemySize - 3.0f - textSize.x;
				if (textPos.y < radarCenter.y - radarRadius - 20.0f)
					textPos.y = radarCenter.y - radarRadius - 20.0f;
				if (textPos.y + textSize.y > radarCenter.y + radarRadius + 20.0f)
					textPos.y = radarCenter.y + radarRadius + 20.0f - textSize.y;

				AddOutlinedText(dl, textFont, labelSize, textPos,
					ImColor(dotCol), ImColor(0, 0, 0, 220), label);
			}
		}
	}

	if (draggingRadar || radarDragPosition) {
		ImGuiIO& io = ImGui::GetIO();
		ImVec2 mousePos = io.MousePos;
		ImVec2 tl{ radarCenter.x - radarRadius, radarCenter.y - radarRadius };
		ImVec2 br{ radarCenter.x + radarRadius, radarCenter.y + radarRadius };

		if (radarDragPosition) {
			dl->AddCircle(radarCenter, radarRadius + 3.0f, IM_COL32(168, 85, 247, 180), 48, 2.0f);
			if (textFont) {
				const char* hint = "Drag radar to reposition";
				ImVec2 ts = textFont->CalcTextSizeA(13.0f, FLT_MAX, 0.0f, hint);
				AddOutlinedText(dl, textFont, 13.0f,
					ImVec2(radarCenter.x - ts.x * 0.5f, radarCenter.y + radarRadius + 8.0f),
					ImColor(180, 140, 220, 255), ImColor(0, 0, 0, 220), hint);
			}
		}

		if (ImGui::IsMouseClicked(0) &&
			mousePos.x >= tl.x && mousePos.x <= br.x &&
			mousePos.y >= tl.y && mousePos.y <= br.y)
		{
			draggingRadar = true;
			dragOffsetRadar = { mousePos.x - radarPosX, mousePos.y - radarPosY };
		}

		if (draggingRadar && ImGui::IsMouseDown(0)) {
			radarPosX = mousePos.x - dragOffsetRadar.x;
			radarPosY = mousePos.y - dragOffsetRadar.y;
		}
		else if (!ImGui::IsMouseDown(0)) {
			draggingRadar = false;
		}
	}
}
struct ArrowTarget
{
	bool isVisible;
	Vector2 headScreen;
};

void DrawEnemyArrows(const std::vector<ArrowTarget>& arrows,
	const ImVec2& center,
	float fovR,
	float screenW, float screenH)
{
	if (!offarrows)
		return;

	ImDrawList* dl = ImGui::GetBackgroundDrawList();
	float offset = 50.f;

	for (const auto& t : arrows)
	{
		ImVec2 head2D(t.headScreen.x, t.headScreen.y);

		ImVec2 dir(head2D.x - center.x, head2D.y - center.y);
		float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
		if (len < 1.0f)
			continue;

		if (len < fovR)
			continue;

		ImVec2 norm(dir.x / len, dir.y / len);

		float radius = fovR + offset;

		ImVec2 arrowTip(
			center.x + norm.x * radius,
			center.y + norm.y * radius
		);

		float osc = sinf(ImGui::GetTime() * 3.f) * 5.f;
		ImVec2 perp(-norm.y, norm.x);
		arrowTip.x += perp.x * osc;
		arrowTip.y += perp.y * osc;

		float arrowLen = 12.f;
		float arrowWidth = 7.f;

		ImVec2 base(
			arrowTip.x - norm.x * arrowLen,
			arrowTip.y - norm.y * arrowLen
		);

		ImVec2 left(
			base.x + perp.x * (arrowWidth * 0.5f),
			base.y + perp.y * (arrowWidth * 0.5f)
		);

		ImVec2 right(
			base.x - perp.x * (arrowWidth * 0.5f),
			base.y - perp.y * (arrowWidth * 0.5f)
		);

		ImU32 col = t.isVisible
			? ToU32(colpick2_generic)
			: ToU32(col_invisible);

		dl->AddTriangleFilled(arrowTip, left, right, col);
	}
}

// Bearing-based warning arrow. Independent of offarrows / ArrowTarget / vis colors.
// 0 deg = camera forward (screen up), +right, -left. Does not read game memory.
void DrawEnemyArrows(ImDrawList* dl,
	float bearingDeg,
	const ImVec2& center,
	float ringRadius,
	ImU32 color,
	float scale,
	bool pulse)
{
	if (!dl)
		return;
	if (!std::isfinite(bearingDeg) || !std::isfinite(ringRadius) || ringRadius < 8.f)
		return;
	if (!std::isfinite(center.x) || !std::isfinite(center.y))
		return;
	if (!std::isfinite(scale) || scale <= 0.f)
		return;

	const float rad = bearingDeg * (float)(M_PI / 180.0);
	ImVec2 dir(sinf(rad), -cosf(rad));
	const float len = sqrtf(dir.x * dir.x + dir.y * dir.y);
	if (!std::isfinite(len) || len < 1.0e-4f)
		return;
	dir.x /= len;
	dir.y /= len;

	ImVec2 arrowTip(center.x + dir.x * ringRadius, center.y + dir.y * ringRadius);

	if (pulse)
	{
		const float osc = sinf((float)ImGui::GetTime() * 3.f) * 5.f * scale;
		const ImVec2 perp(-dir.y, dir.x);
		arrowTip.x += perp.x * osc;
		arrowTip.y += perp.y * osc;
	}

	const ImVec2 display = ImGui::GetIO().DisplaySize;
	const float margin = 8.f;
	if (display.x > 16.f && display.y > 16.f)
	{
		arrowTip.x = std::clamp(arrowTip.x, margin, display.x - margin);
		arrowTip.y = std::clamp(arrowTip.y, margin, display.y - margin);
	}

	const float arrowLen = 12.f * scale;
	const float arrowWidth = 7.f * scale;
	const ImVec2 perp(-dir.y, dir.x);
	const ImVec2 base(arrowTip.x - dir.x * arrowLen, arrowTip.y - dir.y * arrowLen);
	const ImVec2 left(base.x + perp.x * (arrowWidth * 0.5f), base.y + perp.y * (arrowWidth * 0.5f));
	const ImVec2 right(base.x - perp.x * (arrowWidth * 0.5f), base.y - perp.y * (arrowWidth * 0.5f));

	if (!std::isfinite(arrowTip.x) || !std::isfinite(arrowTip.y) ||
		!std::isfinite(left.x) || !std::isfinite(left.y) ||
		!std::isfinite(right.x) || !std::isfinite(right.y))
		return;

	dl->AddTriangleFilled(arrowTip, left, right, color);
}
