#include <Cheat.hpp>

void CButton::Render() {
    SetContainedAlpha(Render::alpha);

    Vector2 text_size = Render::CalcTextSize(UI::WrapText(GetName(), std::max(40.0f, GetSize().x - 16.0f)), Render::Fonts::montserrat14px);
    float w = GetSize().x;
    CGrid *grid = dynamic_cast<CGrid *>(GetParent<CObject>());
    if (!grid)
    {
        CObject *p = GetParent<CObject>();
        w = p ? p->GetSize().x : w;
    }
    Vector2 size = Vector2{w, std::max(32.0f, text_size.y + 12.0f)};
    SetSize(size);

    bool hovered = Input::IsMouseOverRect(GetPosition(), GetSize()) && !UI::BeingBlocked(GetParent()) && !GetHidden();
    Color bg = hovered ? Style::HoverFill(Style::widgetBackground) : Style::widgetBackground;
    Render::AddRectFilled(GetPosition(), GetSize(), bg, Style::rounding);
    Render::AddRect(GetPosition(), GetSize(), Style::outline, Style::rounding);
    Render::AddRectFilled(GetPosition(), GetSize(), Style::accentColor.ScaleAlpha(GetClickedAlpha()), Style::rounding);

    Render::AddText(UI::WrapText(GetName(), GetSize().x - 16.0f), GetPosition() + GetSize() / 2.0f, Style::text, TEXT_FLAG_NONE, {0.5f, 0.5f},
                    Render::Fonts::montserrat14px);

    if (hovered && Input::IsMouseClicked())
    {
        TriggerCallback();
        SetTooltipTime(0.0f);
        SetClickedAlpha(1.0f);
    }

    SetHoverAlpha(std::clamp(GetHoverAlpha() + (10.0f * ImGui::GetIO().DeltaTime * (hovered ? 1.0f : -1.0f)), 0.0f, 1.0f));
    SetClickedAlpha(std::clamp(GetClickedAlpha() + (5.0f * ImGui::GetIO().DeltaTime * -1.0f), 0.0f, 1.0f));
}
