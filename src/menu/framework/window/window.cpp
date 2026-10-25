#include <Cheat.hpp>
#include "settings/Settings.hpp"

void CWindow::Render()
{
    Style::Update();

    this->SetWindowAlpha(
        std::clamp(this->GetWindowAlpha() + (15.0f * ImGui::GetIO().DeltaTime * (this->GetOpened() ? 1.0f : -1.0f)), 0.0f, 1.0f));

    Render::alpha = this->GetWindowAlpha();
    if (this->GetWindowAlpha() <= 0.0f)
    {
        Render::alpha = 1.0f;
        return;
    }

    const bool wasShadows = Render::enableShadows;
    if (GetDragging() || GetResizing())
        Render::enableShadows = false;

    Vector2 size = GetSize();
    size.x = std::max(size.x, Style::minWindowWidth);
    size.y = std::max(size.y, Style::minWindowHeight);
    SetSize(size);

    const float sidebarW = GetSidebarWidth();
    const bool iconsOnly = sidebarW <= Style::sidebarCollapsedWidth + 0.5f;
    const Vector2 pos = GetPosition();

    Render::AddShadowRect(pos, GetSize(), Style::shadow, 25.0f, Style::rounding);
    Render::AddRectFilled(pos, GetSize(), Style::background, Style::rounding);

    Render::AddRectFilled(pos, Vector2{sidebarW, GetSize().y}, Style::sidebar, Style::rounding, ImDrawFlags_RoundCornersLeft);
    Render::AddRectFilled(pos + Vector2{sidebarW, 0.0f}, Vector2{1.0f, GetSize().y}, Style::outline);

    const Vector2 contentHeaderPos = pos + Vector2{sidebarW, 0.0f};
    const Vector2 contentHeaderSize = Vector2{GetSize().x - sidebarW, Style::headerHeight};
    Render::AddRectFilled(contentHeaderPos, contentHeaderSize, Style::header, Style::rounding, ImDrawFlags_RoundCornersTopRight);
    Render::AddRectFilled(contentHeaderPos + Vector2{0.0f, Style::headerHeight - 1.0f}, Vector2{contentHeaderSize.x, 1.0f}, Style::outline);

    Render::AddText(GetName(), contentHeaderPos + Vector2{Style::contentPadding, Style::headerHeight / 2.0f}, Style::headerText,
                    TEXT_FLAG_NONE, {0.0f, 0.5f}, Render::Fonts::montserratBold16px);

    if (!GetTld().empty())
    {
        const float titleW = Render::CalcTextSize(GetName(), Render::Fonts::montserratBold16px).x;
        Render::AddText(GetTld(),
                        contentHeaderPos + Vector2{Style::contentPadding + titleW + 10.0f, Style::headerHeight / 2.0f},
                        Style::statusMuted, TEXT_FLAG_NONE, {0.0f, 0.5f}, Render::Fonts::montserrat14px);
    }

    std::vector<CTab *> mainTabs;
    std::vector<CTab *> bottomTabs;
    for (auto c : GetChildren())
    {
        CTab *t = dynamic_cast<CTab *>(c);
        if (!t)
            continue;
        if (this->GetTabHash() == 0)
            this->SetTabHash(t->GetTabHash());
        if (t->GetPinBottom())
            bottomTabs.push_back(t);
        else
            mainTabs.push_back(t);
    }

    auto DrawNavItem = [&](CTab *t, const Vector2 &itemPos, const Vector2 &itemSize) {
        const bool active = this->GetTabHash() == t->GetTabHash();
        const bool hovered = Input::IsMouseOverRect(itemPos, itemSize) && !this->HasBlockingObject() && !GetDragging() && !GetResizing();
        t->SetHoverAlpha(std::clamp(t->GetHoverAlpha() + (12.0f * ImGui::GetIO().DeltaTime * (hovered && !active ? 1.0f : -1.0f)), 0.0f, 1.0f));
        t->SetTabAlpha(std::clamp(t->GetTabAlpha() + ((ImGui::GetIO().DeltaTime / 0.15f) * (active ? 1.0f : -1.0f)), 0.0f, 1.0f));

        if (hovered && Input::IsMouseClicked())
            this->SetTabHash(t->GetTabHash());

        Color bg = Style::sidebar;
        if (active)
            bg = Style::HoverFill(Style::sidebar);
        else if (t->GetHoverAlpha() > 0.0f)
            bg = Style::sidebar.Lerp(Style::HoverFill(Style::sidebar), t->GetHoverAlpha());

        Render::AddRectFilled(itemPos, itemSize, bg, 4.0f);
        if (active)
            Render::AddRectFilled(itemPos, Vector2{3.0f, itemSize.y}, Style::accentColor, 2.0f);

        const Color labelCol = Style::dimmedText.Lerp(Style::headerText, active ? 1.0f : t->GetHoverAlpha());
        ImFont *iconFont = Render::Fonts::fontAwesome14px ? Render::Fonts::fontAwesome14px : Render::Fonts::montserrat16px;
        const float iconX = itemPos.x + (iconsOnly ? itemSize.x * 0.5f : 18.0f);
        if (!t->GetIcon().empty())
        {
            Render::AddText(t->GetIcon(), Vector2{iconX, itemPos.y + itemSize.y * 0.5f}, labelCol, TEXT_FLAG_NONE,
                            {iconsOnly ? 0.5f : 0.5f, 0.5f}, iconFont);
        }
        if (!iconsOnly)
        {
            Render::AddText(t->GetName(), itemPos + Vector2{40.0f, itemSize.y * 0.5f}, labelCol, TEXT_FLAG_NONE, {0.0f, 0.5f},
                            Render::Fonts::montserrat16px);
        }
    };

    const float brandH = 64.0f;
    if (!iconsOnly)
    {
        Render::AddText(X("SYNDIKATE"), pos + Vector2{Style::contentPadding, 22.0f}, Style::headerText, TEXT_FLAG_NONE, {0.0f, 0.5f},
                        Render::Fonts::montserratBold16px);
        const float brandW = Render::CalcTextSize(X("SYNDIKATE"), Render::Fonts::montserratBold16px).x;
        Render::AddText(X("ARC"), pos + Vector2{Style::contentPadding + brandW + 6.0f, 22.0f}, Style::accentColor, TEXT_FLAG_NONE,
                        {0.0f, 0.5f}, Render::Fonts::montserratBold16px);
    }
    else if (Render::Fonts::fontAwesome14px)
    {
        Render::AddText(ICON_FA_BOLT, pos + Vector2{sidebarW * 0.5f, 28.0f}, Style::accentColor, TEXT_FLAG_NONE, {0.5f, 0.5f},
                        Render::Fonts::fontAwesome14px);
    }

    float navY = pos.y + brandH;
    for (CTab *t : mainTabs)
    {
        DrawNavItem(t, Vector2{pos.x + 8.0f, navY}, Vector2{sidebarW - 16.0f, Style::navItemHeight});
        navY += Style::navItemHeight + 4.0f;
    }

    if (!bottomTabs.empty())
    {
        float bottomY = pos.y + GetSize().y - 12.0f - (float)bottomTabs.size() * (Style::navItemHeight + 4.0f);
        Render::AddRectFilled(Vector2{pos.x + 16.0f, bottomY - 10.0f}, Vector2{sidebarW - 32.0f, 1.0f}, Style::outline);
        for (CTab *t : bottomTabs)
        {
            DrawNavItem(t, Vector2{pos.x + 8.0f, bottomY}, Vector2{sidebarW - 16.0f, Style::navItemHeight});
            bottomY += Style::navItemHeight + 4.0f;
        }
    }

    const Vector2 contentPos = pos + Vector2{sidebarW, Style::headerHeight};
    const Vector2 contentSize = GetSize() - Vector2{sidebarW, Style::headerHeight};

    for (auto c : GetChildren())
    {
        CTab *t = dynamic_cast<CTab *>(c);
        if (!t)
            continue;
        t->SetSize(contentSize);
        t->SetPosition(contentPos);
        t->Render();
    }

    if (!HasBlockingObject() && Input::IsKeyPressed(VK_TAB) &&
        Input::IsMouseOverRect(pos, Vector2{sidebarW, GetSize().y}))
    {
        std::vector<CTab *> ordered = mainTabs;
        ordered.insert(ordered.end(), bottomTabs.begin(), bottomTabs.end());
        if (!ordered.empty())
        {
            int idx = 0;
            for (int i = 0; i < (int)ordered.size(); ++i)
            {
                if (ordered[i]->GetTabHash() == GetTabHash())
                {
                    idx = i;
                    break;
                }
            }
            const bool back = ImGui::GetIO().KeyShift;
            idx = (idx + (back ? (int)ordered.size() - 1 : 1)) % (int)ordered.size();
            SetTabHash(ordered[idx]->GetTabHash());
        }
    }

    if (!HasBlockingObject())
    {
        std::vector<CTab *> ordered = mainTabs;
        ordered.insert(ordered.end(), bottomTabs.begin(), bottomTabs.end());
        if (!ordered.empty())
        {
            int idx = 0;
            for (int i = 0; i < (int)ordered.size(); ++i)
            {
                if (ordered[i]->GetTabHash() == GetTabHash())
                {
                    idx = i;
                    break;
                }
            }
            if (Input::IsKeyPressed(VK_DOWN) && Input::IsMouseOverRect(pos, Vector2{sidebarW, GetSize().y}))
            {
                idx = (idx + 1) % (int)ordered.size();
                SetTabHash(ordered[idx]->GetTabHash());
            }
            if (Input::IsKeyPressed(VK_UP) && Input::IsMouseOverRect(pos, Vector2{sidebarW, GetSize().y}))
            {
                idx = (idx + (int)ordered.size() - 1) % (int)ordered.size();
                SetTabHash(ordered[idx]->GetTabHash());
            }
        }
    }

    Render::AddRect(pos, GetSize(), Style::outline, Style::rounding);

    const Vector2 gripPos = pos + GetSize() - Vector2{16.0f, 16.0f};
    const Vector2 gripSize{16.0f, 16.0f};
    const bool gripHovered = Input::IsMouseOverRect(gripPos, gripSize) && !HasBlockingObject();
    Render::AddLine(gripPos + Vector2{4.0f, 14.0f}, gripPos + Vector2{14.0f, 4.0f}, Style::dimmedText.ScaleAlpha(gripHovered ? 1.0f : 0.45f), 1.5f);
    Render::AddLine(gripPos + Vector2{8.0f, 14.0f}, gripPos + Vector2{14.0f, 8.0f}, Style::dimmedText.ScaleAlpha(gripHovered ? 1.0f : 0.45f), 1.5f);

    if (gripHovered && Input::IsMouseClicked())
        SetResizing(true);

    if (GetResizing())
    {
        if (!Input::IsKeyDown(VK_LBUTTON))
            SetResizing(false);
        else
        {
            Vector2 desired = ImGui::GetIO().MousePos - pos;
            desired.x = std::clamp(desired.x, Style::minWindowWidth, ImGui::GetIO().DisplaySize.x - pos.x - 8.0f);
            desired.y = std::clamp(desired.y, Style::minWindowHeight, ImGui::GetIO().DisplaySize.y - pos.y - 8.0f);
            SetSize({std::roundf(desired.x), std::roundf(desired.y)});
        }
    }

    const bool overHeader = Input::IsMouseOverRect(contentHeaderPos, contentHeaderSize);
    const bool overBrand = Input::IsMouseOverRect(pos, Vector2{sidebarW, brandH});
    // IsMouseClicked() consumes the press, so the cheap state guards run first -
    // otherwise a header click with a popup open was eaten here and never reached the
    // popup's own dismiss handler.
    if ((overHeader || overBrand) && !this->HasBlockingObject() && !gripHovered && Input::IsMouseClicked())
    {
        SetDragging(true);
        SetDragOffset(ImGui::GetIO().MousePos - GetPosition());
    }

    if (GetDragging())
    {
        if (!Input::IsKeyDown(VK_LBUTTON))
            SetDragging(false);

        Vector2 desired = ImGui::GetIO().MousePos - GetDragOffset();
        const float max_x = std::max(0.0f, ImGui::GetIO().DisplaySize.x - GetSize().x - Style::padding);
        const float max_y = std::max(0.0f, ImGui::GetIO().DisplaySize.y - GetSize().y - Style::padding);
        const float cx = std::clamp(desired.x, Style::padding, max_x);
        const float cy = std::clamp(desired.y, Style::padding, max_y);
        SetPosition({std::roundf(cx), std::roundf(cy)});
    }

    Render::enableShadows = wasShadows;
    Render::alpha = 1.0f;
}
