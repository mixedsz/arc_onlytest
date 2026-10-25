#include <Cheat.hpp>
#include <cmath>

void CSliderFloat::Render()
{
    SetContainedAlpha(Render::alpha);

    float rect_height = 6.0f;
    Vector2 text_size = Render::CalcTextSize(GetName());
    Vector2 size = Vector2{GetSize().x, text_size.y + (Style::padding * 0.5f) + rect_height};
    SetSize(size);

    bool rowHovered = Input::IsMouseOverRect(GetPosition(), size) && !UI::BeingBlocked(GetParent()) && !GetHidden() && !GetActive();
    if (rowHovered)
        Render::AddRectFilled(GetPosition(), size, Style::hoverOverlay, 4.0f);

    Render::AddText(GetName(), GetPosition(), Style::dimmedText.Lerp(Style::text, GetActiveAlpha()), TEXT_FLAG_NONE, {0.0f, 0.0f},
                    Render::Fonts::montserrat14px);

    const float hovered_relative = (ImGui::GetIO().MousePos.x - GetPosition().x) / size.x;
    float hovered_value = GetMin() + std::clamp(hovered_relative, 0.0f, 1.0f) * (GetMax() - GetMin());

    std::string value_fmt = std::vformat(GetFormat().empty() ? X("{:.1f}") : GetFormat().data(), std::make_format_args(*GetValue()));
    std::string hovered_value_fmt = std::vformat(GetFormat().empty() ? X("{:.1f}") : GetFormat().data(), std::make_format_args(hovered_value));
    Vector2 value_fmt_size = Render::CalcTextSize(value_fmt);
    Vector2 hovered_fmt_size = Render::CalcTextSize(hovered_value_fmt);

    const float inset = Style::controlInset;
    Render::AddText(hovered_value_fmt, GetPosition() + Vector2{size.x - inset - ((value_fmt_size.x + (Style::padding * 0.5f)) * GetHoverAlpha()), 0.0f},
                     Style::text.ScaleAlpha(GetHoverAlpha()), TEXT_FLAG_NONE, {1.0f, 0.0f});

    Render::AddText(value_fmt, GetPosition() + Vector2{size.x - inset, 0.0f}, Style::dimmedText.Lerp(Style::text, GetActiveAlpha()), TEXT_FLAG_NONE,
                     {1.0f, 0.0f}, Render::Fonts::montserrat14px);

    size.y = rect_height;
    const float barW = (std::max)(40.0f, GetSize().x - Style::controlInset);
    SetPosition({GetPosition().x, GetPosition().y + text_size.y + (Style::padding * 0.5f)});
    Render::AddShadowRect(GetPosition(), Vector2{barW, size.y}, Style::shadow.ScaleAlpha(0.5f + (0.5f * GetHoverAlpha())), 15.0f, 3.0f);
    Render::AddRectFilled(GetPosition(), Vector2{barW, size.y}, Style::widgetBackground, 3.0f);
    Render::AddRect(GetPosition(), Vector2{barW, size.y}, Style::outline, 3.0f);

    bool hovered = Input::IsMouseOverRect(GetPosition(), Vector2{barW, size.y}) && !UI::BeingBlocked(GetParent()) && !GetHidden() && !GetActive();
    if (hovered && Input::IsMouseClicked())
    {
        UI::SetBlockingObject(this, GetParent());
        SetActive(true);
    }

    if (GetActive())
    {
        SetTooltipTime(0.0f);

        if (!Input::IsKeyDown(VK_LBUTTON))
        {
            SetActive(false);
            UI::SetBlockingObject(nullptr, GetParent());
        }

        const float relative = (ImGui::GetIO().MousePos.x - GetPosition().x) / barW;
        *GetValue() = GetMin() + std::clamp(relative, 0.0f, 1.0f) * (GetMax() - GetMin());
    }

    float slider_fraction_static = std::clamp((*GetValue() - GetMin()) / (GetMax() - GetMin()), 0.0f, 1.0f);
    SetLerpedValue(std::lerp(GetLerpedValue(), slider_fraction_static, 25.0f * ImGui::GetIO().DeltaTime));
    float slider_fraction = GetLerpedValue();

    bool zero_centered = GetMin() < 0.0f && GetMax() > 0.0f;

    if (zero_centered)
    {
        float center_fraction = -GetMin() / (GetMax() - GetMin());
        float zero_fraction = slider_fraction - center_fraction;
        float center_px = GetPosition().x + (barW * center_fraction);

        float bar_width_px = std::abs(barW * zero_fraction);
        float pos_x = (zero_fraction >= 0.0f) ? center_px : (center_px - bar_width_px);

        Render::AddRectFilled({pos_x, GetPosition().y}, Vector2{bar_width_px, size.y}, Style::accentColor, Style::rounding);
    }
    else
    {
        Render::AddRectFilled(GetPosition(), Vector2{barW * slider_fraction, size.y}, Style::accentColor, Style::rounding);
    }

    SetHoverAlpha(std::clamp(GetHoverAlpha() + (10.0f * ImGui::GetIO().DeltaTime * (hovered ? 1.0f : -1.0f)), 0.0f, 1.0f));
    SetActiveAlpha(std::clamp(GetActiveAlpha() + (10.0f * ImGui::GetIO().DeltaTime * (GetActive() ? 1.0f : -1.0f)), 0.0f, 1.0f));
}

void CSliderInt::Render()
{
    SetContainedAlpha(Render::alpha);

    float rect_height = 6.0f;
    Vector2 text_size = Render::CalcTextSize(GetName());
    Vector2 widget_size = Vector2{GetSize().x, text_size.y + (Style::padding * 0.5f) + rect_height};
    SetSize(widget_size);

    const Vector2 origin = GetPosition();
    const float inset = Style::controlInset;
    const Vector2 bar_pos = origin + Vector2{0.0f, text_size.y + (Style::padding * 0.5f)};
    const Vector2 bar_size = Vector2{(std::max)(40.0f, widget_size.x - inset), rect_height};
    const int range = (std::max)(1, GetMax() - GetMin());

    auto SnapInt = [&](float relative) -> int {
        float t = std::clamp(relative, 0.0f, 1.0f);
        int v = GetMin() + (int)std::lround(t * (float)range);
        return std::clamp(v, GetMin(), GetMax());
    };

    const float hover_relative = (ImGui::GetIO().MousePos.x - origin.x) / bar_size.x;
    int hovered_value = SnapInt(hover_relative);

    const std::string fmt = GetFormat().empty() ? std::string(X("{}")) : GetFormat();
    std::string value_fmt = std::vformat(fmt, std::make_format_args(*GetValue()));
    std::string hovered_value_fmt = std::vformat(fmt, std::make_format_args(hovered_value));
    Vector2 value_fmt_size = Render::CalcTextSize(value_fmt);

    Render::AddText(GetName(), origin, Style::dimmedText.Lerp(Style::text, GetActiveAlpha()), TEXT_FLAG_NONE, {0.0f, 0.0f});

    Render::AddText(hovered_value_fmt, origin + Vector2{widget_size.x - inset - ((value_fmt_size.x + (Style::padding * 0.5f)) * GetHoverAlpha()), 0.0f},
                     Style::text.ScaleAlpha(GetHoverAlpha()), TEXT_FLAG_NONE, {1.0f, 0.0f});

    Render::AddText(value_fmt, origin + Vector2{widget_size.x - inset, 0.0f}, Style::dimmedText.Lerp(Style::text, GetActiveAlpha()), TEXT_FLAG_NONE,
                     {1.0f, 0.0f});

    Render::AddShadowRect(bar_pos, bar_size, Style::shadow.ScaleAlpha(0.5f + (0.5f * GetHoverAlpha())), 15.0f, 3.0f);
    Render::AddRectFilled(bar_pos, bar_size, Style::widgetBackground, 3.0f);
    Render::AddRect(bar_pos, bar_size, Style::outline, 3.0f);

    // Full widget hitbox (label + bar) — 6px bar alone is nearly unusable.
    bool hovered = Input::IsMouseOverRect(origin, widget_size) && !UI::BeingBlocked(GetParent()) && !GetHidden() && !GetActive();
    if (hovered && Input::IsMouseClicked())
    {
        UI::SetBlockingObject(this, GetParent());
        SetActive(true);
    }

    if (GetActive())
    {
        SetTooltipTime(0.0f);

        if (!Input::IsKeyDown(VK_LBUTTON))
        {
            SetActive(false);
            UI::SetBlockingObject(nullptr, GetParent());
        }

        const float relative = (ImGui::GetIO().MousePos.x - origin.x) / bar_size.x;
        *GetValue() = SnapInt(relative);
    }

    float slider_fraction_static = std::clamp((static_cast<float>(*GetValue()) - static_cast<float>(GetMin())) / static_cast<float>(range), 0.0f, 1.0f);
    SetLerpedValue(std::lerp(GetLerpedValue(), slider_fraction_static, 25.0f * ImGui::GetIO().DeltaTime));
    float slider_fraction = GetLerpedValue();

    bool zero_centered = GetMin() < 0 && GetMax() > 0;

    if (zero_centered)
    {
        float center_fraction = -static_cast<float>(GetMin()) / static_cast<float>(range);
        float zero_fraction = slider_fraction - center_fraction;
        float center_px = bar_pos.x + (bar_size.x * center_fraction);

        float bar_width_px = std::abs(bar_size.x * zero_fraction);
        float pos_x = (zero_fraction >= 0.0f) ? center_px : (center_px - bar_width_px);

        Render::AddRectFilled({pos_x, bar_pos.y}, Vector2{bar_width_px, bar_size.y}, Style::accentColor, Style::rounding);
    }
    else
    {
        Render::AddRectFilled(bar_pos, Vector2{bar_size.x * slider_fraction, bar_size.y}, Style::accentColor, Style::rounding);
    }

    SetHoverAlpha(std::clamp(GetHoverAlpha() + (10.0f * ImGui::GetIO().DeltaTime * (hovered || GetActive() ? 1.0f : -1.0f)), 0.0f, 1.0f));
    SetActiveAlpha(std::clamp(GetActiveAlpha() + (10.0f * ImGui::GetIO().DeltaTime * (GetActive() ? 1.0f : -1.0f)), 0.0f, 1.0f));
}
