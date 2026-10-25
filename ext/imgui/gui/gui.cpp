#include "gui.h"
#include "gui_font.h"
#include "gui_colors.h"
#include "IconsLucide.h"
#include "lucide_font_data.h"
#include "imgui_freetype.h"
#include <chrono>
#include <string>
#include <ctime>
#include <iomanip>
#include <sstream>

static std::string str_lower(const std::string& s) {
    std::string r = s;
    std::transform(r.begin(), r.end(), r.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return r;
}

void ui::initialize_fonts()
{
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = NULL;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

    ImFontConfig cfg;
    cfg.FontDataOwnedByAtlas = false;
    cfg.FontBuilderFlags = ImGuiFreeTypeBuilderFlags_ForceAutoHint
                         | ImGuiFreeTypeBuilderFlags_LightHinting
                         | ImGuiFreeTypeBuilderFlags_LoadColor;
    cfg.GlyphExtraSpacing.x = -1.0f;

    ImFont* fallback = io.Fonts->AddFontDefault();
    const ImWchar* ranges = io.Fonts->GetGlyphRangesCyrillic();

    font.heading = io.Fonts->AddFontFromMemoryTTF(inter_semibold, sizeof(inter_semibold), 18, &cfg, ranges);
    font.body    = io.Fonts->AddFontFromMemoryTTF(inter_semibold, sizeof(inter_semibold), 14, &cfg, ranges);

    static const ImWchar lucide_ranges[] = { ICON_MIN_LC, ICON_MAX_LC, 0 };

    {
        ImFontConfig lc = cfg;
        lc.FontDataOwnedByAtlas = false;
        font.tab_icon       = io.Fonts->AddFontFromMemoryTTF((void*)lucide_font_raw, (int)lucide_font_raw_size, 18.0f, &lc, lucide_ranges);
        lc.FontDataOwnedByAtlas = false;
        font.widget_icon[0] = io.Fonts->AddFontFromMemoryTTF((void*)lucide_font_raw, (int)lucide_font_raw_size, 14.0f, &lc, lucide_ranges);
        lc.FontDataOwnedByAtlas = false;
        font.widget_icon[1] = io.Fonts->AddFontFromMemoryTTF((void*)lucide_font_raw, (int)lucide_font_raw_size, 18.0f, &lc, lucide_ranges);
        lc.FontDataOwnedByAtlas = false;
        font.widget_icon[2] = io.Fonts->AddFontFromMemoryTTF((void*)lucide_font_raw, (int)lucide_font_raw_size, 12.0f, &lc, lucide_ranges);
    }

    if (!font.heading) font.heading = fallback;
    if (!font.body)    font.body    = fallback;
    if (!font.tab_icon) font.tab_icon = fallback;
    for (auto& fi : font.widget_icon)
        if (!fi) fi = fallback;

    font.inter_semibold[0] = font.heading;
    font.inter_semibold[1] = font.body;
    font.spacegrotesk_medium[0] = font.heading;
    font.spacegrotesk_medium[1] = font.body;

    io.FontDefault = font.body;
}

void ui::items::text_shadow(ImDrawList* draw_list, ImVec2 pos, ImColor text_color, const char* text)
{
    ImColor shadow_color = IM_COL32(0, 0, 0, (int)(text_color.Value.w * 180));
    draw_list->AddText(ImVec2(pos.x + 1, pos.y + 1), shadow_color, text);
    draw_list->AddText(pos, text_color, text);
}

SidebarCategory& ui::add_category(const char* name, const char* icon)
{
    sidebar_categories.push_back(SidebarCategory{ name, icon, {}, 0 });
    return sidebar_categories.back();
}

void ui::add_tab(SidebarCategory& cat, const char* name, const char* icon, std::function<void()> fn)
{
    cat.items.push_back(SidebarSubItem{ name, icon, std::move(fn) });
}

void ui::initialize_sidebar()
{
    // Old ImGui sidebar/tabs removed — menu lives in src/menu (Rust UI).
    sidebar_categories.clear();
    sidebar_icon_anim.clear();
}

void ui::render_background()
{
    ImVec2 pos = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    dl->AddRectFilled(pos, ImVec2(pos.x + ui::size.x, pos.y + ui::size.y),
        ImGui::GetColorU32(ui::colors::background), ui::rounding);

    {
        ImU32 grid_col = IM_COL32(255, 255, 255, 4);
        float grid_step = 24.0f;
        float x0 = pos.x + sidebar_items_width;
        float y0 = pos.y + topbar_icon_height;
        float x1 = pos.x + ui::size.x;
        float y1 = pos.y + ui::size.y;

        dl->PushClipRect(ImVec2(x0, y0), ImVec2(x1, y1), true);
        for (float x = x0; x < x1; x += grid_step)
            dl->AddLine(ImVec2(x, y0), ImVec2(x, y1), grid_col, 1.0f);
        for (float y = y0; y < y1; y += grid_step)
            dl->AddLine(ImVec2(x0, y), ImVec2(x1, y), grid_col, 1.0f);
        dl->PopClipRect();
    }

    dl->AddRectFilled(pos, ImVec2(pos.x + ui::size.x, pos.y + topbar_icon_height),
        ImGui::GetColorU32(ui::colors::background_dark), ui::rounding, ImDrawFlags_RoundCornersTop);

    dl->AddRectFilled(
        ImVec2(pos.x, pos.y + topbar_icon_height),
        ImVec2(pos.x + sidebar_items_width, pos.y + ui::size.y),
        ImGui::GetColorU32(ui::colors::sidebar_bg), 0.0f, ImDrawFlags_RoundCornersBottomLeft);

    dl->AddLine(
        ImVec2(pos.x + 8, pos.y + topbar_icon_height),
        ImVec2(pos.x + ui::size.x - 8, pos.y + topbar_icon_height),
        ImGui::GetColorU32(ui::colors::separator), 1.0f);

    dl->AddLine(
        ImVec2(pos.x + sidebar_items_width, pos.y + topbar_icon_height + 8),
        ImVec2(pos.x + sidebar_items_width, pos.y + ui::size.y - 8),
        ImGui::GetColorU32(ui::colors::separator), 1.0f);
}

std::string ui::get_breadcrumb()
{
    if (selected_category < 0 || selected_category >= (int)sidebar_categories.size())
        return "";
    auto& cat = sidebar_categories[selected_category];
    int idx = cat.active_item;
    if (idx < 0 || idx >= (int)cat.items.size())
        return cat.name;
    return cat.name + X(" / ") + cat.items[idx].name;
}

void ui::render_sidebar(float dt)
{
    ImVec2 pos = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    std::string filter = str_lower(search_buffer);

    const int icon_count = (int)sidebar_categories.size();
    const float icon_size = 26.0f;
    const float icon_spacing = 6.0f;

    float total_icons_w = icon_count * icon_size + (icon_count - 1) * icon_spacing;
    float icons_start_x = pos.x + (ui::size.x - total_icons_w) * 0.5f;
    float icons_cy = pos.y + topbar_icon_height * 0.5f;

    {
        const float conn_sz = 26.0f;
        const float conn_x = pos.x + 8.0f;
        const float conn_y = icons_cy - conn_sz * 0.5f;
        ImVec2 conn_min(conn_x, conn_y);
        ImVec2 conn_max(conn_x + conn_sz, conn_y + conn_sz);

        ImGui::SetCursorScreenPos(conn_min);
        ImGui::InvisibleButton("##conn_status", ImVec2(conn_sz, conn_sz));
        const bool conn_hov = ImGui::IsItemHovered();
        if (conn_hov) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        if (conn_hov) {
            ImVec4 bg = ImVec4(1.0f, 1.0f, 1.0f, 0.06f);
            dl->AddRectFilled(conn_min, conn_max, ImGui::GetColorU32(bg), 8.0f);
        }

        ImVec4 skull_col = conn_hov ? ImVec4(0.92f, 0.92f, 0.96f, 1.0f) : ui::colors::text_sidebar;
        ImGui::PushFont(font.tab_icon);
        ImVec2 sk_sz = ImGui::CalcTextSize(ICON_LC_SKULL);
        float sk_x = conn_x + (conn_sz - sk_sz.x) * 0.5f;
        float sk_y = conn_y + (conn_sz - sk_sz.y) * 0.5f;
        dl->AddText(ImVec2(sk_x, sk_y), ImGui::GetColorU32(skull_col), ICON_LC_SKULL);
        ImGui::PopFont();
    }

    for (int i = 0; i < icon_count; i++)
    {
        float ix = icons_start_x + i * (icon_size + icon_spacing);
        float iy = icons_cy - icon_size * 0.5f;
        ImVec2 icon_min(ix, iy);
        ImVec2 icon_max(ix + icon_size, iy + icon_size);

        ImGui::SetCursorScreenPos(icon_min);
        ImGui::InvisibleButton(("topbar_icon_" + std::to_string(i)).c_str(), ImVec2(icon_size, icon_size));
        bool hovered = ImGui::IsItemHovered();
        if (ImGui::IsItemClicked()) selected_category = i;
        if (hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

        float target_alpha = (selected_category == i) ? 1.0f : (hovered ? 0.6f : 0.0f);
        sidebar_icon_anim[i] = ImLerp(sidebar_icon_anim[i], target_alpha, dt * 12.0f);

        if (sidebar_icon_anim[i] > 0.01f) {
            ImVec4 bg = ui::colors::main;
            bg.w = 0.12f * sidebar_icon_anim[i];
            dl->AddRectFilled(icon_min, icon_max, ImGui::GetColorU32(bg), 8.0f);
        }

        if (selected_category == i) {
            float bar_w = 14.0f;
            float bar_x = ix + (icon_size - bar_w) * 0.5f;
            dl->AddRectFilled(
                ImVec2(bar_x, pos.y + topbar_icon_height - 2),
                ImVec2(bar_x + bar_w, pos.y + topbar_icon_height),
                ImGui::GetColorU32(ui::colors::main), 1.5f);
        }

        ImVec4 icon_col = (selected_category == i) ? ui::colors::main : ui::colors::text_sidebar;
        const char* icon_glyph = sidebar_categories[i].icon.c_str();
        ImGui::PushFont(font.tab_icon);
        ImVec2 tsz = ImGui::CalcTextSize(icon_glyph);
        dl->AddText(ImVec2(ix + (icon_size - tsz.x) * 0.5f, iy + (icon_size - tsz.y) * 0.5f),
            ImGui::GetColorU32(icon_col), icon_glyph);
        ImGui::PopFont();
    }

    const float footer_reserve = 26.0f;
    const float clip_bottom = pos.y + ui::size.y - footer_reserve;
    dl->PushClipRect(ImVec2(pos.x, pos.y + topbar_icon_height), ImVec2(pos.x + sidebar_items_width, clip_bottom), true);

    float item_y = pos.y + topbar_icon_height + 10.0f;
    const float item_x = pos.x + 3.0f;
    const float item_w = sidebar_items_width - 6.0f;
    const float item_h = 23.0f;
    const float item_spc = 5.0f;

    if (selected_category >= 0 && selected_category < (int)sidebar_categories.size())
    {
        if (filter.empty())
        {
            auto& cat = sidebar_categories[selected_category];

            ImGui::PushFont(font.heading);
            ImVec2 cat_sz = ImGui::CalcTextSize(cat.name.c_str());
            dl->AddText(ImVec2(item_x + 4, item_y), ImGui::GetColorU32(ui::colors::text), cat.name.c_str());
            ImGui::PopFont();
            item_y += cat_sz.y + 8.0f;

            for (int i = 0; i < (int)cat.items.size(); i++)
            {
                auto& item = cat.items[i];

                ImVec2 item_min(item_x, item_y);
                ImVec2 item_max(item_x + item_w, item_y + item_h);

                ImGui::SetCursorScreenPos(item_min);
                ImGui::InvisibleButton(("sidebar_item_" + std::to_string(selected_category) + "_" + std::to_string(i)).c_str(), ImVec2(item_w, item_h));
                bool item_hovered = ImGui::IsItemHovered();
                if (ImGui::IsItemClicked()) cat.active_item = i;
                if (item_hovered) ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);

                bool is_active = (cat.active_item == i);
                float target = is_active ? 1.0f : (item_hovered ? 0.5f : 0.0f);
                cat.item_anim[i] = ImLerp(cat.item_anim[i], target, dt * 12.0f);

                if (cat.item_anim[i] > 0.01f) {
                    ImVec4 bg = ui::colors::main;
                    bg.w = 0.1f * cat.item_anim[i];
                    dl->AddRectFilled(item_min, item_max, ImGui::GetColorU32(bg), 5.0f);
                }

                ImVec4 text_col = is_active ? ui::colors::text_sidebar_active : ui::colors::text_sidebar;
                if (item_hovered && !is_active) text_col = ImVec4(0.88f, 0.88f, 0.93f, 1.0f);

                ImGui::PushFont(font.widget_icon[0]);
                ImVec2 icon_sz = ImGui::CalcTextSize(item.icon.c_str());
                dl->AddText(ImVec2(item_x + 6, item_y + (item_h - icon_sz.y) * 0.5f),
                    ImGui::GetColorU32(text_col), item.icon.c_str());
                ImGui::PopFont();

                float label_x = item_x + 6 + icon_sz.x + 4.0f;
                ImGui::PushFont(font.body);
                ImVec2 name_sz = ImGui::CalcTextSize(item.name.c_str());
                dl->AddText(ImVec2(label_x, item_y + (item_h - name_sz.y) * 0.5f),
                    ImGui::GetColorU32(text_col), item.name.c_str());
                ImGui::PopFont();

                item_y += item_h + item_spc;
            }
        }
        else
        {
            ImGui::PushFont(font.heading);
            const char* rh = X("Results");
            ImVec2 rh_sz = ImGui::CalcTextSize(rh);
            dl->AddText(ImVec2(item_x + 4, item_y), ImGui::GetColorU32(ui::colors::text_disabled), rh);
            ImGui::PopFont();
            item_y += rh_sz.y + 8.0f;

            for (int ci = 0; ci < (int)sidebar_categories.size(); ci++)
            {
                auto& c = sidebar_categories[ci];
                const bool cat_match = str_lower(c.name).find(filter) != std::string::npos;
                for (int ii = 0; ii < (int)c.items.size(); ii++)
                {
                    auto& item = c.items[ii];
                    if (!cat_match && str_lower(item.name).find(filter) == std::string::npos)
                        continue;

                    ImVec2 item_min(item_x, item_y);
                    ImVec2 item_max(item_x + item_w, item_y + item_h);

                    ImGui::SetCursorScreenPos(item_min);
                    const std::string bid = "sidebar_find_" + std::to_string(ci) + "_" + std::to_string(ii);
                    ImGui::InvisibleButton(bid.c_str(), ImVec2(item_w, item_h));
                    bool item_hovered = ImGui::IsItemHovered();
                    if (ImGui::IsItemClicked()) {
                        selected_category = ci;
                        c.active_item = ii;
                    }
                    if (item_hovered) {
                        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
                        ImGui::SetTooltip("%s  >  %s", c.name.c_str(), item.name.c_str());
                    }

                    bool is_active = (selected_category == ci && c.active_item == ii);
                    float target = is_active ? 1.0f : (item_hovered ? 0.5f : 0.0f);
                    c.item_anim[ii] = ImLerp(c.item_anim[ii], target, dt * 12.0f);

                    if (c.item_anim[ii] > 0.01f) {
                        ImVec4 bg = ui::colors::main;
                        bg.w = 0.1f * c.item_anim[ii];
                        dl->AddRectFilled(item_min, item_max, ImGui::GetColorU32(bg), 5.0f);
                    }

                    ImVec4 text_col = is_active ? ui::colors::text_sidebar_active : ui::colors::text_sidebar;
                    if (item_hovered && !is_active) text_col = ImVec4(0.88f, 0.88f, 0.93f, 1.0f);

                    ImGui::PushFont(font.widget_icon[0]);
                    ImVec2 icon_sz2 = ImGui::CalcTextSize(item.icon.c_str());
                    dl->AddText(ImVec2(item_x + 6, item_y + (item_h - icon_sz2.y) * 0.5f),
                        ImGui::GetColorU32(text_col), item.icon.c_str());
                    ImGui::PopFont();

                    float lbl_x = item_x + 6 + icon_sz2.x + 4.0f;
                    ImGui::PushFont(font.body);
                    ImVec2 name_sz = ImGui::CalcTextSize(item.name.c_str());
                    dl->AddText(ImVec2(lbl_x, item_y + (item_h - name_sz.y) * 0.5f),
                        ImGui::GetColorU32(text_col), item.name.c_str());
                    ImGui::PopFont();

                    item_y += item_h + item_spc;
                }
            }
        }
    }

    dl->PopClipRect();

    {
        const char* ft_text = g_brandInfo.loaded ? g_brandInfo.brand_name.c_str() : X("Arc External");

        ImGui::PushFont(font.body);
        ImVec2 ft_sz = ImGui::CalcTextSize(ft_text);
        float ft_x = pos.x + (sidebar_items_width - ft_sz.x) * 0.5f;
        float ft_y = pos.y + ui::size.y - ft_sz.y - 9.0f;

        ImVec4 accent = ui::colors::main;
        accent.w = 0.85f;
        dl->AddText(ImVec2(ft_x, ft_y), ImGui::GetColorU32(accent), ft_text);
        ImGui::PopFont();
    }
}

void ui::render_content()
{
    ImVec2 pos = ImGui::GetWindowPos();
    ImDrawList* dl = ImGui::GetBackgroundDrawList();

    float content_x = pos.x + sidebar_items_width;
    float content_y = pos.y + topbar_icon_height;
    float content_w = ui::size.x - sidebar_items_width;
    float content_h = ui::size.y - topbar_icon_height;

    ImGui::PushFont(font.body);
    float bc_y = content_y + 10.0f;
    float bc_x = content_x + 12.0f;

    if (search_buffer[0] != '\0')
    {
        dl->AddText(ImVec2(bc_x, bc_y), ImGui::GetColorU32(ui::colors::main), X("Results"));
    }
    else if (selected_category >= 0 && selected_category < (int)sidebar_categories.size())
    {
        auto& cat = sidebar_categories[selected_category];
        std::string cat_name = cat.name;
        int item_idx = cat.active_item;

        dl->AddText(ImVec2(bc_x, bc_y), ImGui::GetColorU32(ui::colors::text_disabled), cat_name.c_str());
        if (item_idx >= 0 && item_idx < (int)cat.items.size()) {
            float name_w = ImGui::CalcTextSize(cat_name.c_str()).x;
            dl->AddText(ImVec2(bc_x + name_w, bc_y), ImGui::GetColorU32(ui::colors::text_disabled), X(" / "));
            float slash_w = ImGui::CalcTextSize(X(" / ")).x;
            dl->AddText(ImVec2(bc_x + name_w + slash_w, bc_y),
                ImGui::GetColorU32(ui::colors::main), cat.items[item_idx].name.c_str());
        }
    }

    float search_w = 130.0f;
    float search_h = 22.0f;
    float search_x = content_x + content_w - search_w - 12.0f;
    float search_y = content_y + 8.0f;

    ImGui::SetCursorScreenPos(ImVec2(search_x, search_y));
    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, (search_h - ImGui::GetFontSize()) * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 6.0f);
    ImGui::PushStyleColor(ImGuiCol_FrameBg,        IM_COL32(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, IM_COL32(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_FrameBgActive,  IM_COL32(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Border,         IM_COL32(0,0,0,0));
    ImGui::PushStyleColor(ImGuiCol_Text,           ImGui::GetColorU32(ui::colors::text));
    ImGui::InputTextEx("##search_bar", X("Search here"), search_buffer, (int)sizeof(search_buffer),
        ImVec2(search_w, search_h), ImGuiInputTextFlags_SearchBarLook);
    ImGui::PopStyleColor(5);
    ImGui::PopStyleVar(2);

    ImGui::PopFont();

    float tabs_y = content_y + 26.0f;
    ImVec2 main_area_pos(content_x + 10.0f, tabs_y);
    ImVec2 main_area_size(content_w - 20.0f, content_h - (tabs_y - content_y) - 6.0f);

    ImGui::SetCursorScreenPos(main_area_pos);
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0, 0, 0, 0));

    if (ImGui::BeginChild("", X("main_content"), main_area_size, false, ImGuiWindowFlags_NoBackground))
    {
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));

        bool searching = search_buffer[0] != '\0';

        if (searching)
        {
            g_search_global_mode = true;
            g_search_rendered_labels.clear();

            for (int ci = 0; ci < (int)sidebar_categories.size(); ci++)
            {
                auto& cat = sidebar_categories[ci];
                for (int ti = 0; ti < (int)cat.items.size(); ti++)
                {
                    if (!cat.items[ti].render) continue;

                    ImVec2 save_pos = ImGui::GetCursorScreenPos();
                    const float label_h = 15.0f;
                    ImGui::SetCursorScreenPos(ImVec2(save_pos.x, save_pos.y + label_h));

                    ImGui::PushID(ci * 100 + ti);
                    cat.items[ti].render();
                    ImGui::PopID();

                    ImVec2 after_pos = ImGui::GetCursorScreenPos();
                    const bool had_content = (after_pos.y - save_pos.y - label_h) > 8.0f;

                    if (had_content) {
                        std::string path = cat.name + X(" / ") + cat.items[ti].name;
                        ImGui::PushFont(font.body);
                        ImGui::GetWindowDrawList()->AddText(
                            ImVec2(save_pos.x + 2.0f, save_pos.y + 1.0f),
                            ImGui::GetColorU32(ui::colors::text_disabled),
                            path.c_str());
                        ImGui::PopFont();
                        ImGui::Dummy(ImVec2(0, 4.0f));
                    } else {
                        ImGui::SetCursorScreenPos(save_pos);
                    }
                }
            }

            g_search_global_mode = false;
            g_search_current_path.clear();
        }
        else if (selected_category >= 0 && selected_category < (int)sidebar_categories.size())
        {
            auto& cat = sidebar_categories[selected_category];
            int idx = cat.active_item;
            if (idx >= 0 && idx < (int)cat.items.size() && cat.items[idx].render)
                cat.items[idx].render();
        }

        ImGui::PopStyleVar();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void ui::render_watermark()
{
    const float top_height = 50.0f;
    const float left_padding = 12.0f;
    const ImVec2 extra_padding = ImVec2(10, 6);
    const ImVec2 wm_pos = ImVec2(10, 4);

    const char* watermark_text = ImGui::GetCurrentWindow()->Name;
    ImDrawList* draw_list = ImGui::GetBackgroundDrawList();

    ImGui::PushFont(font.heading);
    ImVec2 watermark_size = ImGui::CalcTextSize(watermark_text);
    ImGui::PopFont();

    float text_height = ImGui::GetFontSize();
    float centered_y = wm_pos.y + (top_height - text_height) * 0.5f;
    ImVec2 start_pos = ImVec2(wm_pos.x + left_padding, centered_y);

    ImVec2 bg_min = ImVec2(start_pos.x - extra_padding.x, start_pos.y - extra_padding.y);
    ImVec2 bg_max = ImVec2(start_pos.x + watermark_size.x + extra_padding.x,
        start_pos.y + watermark_size.y + extra_padding.y);

    draw_list->AddRectFilled(bg_min, bg_max, ImGui::GetColorU32(ui::colors::background), 3.0f);
    draw_list->AddRect(bg_min, bg_max, ImGui::GetColorU32(ui::colors::outline), 3.0f);

    ImGui::PushFont(font.heading);
    std::string text_str = watermark_text;
    size_t dot_pos = text_str.find('.');

    if (dot_pos != std::string::npos)
    {
        std::string first_part = text_str.substr(0, dot_pos);
        std::string second_part = text_str.substr(dot_pos);
        ui::items::text_shadow(draw_list, start_pos, ImColor(250, 250, 250), first_part.c_str());
        float first_width = ImGui::CalcTextSize(first_part.c_str()).x;
        ImVec2 second_pos = ImVec2(start_pos.x + first_width, start_pos.y);
        ui::items::text_shadow(draw_list, second_pos, ImGui::GetColorU32(ui::colors::main), second_part.c_str());
    }
    else
    {
        ui::items::text_shadow(draw_list, start_pos, ImColor(250, 250, 250), watermark_text);
    }
    ImGui::PopFont();
}

void ui::render_outline()
{
    ImVec2 pos(ImGui::GetWindowPos());
    ImVec2 sz(pos.x + ui::size.x, pos.y + ui::size.y);
    ImGui::GetBackgroundDrawList()->AddRect(pos, sz, ImGui::GetColorU32(ui::colors::window_border),
        ui::rounding, ImDrawFlags_RoundCornersAll, ui::outer_border_thickness);
}

void ui::esp::bone(ImDrawList* dl, ImVec2 from, ImVec2 to, ImU32 col, float t)
{
    ImVec4 cf = ImGui::ColorConvertU32ToFloat4(col);
    float a = cf.w;
    int sh_a = (int)(72.f * a); if (sh_a > 255) sh_a = 255;
    ImU32 shadow = IM_COL32(0, 0, 0, sh_a);
    if (t > 0.4f)
        dl->AddLine(from, to, shadow, t + 1.25f);
    dl->AddLine(from, to, col, t);
}

void ui::esp::joint(ImDrawList* dl, ImVec2 pos, float r, ImU32 col)
{
    (void)dl; (void)pos; (void)r; (void)col;
}

void ui::esp::label(ImDrawList* dl, const char* text, float cx, float y, ImU32 col, bool center_x)
{
    ImGui::PushFont(font.body);
    ImVec2 sz = ImGui::CalcTextSize(text);
    float x = center_x ? (cx - sz.x * 0.5f) : cx;
    dl->AddText(ImVec2(x, y), col, text);
    ImGui::PopFont();
}

void ui::esp::health_bar(ImDrawList* dl, float x, float y_top, float y_bot, float hp, float w)
{
    dl->AddRectFilled(ImVec2(x, y_top), ImVec2(x + w, y_bot), IM_COL32(40, 40, 40, 180), 1.0f);
    float fill_top = y_bot - (y_bot - y_top) * hp;
    dl->AddRectFilled(ImVec2(x, fill_top), ImVec2(x + w, y_bot), IM_COL32(80, 220, 80, 230), 1.0f);
}

void ui::esp::box(ImDrawList* dl, ImVec2 tl, ImVec2 br, float col[4], float t)
{
    ImVec4 c(col[0], col[1], col[2], col[3]);
    dl->AddRect(tl, br, ImGui::ColorConvertFloat4ToU32(c), 0.0f, 0, t);
}

void ui::esp::box_fill(ImDrawList* dl, ImVec2 tl, ImVec2 br, float col[4])
{
    ImVec4 c(col[0], col[1], col[2], col[3]);
    dl->AddRectFilled(tl, br, ImGui::ColorConvertFloat4ToU32(c));
}

void ui::render_esp_preview(float px, float py, float pw, float ph)
{
    ImDrawList* dl = ImGui::GetBackgroundDrawList();
    auto& ep = ui::esp_preview;

    dl->AddRectFilled(ImVec2(px, py), ImVec2(px + pw, py + ph),
        ImGui::GetColorU32(ui::colors::background), ui::rounding);
    dl->AddRect(ImVec2(px, py), ImVec2(px + pw, py + ph),
        ImGui::GetColorU32(ui::colors::outline), ui::rounding);

    float cx = px + pw * 0.5f;
    float body_top = py + ph * 0.17f;
    float body_bot = py + ph * 0.84f;
    float body_h = body_bot - body_top;

    float head_r = body_h * 0.07f;
    float head_cy = body_top + head_r + body_h * 0.02f;
    float shoulder_y = head_cy + head_r + body_h * 0.065f;
    float shoulder_w = body_h * 0.145f;
    float hip_y = shoulder_y + body_h * 0.34f;
    float hip_w = body_h * 0.09f;

    float elbow_y = shoulder_y + body_h * 0.14f;
    float elbow_out = shoulder_w + body_h * 0.048f;
    float wrist_y = elbow_y + body_h * 0.125f;
    float wrist_out = elbow_out - body_h * 0.028f;

    float knee_y = hip_y + body_h * 0.21f;
    float foot_y = knee_y + body_h * 0.2f;

    ImVec2 hip_l(cx - hip_w, hip_y);
    ImVec2 hip_r(cx + hip_w, hip_y);
    ImVec2 sh_l(cx - shoulder_w, shoulder_y);
    ImVec2 sh_r(cx + shoulder_w, shoulder_y);
    ImVec2 el_l(cx - elbow_out, elbow_y);
    ImVec2 el_r(cx + elbow_out, elbow_y);
    ImVec2 wr_l(cx - wrist_out, wrist_y);
    ImVec2 wr_r(cx + wrist_out, wrist_y);
    ImVec2 knee_l(hip_l.x - body_h * 0.018f, knee_y);
    ImVec2 knee_r(hip_r.x + body_h * 0.018f, knee_y);
    ImVec2 foot_l(knee_l.x - body_h * 0.022f, foot_y);
    ImVec2 foot_r(knee_r.x + body_h * 0.022f, foot_y);

    float box_pad = 14.0f;
    float box_l = cx - elbow_out - box_pad;
    float box_r = cx + elbow_out + box_pad;
    float box_t = head_cy - head_r - 8.0f;
    float box_b = foot_y + 9.0f;

    if (ep.box_fill)
        esp::box_fill(dl, ImVec2(box_l, box_t), ImVec2(box_r, box_b), ep.fill_color);
    if (ep.box)
        esp::box(dl, ImVec2(box_l, box_t), ImVec2(box_r, box_b), ep.box_color);

    if (ep.skeleton)
    {
        ImVec4 sc(ep.skel_color[0], ep.skel_color[1], ep.skel_color[2], ep.skel_color[3] * 0.88f);
        ImU32 col = ImGui::ColorConvertFloat4ToU32(sc);
        float t = 2.05f;

        ImVec2 head_c(cx, head_cy);
        float head_bot = head_cy + head_r;
        dl->AddCircleFilled(head_c, head_r, IM_COL32(32, 33, 40, 185));
        dl->AddCircle(head_c, head_r, col, 0, t);

        ImVec2 torso_top(cx, shoulder_y);
        esp::bone(dl, ImVec2(cx, head_bot), torso_top, col, t * 0.88f);
        esp::bone(dl, torso_top, ImVec2(cx, hip_y), col, t * 0.92f);
        esp::bone(dl, sh_l, sh_r, col, t);

        esp::bone(dl, sh_l, el_l, col, t);
        esp::bone(dl, sh_r, el_r, col, t);
        esp::bone(dl, el_l, wr_l, col, t);
        esp::bone(dl, el_r, wr_r, col, t);

        esp::bone(dl, hip_l, hip_r, col, t * 0.88f);
        esp::bone(dl, hip_l, knee_l, col, t);
        esp::bone(dl, hip_r, knee_r, col, t);
        esp::bone(dl, knee_l, foot_l, col, t);
        esp::bone(dl, knee_r, foot_r, col, t);
    }

    if (ep.chams)
    {
        ImU32 vis_col = IM_COL32(60, 220, 60, 100);
        ImU32 occ_col = IM_COL32(220, 40, 40, 80);
        float tw = shoulder_w * 0.65f;
        ImVec2 torso_vis[4] = {
            ImVec2(cx - tw, shoulder_y + 2), ImVec2(cx, shoulder_y + 2),
            ImVec2(cx, hip_y - 2), ImVec2(cx - hip_w, hip_y - 2)
        };
        ImVec2 torso_occ[4] = {
            ImVec2(cx, shoulder_y + 2), ImVec2(cx + tw, shoulder_y + 2),
            ImVec2(cx + hip_w, hip_y - 2), ImVec2(cx, hip_y - 2)
        };
        dl->AddConvexPolyFilled(torso_vis, 4, vis_col);
        dl->AddConvexPolyFilled(torso_occ, 4, occ_col);
        float lw = 4.0f;
        dl->AddRectFilled(ImVec2(cx - hip_w - lw, hip_y + 1), ImVec2(cx - hip_w + lw, foot_y), vis_col, 2.0f);
        dl->AddRectFilled(ImVec2(cx + hip_w - lw, hip_y + 1), ImVec2(cx + hip_w + lw, foot_y), occ_col, 2.0f);
    }

    if (ep.health)
        esp::health_bar(dl, box_l - 12.0f, box_t, box_b, 0.72f);

    if (ep.nickname)
        esp::label(dl, X("Nickname"), cx, box_t - ImGui::CalcTextSize("N").y - 4, IM_COL32(240,240,240,230));

    if (ep.distance)
        esp::label(dl, X("100M"), box_r + 6, box_t + 2, IM_COL32(200,200,210,220), false);

    if (ep.weapon)
        esp::label(dl, X("M4A1"), cx, box_b + 4, IM_COL32(240,240,240,220));

    if (ep.quest_mark) {
        float qy = box_b + 4;
        if (ep.weapon) qy += ImGui::CalcTextSize("M").y + 2;
        esp::label(dl, X("quest"), cx, qy, IM_COL32(240,200,40,230));
    }
}
