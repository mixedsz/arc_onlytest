#pragma once
#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <iostream>
#include <windows.h>
#include <d3d11.h>
#include <tchar.h>
#include <dwmapi.h>
#include <map>
#include <algorithm>
#include <array>
#include <vector>
#include <functional>

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_freetype.h"
#include "imgui_impl_win32.h"
#include "imgui_impl_dx9.h"
#include "fonts.h"
#include "Font2.h"
#include "draws/Textures.h"

inline bool panel_visible = false;
inline bool toggle_held = false;

constexpr float SCALE = 0.75f;
#define SCALED(x) ((x) * SCALE)


using namespace ImGui;


#define DXBG

#define _C(ARG) (CalcTextSize(ARG))
#define _CO(ARG) (GetColorU32(ARG))
#define _S(ARG) (sizeof(ARG) / sizeof(ARG[0]))

#define _V2 ImVec2
#define _V4 ImVec4
#define RGBA ImColor


#define EFONT(PDATA, FSIZE, CFG) \
    (io.Fonts->AddFontFromMemoryTTF(PDATA, sizeof(PDATA), FSIZE, &CFG))


inline float vtime(float speed) noexcept { return GetIO().DeltaTime * 5; }
inline float animSpeed = 5.f;


namespace wnd
{
    namespace panel
    {
        inline _V2 size = { SCALED(950.f), SCALED(665.f) };
        inline _V2 SizeOnChange = size;
        inline float rounding = SCALED(17.f);
    }

    namespace gfx
    {
        inline HWND hwnd;
#ifdef DXBG
        inline _V2 size = { 1720, 960 };
#else
        inline _V2 &size = wnd::panel::size;
#endif
    }
}


namespace ui
{
    void BeforeLoop(ID3D11Device* dvc);
    void Render();
}

struct Fonts {
    std::vector<float> PoppinsMediumS = { 16, 18, 14 };
    std::vector<float> InterMediumS = { 14, 16, 36 };
    std::vector<float> iconsS = { 16, 26 };
    std::vector<float> CustomFontS = { 15, 22, 28 };

    std::vector<ImFont*> poppins = {  };
    std::vector<ImFont*> inter = {  };
    std::vector<ImFont*> icons = {  };
    std::vector<ImFont*> custom = { };


    auto RenderFonts() noexcept -> void;
}; inline Fonts f;

inline auto Fonts::RenderFonts() noexcept -> void {
    ImGuiIO& io = GetIO();
    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint | ImGuiFreeTypeBuilderFlags_LightHinting | ImGuiFreeTypeBuilderFlags_LoadColor;

    for (int i = 0; i < PoppinsMediumS.size(); i++)
        poppins.push_back(EFONT(__PoppinsMedium, SCALED(PoppinsMediumS[i] + 6), cfg));

    for (int i = 0; i < InterMediumS.size(); i++)
        inter.push_back(EFONT(__InterMedium, SCALED(InterMediumS[i] + 3), cfg));

    for (int i = 0; i < iconsS.size(); i++)
        icons.push_back(EFONT(__Icons, SCALED(iconsS[i]), cfg));

    for (int i = 0; i < CustomFontS.size(); i++) {
        this->custom.push_back(io.Fonts->AddFontFromMemoryTTF(
            (void*)rawData,
            rawDataSize,
            SCALED(CustomFontS[i]),
            &cfg
        ));
    }


}


namespace images {
    inline ID3D11ShaderResourceView* texture;
    inline ID3D11ShaderResourceView* logo;
}

enum tabs {
    aimbot,
    visuals,
    world,
    _misc,
    settings
};

class __Fade__ {
public:
    int index, tab;
    float alpha;

    auto render() noexcept -> bool {
        if (alpha == 0)
            tab = index;

        time = vtime(5);
        return alpha = std::clamp(alpha + (time * (index == tab ? 1.f : -1.f)), 0.f, 1.f);
    }
private:
    float time;
};

