#include <Cheat.hpp>

static void LayoutScrollColumn(CObject *owner, const Vector2 &origin, const Vector2 &area, float &scrollOffset, float &scrollLerp)
{
    float y = 0.0f;
    const float scroll = std::roundf(scrollLerp);
    Render::PushClipRect(origin, area, true);

    for (auto &c : owner->GetChildren())
    {
        if (dynamic_cast<CSubtab *>(c))
            continue;

        c->UpdateState();
        if (c->GetHidden() && c->GetAlpha() <= 0.0f)
            continue;

        const float colW = (std::max)(80.0f, area.x - Style::scrollbarGutter);
        c->SetPosition(origin + Vector2{0.0f, y - scroll});
        c->SetSize(Vector2{colW, c->GetSize().y});

        float alpha_save = Render::alpha;
        Render::alpha *= c->GetAlpha();
        c->Render();
        Render::alpha = alpha_save;

        y += c->GetSize().y + Style::sectionSpacing;
    }

    Render::PopClipRect();

    const float maxScroll = std::max(0.0f, y - area.y);
    if (maxScroll <= 0.0f)
    {
        scrollOffset = 0.0f;
    }
    else
    {
        const bool overBody = Input::IsMouseOverRect(origin, area);
        if (overBody && ImGui::GetIO().MouseWheel != 0.0f && !UI::BeingBlocked(owner))
        {
            scrollOffset = std::clamp(scrollOffset - (ImGui::GetIO().MouseWheel * 40.0f), 0.0f, maxScroll);
        }
        else
        {
            scrollOffset = std::clamp(scrollOffset, 0.0f, maxScroll);
        }
    }

    scrollLerp = std::lerp(scrollLerp, scrollOffset, 25.0f * ImGui::GetIO().DeltaTime);

    if (maxScroll > 0.0f)
    {
        const float colW = (std::max)(80.0f, area.x - Style::scrollbarGutter);
        const Vector2 trackPos = origin + Vector2{colW + 4.0f, 0.0f};
        const Vector2 trackSize = Vector2{4.0f, area.y};
        Render::AddRectFilled(trackPos, trackSize, Style::background, Style::rounding);

        const float visibleRatio = std::clamp(area.y / std::max(y, 1.0f), 0.1f, 1.0f);
        const float thumbHeight = trackSize.y * visibleRatio;
        const float thumbY = (scrollOffset / maxScroll) * (trackSize.y - thumbHeight);
        Render::AddRectFilled(trackPos + Vector2{0.0f, thumbY}, Vector2{trackSize.x, thumbHeight}, Style::accentColor, Style::rounding);
    }
}

void CTab::Render()
{
    if (!this->ShouldShow() && this->GetTabAlpha() <= 0.0f)
    {
        this->SetAnim(1.0f);
        return;
    }

    this->SetAnim(std::lerp(this->GetAnim(), 0.0f, 20.0f * ImGui::GetIO().DeltaTime));

    float alpha = Render::alpha;
    Render::alpha *= this->GetTabAlpha();

    std::vector<CSubtab *> subtabs;
    for (auto &c : GetChildren())
    {
        if (auto st = dynamic_cast<CSubtab *>(c))
        {
            if (GetSubtabHash() == 0)
                SetSubtabHash(st->GetSubtabHash());
            subtabs.push_back(st);
        }
    }

    float pillH = 0.0f;
    if (!subtabs.empty())
    {
        pillH = Style::pillHeight + 8.0f;
        const float padL = Style::contentPadding;
        const float padR = Style::contentPadRight;
        float x = GetPosition().x + padL;
        const float y = GetPosition().y + 8.0f;
        float totalW = GetSize().x - padL - padR;
        const float gap = 6.0f;
        const float pillW = (totalW - gap * (float)(subtabs.size() - 1)) / (float)subtabs.size();

        if (!UI::BeingBlocked(GetParent()) && ShouldShow())
        {
            if (Input::IsKeyPressed(VK_RIGHT) || Input::IsKeyPressed(VK_LEFT))
            {
                int idx = 0;
                for (int i = 0; i < (int)subtabs.size(); ++i)
                {
                    if (subtabs[i]->GetSubtabHash() == GetSubtabHash())
                    {
                        idx = i;
                        break;
                    }
                }
                if (Input::IsKeyPressed(VK_RIGHT))
                    idx = (idx + 1) % (int)subtabs.size();
                else
                    idx = (idx + (int)subtabs.size() - 1) % (int)subtabs.size();
                SetSubtabHash(subtabs[idx]->GetSubtabHash());
            }
        }

        for (auto &st : subtabs)
        {
            Vector2 p{x, y};
            Vector2 s{pillW, Style::pillHeight};
            const bool active = st->ShouldShow();
            const bool hovered = Input::IsMouseOverRect(p, s) && !UI::BeingBlocked(GetParent());
            if (hovered && Input::IsMouseClicked())
                SetSubtabHash(st->GetSubtabHash());

            st->SetSubtabAlpha(std::clamp(st->GetSubtabAlpha() + ((ImGui::GetIO().DeltaTime / 0.15f) * (active ? 1.0f : -1.0f)), 0.0f, 1.0f));

            Color bg = Style::widgetBackground;
            if (active)
                bg = Style::accentColor.ScaleAlpha(0.22f);
            else if (hovered)
                bg = Style::HoverFill(Style::widgetBackground);

            Render::AddRectFilled(p, s, bg, s.y * 0.5f);
            if (active)
                Render::AddRect(p, s, Style::accentColor.ScaleAlpha(0.55f), s.y * 0.5f, 1.0f);

            const Color col = active ? Style::headerText : Style::dimmedText;
            Render::AddText(st->GetName(), p + s / 2.0f, col, TEXT_FLAG_NONE, {0.5f, 0.5f}, Render::Fonts::montserrat14px);
            x += pillW + gap;
        }
    }

    const float padL = Style::contentPadding;
    const float padR = Style::contentPadRight;
    const Vector2 bodyPos = GetPosition() + Vector2{padL, pillH + (subtabs.empty() ? Style::contentPadding : 4.0f)};
    const Vector2 bodySize = GetSize() - Vector2{padL + padR, pillH + Style::contentPadding + (subtabs.empty() ? Style::contentPadding : 4.0f)};

    if (subtabs.empty())
    {
        float off = GetScrollOffset();
        float lerp = GetScrollOffsetLerp();
        LayoutScrollColumn(this, bodyPos, bodySize, off, lerp);
        SetScrollOffset(off);
        SetScrollOffsetLerp(lerp);
    }
    else
    {
        for (auto &st : subtabs)
        {
            if (st->GetSubtabAlpha() <= 0.0f)
                continue;

            float alpha_save = Render::alpha;
            Render::alpha *= st->GetSubtabAlpha();
            const float slide = (1.0f - st->GetSubtabAlpha()) * 18.0f;
            st->SetPosition(bodyPos + Vector2{slide, 0.0f});
            st->SetSize(bodySize);
            st->Render();
            Render::alpha = alpha_save;
        }
    }

    CWindow *win = UI::GetDesc<CWindow>(this);
    if (!win || !win->GetDragging())
    {
        for (auto c : GetChildren())
            UI::RenderAllTooltips(c);
    }

    Render::alpha = alpha;
}
