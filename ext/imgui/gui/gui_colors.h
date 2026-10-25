#pragma once

#include <imgui.h>

namespace ui
{
    namespace colors
    {
        inline ImVec4 window_border = ImColor(78, 80, 88, 38);
        inline ImVec4 inner_border = ImColor(255, 255, 255, 6);
        inline ImVec4 background = ImColor(16, 17, 21, 255);
        inline ImVec4 background_dark = ImColor(16, 17, 21, 255);
        inline ImVec4 background_light = ImColor(20, 21, 25, 255);
        inline ImVec4 sidebar_bg = ImColor(20, 21, 25, 255);
        inline ImVec4 section_bg = ImColor(20, 21, 25, 255);
        inline ImVec4 content_bg = ImColor(16, 17, 21, 255);
        inline ImVec4 outline = inner_border;
        inline ImVec4 separator = ImColor(255, 255, 255, 9);
        inline ImVec4 main = ImColor(100, 65, 185, 255);
        inline ImVec4 main_dark = ImColor(80, 50, 155, 255);
        inline ImVec4 main_light = ImColor(130, 95, 210, 255);
        inline ImVec4 text = ImColor(228, 228, 236, 255);
        inline ImVec4 text_disabled = ImColor(95, 95, 112, 255);
        inline ImVec4 text_sidebar = ImColor(108, 108, 125, 255);
        inline ImVec4 text_sidebar_active = ImColor(100, 65, 185, 255);
        inline ImVec4 warning = ImColor(200, 60, 60, 255);

        namespace child
        {
            inline ImVec4 top_background = ImColor(20, 21, 25, 255);
            inline ImVec4 outline = inner_border;
        };

        namespace toggle
        {
            inline ImVec4 track_off = ImColor(32, 33, 40, 255);
            inline ImVec4 track_on = ImColor(100, 65, 185, 255);
            inline ImVec4 thumb_off = ImColor(90, 90, 105, 255);
            inline ImVec4 thumb_on = ImColor(210, 210, 220, 255);
            inline float rounding = 30;
        }

        namespace checkbox
        {
            inline ImVec4 circle_inactive = ImColor(32, 33, 33, 255);
            inline ImVec4 background = ImColor(16, 17, 21, 255);
            inline ImVec4 outline_background = inner_border;
            inline float rounding = 30;
        }

        namespace slider
        {
            inline ImVec4 background = ImColor(16, 17, 21, 255);
            inline ImVec4 outline_background = inner_border;
            inline ImVec4 grabber = ImColor(100, 65, 185, 255);
            inline ImVec4 fill = ImColor(100, 65, 185, 255);
        }

        namespace combo
        {
            inline ImVec4 background = ImColor(28, 29, 35, 255);
            inline ImVec4 outline_background = inner_border;
        }

        namespace keybind
        {
            inline ImVec4 background = ImColor(28, 29, 35, 255);
            inline ImVec4 outline_background = inner_border;
        }

        namespace button
        {
            inline ImVec4 background = ImColor(28, 29, 35, 255);
            inline ImVec4 background_light = ImColor(32, 33, 40, 255);
            inline ImVec4 outline_background = inner_border;
        }

        namespace input
        {
            inline ImVec4 background = ImColor(28, 29, 35, 255);
            inline ImVec4 background_light = ImColor(32, 33, 40, 255);
            inline ImVec4 outline_background = inner_border;
        }

        namespace picker
        {
            inline ImVec4 background = ImColor(16, 17, 21, 255);
            inline ImVec4 outline_background = inner_border;
        }

        namespace gear_popup
        {
            inline ImVec4 background = ImColor(18, 19, 24, 245);
            inline ImVec4 outline = inner_border;
            inline ImVec4 item_bg = ImColor(24, 25, 30, 255);
        }

        namespace search
        {
            inline ImVec4 background = ImColor(20, 21, 25, 255);
            inline ImVec4 outline = inner_border;
            inline ImVec4 text = ImColor(120, 120, 138, 255);
        }

        namespace dots_btn
        {
            inline ImVec4 background = ImColor(28, 29, 35, 255);
            inline ImVec4 background_hover = ImColor(35, 36, 44, 255);
        }

        namespace preset
        {
            inline ImVec4 save_btn = ImColor(100, 65, 185, 255);
            inline ImVec4 load_btn = ImColor(100, 65, 185, 255);
            inline ImVec4 delete_btn = ImColor(190, 50, 50, 255);
        }
        inline void ApplyBrandTheme()
        {
            auto clamp01 = [](float v) { return v < 0.f ? 0.f : (v > 1.f ? 1.f : v); };
            float r = main.x, g = main.y, b = main.z;

            main_dark  = ImVec4(clamp01(r * 0.75f), clamp01(g * 0.75f), clamp01(b * 0.75f), 1.0f);
            main_light = ImVec4(clamp01(r * 1.3f),  clamp01(g * 1.3f),  clamp01(b * 1.3f),  1.0f);

            text_sidebar_active = main;
            toggle::track_on    = main;
            slider::grabber     = main;
            slider::fill        = main;
            preset::save_btn    = main;
            preset::load_btn    = main;
        }
    };
};
