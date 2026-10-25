#pragma once

#include "imgui.h"
#include "imgui_internal.h"
#include "settings/Settings.hpp"
#include "menu/string_encryption.hpp"
#include "gui_colors.h"
#include "IconsLucide.h"

#include <string>
#include <vector>
#include <set>
#include <unordered_map>
#include <cctype>
#include <algorithm>
#include <functional>
#include <cmath>
#include <mutex>

struct GuiFonts {
    ImFont* heading = nullptr;
    ImFont* body = nullptr;
    ImFont* widget_icon[3] = {};
    ImFont* tab_icon = nullptr;
    ImFont* inter_semibold[2] = {};
    ImFont* spacegrotesk_medium[2] = {};
}; inline GuiFonts font;

struct SidebarSubItem
{
    std::string                 name;
    std::string                 icon;
    std::function<void()>       render;
};

struct SidebarCategory
{
    std::string                 name;
    std::string                 icon;
    std::vector<SidebarSubItem> items;
    int                         active_item = 0;
    float                       hover_anim = 0.0f;
    std::vector<float>          item_anim;
};

inline std::vector<SidebarCategory> sidebar_categories;
inline int                          selected_category = 0;
inline int                          selected_sidebar_item = 0;
inline char                         search_buffer[128] = "";
inline bool                         g_search_global_mode = false;
inline std::string                  g_search_current_path;
inline std::set<std::string>        g_search_rendered_labels;
inline int                          sidebar_icon_index = 0;
inline std::vector<float>           sidebar_icon_anim;

struct MinimizeData
{
    bool  target_minimized = false;
    float animated_height = 0.0f;
};
static std::unordered_map<ImGuiID, MinimizeData> minimize_data;

struct BrandInfo {
    std::string brand_name;
    std::string secondary_text;
    ImVec4 color;
    bool loaded = false;
};
inline BrandInfo g_brandInfo;

namespace ui
{
    inline ImVec2 size = ImVec2(830, 494);
    const float rounding            = 6.f;
    const float outer_border_thickness = 1.0f;
    const float topbar_icon_height  = 38.f;
    const float sidebar_items_width = 120.f;
    const float sidebar_width       = sidebar_items_width;
    const float sidebar_icon_width  = 0.f;
    const float topbar_height       = 0.f;
    const float content_padding     = 12.f;

    void initialize_fonts();
    void initialize_sidebar();

    void render_background();
    void render_sidebar(float dt);
    void render_content();
    void render_outline();
    void render_watermark();
    void Render();

    std::string get_breadcrumb();

    SidebarCategory& add_category(const char* name, const char* icon);
    void add_tab(SidebarCategory& cat, const char* name, const char* icon, std::function<void()> fn);

    struct EspPreviewState {
        bool  box       = false;
        bool  box_fill  = true;
        bool  skeleton  = true;
        bool  nickname  = true;
        bool  distance  = true;
        bool  weapon    = true;
        bool  health    = true;
        bool  quest_mark= true;
        bool  chams     = false;
        float box_color [4] = {0.43f, 0.29f, 0.78f, 1.0f};
        float fill_color[4] = {0.43f, 0.29f, 0.78f, 0.3f};
        float skel_color[4] = {1,1,1,1};
    };
    inline EspPreviewState esp_preview;

    void render_esp_preview(float px, float py, float pw, float ph);

    namespace esp
    {
        void bone(ImDrawList* dl, ImVec2 from, ImVec2 to, ImU32 col, float t = 1.4f);
        void joint(ImDrawList* dl, ImVec2 pos, float r, ImU32 col);
        void label(ImDrawList* dl, const char* text, float cx, float y, ImU32 col, bool center_x = true);
        void health_bar(ImDrawList* dl, float x, float y_top, float y_bot, float hp, float w = 3.f);
        void box(ImDrawList* dl, ImVec2 tl, ImVec2 br, float col[4], float t = 1.5f);
        void box_fill(ImDrawList* dl, ImVec2 tl, ImVec2 br, float col[4]);
    };

    namespace items
    {
        void text_shadow(ImDrawList* draw_list, ImVec2 pos, ImColor text_color, const char* text);
    };
};

inline bool acceptedAimbotRisk = false;
inline bool pendingAimbotAccept = false;

inline void StyledSliderFloat(const char* label, const char* id, float* v, float v_min, float v_max, const char* icon = nullptr, const char* fmt = "%.0f")
{
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", label, id);
    ImGui::SliderFloat(combined, v, v_min, v_max, fmt);
}

inline void StyledSliderInt(const char* label, const char* id, int* v, int v_min, int v_max, const char* icon = nullptr)
{
    char combined[256];
    snprintf(combined, sizeof(combined), "%s%s", label, id);
    ImGui::SliderInt(combined, v, v_min, v_max, "%d");
}

enum class NotifType { Info, Success, Error };

struct Notification
{
    std::string message;
    NotifType   type = NotifType::Info;
    float       lifetime = 0.f;
    float       timeLeft = 0.f;
    float       animProgress = 0.0f;
    bool        persistent = false;
    int         id = 0;
};

inline std::vector<Notification> g_notifications;
inline std::mutex                g_notifMutex;
inline int                       g_notifNextId = 1;
inline int                       g_notifVisibleCount = 0;

inline int PushNotification(const std::string& msg, NotifType type = NotifType::Info, float duration = 3.0f, bool persistent = false)
{
    std::lock_guard<std::mutex> lk(g_notifMutex);
    Notification n{};
    n.message = msg;
    n.type = type;
    n.lifetime = duration;
    n.timeLeft = duration;
    n.persistent = persistent;
    n.id = g_notifNextId++;
    g_notifications.emplace_back(n);
    return n.id;
}

inline void DismissNotification(int id)
{
    std::lock_guard<std::mutex> lk(g_notifMutex);
    for (auto& n : g_notifications)
        if (n.id == id) { n.persistent = false; n.timeLeft = 0.3f; }
}

inline void RenderNotifications()
{
    auto draw = ImGui::GetForegroundDrawList();
    if (!draw) return;

    std::lock_guard<std::mutex> lk(g_notifMutex);

    float screenW = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    const float barW = 320.f, barH = 38.f, pad = 14.f, gap = 6.f;
    const float accentW = 3.f, iconSpace = 28.f, rounding = 5.f;
    constexpr float dt = 1.f / 60.f;
    constexpr int maxVisible = 3;
    int drawn = 0;

    for (int i = 0; i < (int)g_notifications.size();)
    {
        auto& n = g_notifications[i];
        if (!n.persistent) n.timeLeft -= dt;

        if (n.timeLeft > 0.3f || n.persistent)
            n.animProgress = (std::min)(1.0f, n.animProgress + dt * 6.0f);
        else
            n.animProgress = (std::max)(0.0f, n.animProgress - dt * 4.0f);

        if (n.animProgress <= 0.0f && n.timeLeft <= 0.0f) {
            g_notifications.erase(g_notifications.begin() + i);
            continue;
        }

        if (drawn < maxVisible) {
            float a = n.animProgress;
            float slideX = screenW - barW * a - pad;
            float posY = pad + (barH + gap) * drawn;

            ImVec2 p1(slideX, posY), p2(slideX + barW, posY + barH);
            int bgA = (int)(235 * a);

            draw->AddRectFilled(p1, p2, IM_COL32(18, 19, 24, bgA), rounding);
            draw->AddRect(p1, p2, IM_COL32(255, 255, 255, (int)(12 * a)), rounding);

            ImU32 accentCol;
            const char* icon = ICON_LC_INFO;
            switch (n.type) {
            case NotifType::Success:
                accentCol = IM_COL32(60, 200, 120, (int)(255 * a));
                icon = ICON_LC_CHECK;
                break;
            case NotifType::Error:
                accentCol = IM_COL32(220, 60, 60, (int)(255 * a));
                icon = ICON_LC_X;
                break;
            default: {
                auto& m = ui::colors::main;
                accentCol = IM_COL32((int)(m.x * 255), (int)(m.y * 255), (int)(m.z * 255), (int)(255 * a));
                icon = ICON_LC_WIFI;
                break;
            }
            }

            draw->AddRectFilled(ImVec2(p1.x, p1.y + 4), ImVec2(p1.x + accentW, p2.y - 4), accentCol, 2.f);

            ImFont* iconFnt = font.widget_icon[0];
            if (iconFnt) {
                float iy = posY + (barH - iconFnt->FontSize) * 0.5f;
                draw->AddText(iconFnt, iconFnt->FontSize, ImVec2(slideX + 10, iy), accentCol, icon);
            }

            float ty = posY + (barH - 13.f) * 0.5f;
            draw->AddText(nullptr, 13.f, ImVec2(slideX + iconSpace + 6, ty),
                IM_COL32(228, 228, 236, (int)(255 * a)), n.message.c_str());

            drawn++;
        }
        i++;
    }

    g_notifVisibleCount = drawn;
}

inline void RenderNotificationsButton()
{
    float screenW = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    float screenH = static_cast<float>(GetSystemMetrics(SM_CYSCREEN));

    for (int i = 0; i < (int)g_notifications.size(); i++)
    {
        auto& n = g_notifications[i];
        if (n.message.find(X("AI Detection Notice")) == std::string::npos) continue;

        constexpr float dt = 0.016f;
        if (!n.persistent) n.timeLeft -= dt;
        if (n.timeLeft > 0.3f) n.animProgress = (std::min)(1.0f, n.animProgress + dt * 6.0f);
        else n.animProgress = (std::max)(0.0f, n.animProgress - dt * 4.0f);
        float alpha = n.animProgress;

        const ImVec2 winSize(420, 170);
        ImGui::SetNextWindowPos(ImVec2(screenW * 0.5f, screenH * 0.5f), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
        ImGui::SetNextWindowSize(winSize);
        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 10.0f);
        ImGui::PushStyleVar(ImGuiStyleVar_Alpha, alpha);
        ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(18.f/255.f, 20.f/255.f, 24.f/255.f, (235.f/255.f)*alpha));
        ImGui::PushStyleColor(ImGuiCol_Border, ui::colors::main);

        if (ImGui::Begin("##AimbotWarning", nullptr,
            ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoCollapse |
            ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar))
        {
            auto* dl = ImGui::GetWindowDrawList();
            ImVec2 pMin = ImGui::GetWindowPos();
            ImVec2 pMax = ImVec2(pMin.x + winSize.x, pMin.y + winSize.y);
            dl->AddRect(pMin, pMax, ImGui::GetColorU32(ui::colors::main), 10.0f, 0, 1.5f);

            const char* title = X("Disclaimer");
            ImGui::PushStyleColor(ImGuiCol_Text, ui::colors::main);
            ImGui::SetCursorPosY(10.0f);
            ImGui::SetCursorPosX((winSize.x - ImGui::CalcTextSize(title).x) * 0.5f);
            ImGui::Text("%s", title);
            ImGui::PopStyleColor();

            const char* body = X("This game uses AI-based aimbot detection. Use the aimbot at your own risk.");
            ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1,1,1,alpha));
            ImGui::SetCursorPosX(18.0f); ImGui::SetCursorPosY(36.0f);
            ImGui::PushTextWrapPos(ImGui::GetCursorPosX() + (winSize.x - 36.0f));
            ImGui::TextWrapped("%s", body);
            ImGui::PopTextWrapPos(); ImGui::PopStyleColor();

            ImGui::Dummy(ImVec2(0, 10));
            ImVec2 btnSize(120, 32);
            float startX = (winSize.x - btnSize.x * 2 - 10) * 0.5f;
            ImGui::SetCursorPosX(startX);

            ImVec4 btnCol(ui::colors::main.x*0.35f, ui::colors::main.y*0.35f, ui::colors::main.z*0.35f, alpha);
            ImVec4 btnHov(ui::colors::main.x*0.55f, ui::colors::main.y*0.55f, ui::colors::main.z*0.55f, alpha);
            ImVec4 btnAct(ui::colors::main.x*0.25f, ui::colors::main.y*0.25f, ui::colors::main.z*0.25f, alpha);
            ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
            ImGui::PushStyleColor(ImGuiCol_ButtonHovered, btnHov);
            ImGui::PushStyleColor(ImGuiCol_ButtonActive, btnAct);

            if (ImGui::Button(X("Accept"), btnSize)) { acceptedAimbotRisk = true; AimCfg.enable = true; pendingAimbotAccept = false; n.timeLeft = 0.f; }
            ImGui::SameLine(0, 10);
            if (ImGui::Button(X("Deny"), btnSize)) { acceptedAimbotRisk = false; AimCfg.enable = false; pendingAimbotAccept = false; n.timeLeft = 0.f; }
            ImGui::PopStyleColor(3);
        }
        ImGui::End();
        ImGui::PopStyleColor(2);
        ImGui::PopStyleVar(2);
    }
}

inline void RenderViewportLoadingNotification()
{
    auto draw = ImGui::GetForegroundDrawList();
    if (!draw) return;

    float screenW = static_cast<float>(GetSystemMetrics(SM_CXSCREEN));
    const float barW = 340.f, barH = 40.f, pad = 14.f, gap = 6.f, rnd = 5.f, accentW = 3.f;

    static float loadAnim = 0.0f;
    loadAnim = (std::min)(1.0f, loadAnim + (1.f / 60.f) * 5.0f);
    float a = loadAnim;

    float x = screenW - barW * a - pad;
    float y = pad + (barH + gap) * g_notifVisibleCount;
    ImVec2 p1(x, y), p2(x + barW, y + barH);

    draw->AddRectFilled(p1, p2, IM_COL32(18, 19, 24, (int)(235 * a)), rnd);
    draw->AddRect(p1, p2, IM_COL32(255, 255, 255, (int)(12 * a)), rnd);

    auto& m = ui::colors::main;
    ImU32 accent = IM_COL32((int)(m.x*255), (int)(m.y*255), (int)(m.z*255), (int)(255*a));
    draw->AddRectFilled(ImVec2(p1.x, p1.y + 4), ImVec2(p1.x + accentW, p2.y - 4), accent, 2.f);

    ImFont* iconFnt = font.widget_icon[0];
    if (iconFnt) {
        float iy = y + (barH - iconFnt->FontSize) * 0.5f;
        draw->AddText(iconFnt, iconFnt->FontSize, ImVec2(x + 10, iy), accent, ICON_LC_LOADER);
    }

    float ty = y + (barH - 13.f) * 0.5f;
    draw->AddText(nullptr, 13.f, ImVec2(x + 36, ty),
        IM_COL32(228, 228, 236, (int)(255 * a)), X("Resolving decryptions and pointers..."));
}
