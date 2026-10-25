#include <Cheat.hpp>
#include "settings/Settings.hpp"

void CContainer::Render()
{
    if (!GetLoadedPersist())
    {
        SetCollapsed(ui_section_collapsed(GetPersistId().empty() ? GetName() : GetPersistId()));
        SetLoadedPersist(true);
        SetExpandAnim(GetCollapsed() ? 0.0f : 1.0f);
    }

    const float target = GetCollapsed() ? 0.0f : 1.0f;
    SetExpandAnim(std::lerp(GetExpandAnim(), target, 18.0f * ImGui::GetIO().DeltaTime));
    if (std::abs(GetExpandAnim() - target) < 0.01f)
        SetExpandAnim(target);

    Vector2 headerPos = GetPosition();
    Vector2 headerSize = Vector2{GetSize().x, Style::containerHeaderHeight};

    const bool headerHovered = Input::IsMouseOverRect(headerPos, headerSize) && !UI::BeingBlocked(GetParent()) && !GetHidden();
    SetHoverAlpha(std::clamp(GetHoverAlpha() + (12.0f * ImGui::GetIO().DeltaTime * (headerHovered ? 1.0f : -1.0f)), 0.0f, 1.0f));

    if (GetCollapsible() && headerHovered && Input::IsMouseClicked())
    {
        const bool next = !GetCollapsed();
        SetCollapsed(next);
        ui_set_section_collapsed(GetPersistId().empty() ? GetName() : GetPersistId(), next);
    }

    if (GetHoverAlpha() > 0.0f)
        Render::AddRectFilled(headerPos, headerSize, Style::hoverOverlay.ScaleAlpha(GetHoverAlpha()), 4.0f);

    const std::string header = UI::ToUpper(GetName());
    Render::AddText(header, headerPos + Vector2{0.0f, headerSize.y * 0.5f}, Style::sectionHeader, TEXT_FLAG_NONE, {0.0f, 0.5f},
                    Render::Fonts::montserrat14px);

    if (GetCollapsible() && Render::Fonts::fontAwesome14px)
    {
        const char *chev = GetCollapsed() ? ICON_FA_ANGLE_RIGHT : ICON_FA_ANGLE_DOWN;
        Render::AddText(chev, headerPos + Vector2{headerSize.x - Style::controlInset, headerSize.y * 0.5f}, Style::dimmedText, TEXT_FLAG_NONE,
                        {1.0f, 0.5f}, Render::Fonts::fontAwesome14px);
    }

    Render::AddRectFilled(headerPos + Vector2{0.0f, headerSize.y - 1.0f}, Vector2{headerSize.x, 1.0f}, Style::outline);

    float offset = Style::containerHeaderHeight + 8.0f;
    const float anim = GetExpandAnim();

    if (anim > 0.01f)
    {
        Render::PushClipRect(GetPosition() + Vector2{0.0f, Style::containerHeaderHeight},
                             Vector2{GetSize().x, 4000.0f}, true);

        for (auto c : GetChildren())
        {
            c->UpdateState();
            if (c->GetHidden() && c->GetAlpha() <= 0.0f)
                continue;

            c->SetPosition(GetPosition() + Vector2(0.0f, offset - this->GetScrollOffset()));
            c->SetSize(Vector2{GetSize().x, c->GetSize().y});

            float alpha_save = Render::alpha;
            Render::alpha *= c->GetAlpha() * anim;
            c->Render();
            Render::alpha = alpha_save;

            offset += std::roundf((c->GetSize().y + 4.0f) * c->GetAlpha());
        }

        Render::PopClipRect();
    }

    const float contentH = (offset - Style::containerHeaderHeight) * anim;
    const float height = Style::containerHeaderHeight + contentH + (anim > 0.05f ? 6.0f : 2.0f);
    SetSize(Vector2{GetSize().x, height});

    this->SetScrollOffset(0.0f);
    this->SetScrollOffsetLerp(0.0f);
    this->SetScrollState(false);
}
