#include <Cheat.hpp>

void CRadio::Render()
{
    SetContainedAlpha(Render::alpha);
    if (!value || items.empty())
        return;

    if (*value < 0 || *value >= (int)items.size())
        *value = 0;

    const float rowH = Style::rowHeight;
    float width = GetSize().x;
    if (width < 40.0f)
    {
        CObject *p = GetParent<CObject>();
        width = p ? p->GetSize().x : 200.0f;
    }

    Vector2 nameSize = Render::CalcTextSize(GetName(), Render::Fonts::montserrat14px);
    const float pillsY = nameSize.y > 1.0f ? nameSize.y + 6.0f : 0.0f;
    SetSize(Vector2{width, pillsY + rowH});

    if (!GetName().empty())
    {
        Render::AddText(GetName(), GetPosition() + Vector2{0.0f, nameSize.y * 0.5f}, Style::text, TEXT_FLAG_NONE, {0.0f, 0.5f},
                        Render::Fonts::montserrat14px);
    }

    const float gap = 6.0f;
    const float pillW = (width - gap * (float)(items.size() - 1)) / (float)items.size();
    const Vector2 origin = GetPosition() + Vector2{0.0f, pillsY};

    bool hoveredRow = Input::IsMouseOverRect(origin, Vector2{width, rowH}) && !UI::BeingBlocked(GetParent()) && !GetHidden();
    if (hoveredRow)
    {
        if (Input::IsKeyPressed(VK_RIGHT))
            *value = (*value + 1) % (int)items.size();
        if (Input::IsKeyPressed(VK_LEFT))
            *value = (*value + (int)items.size() - 1) % (int)items.size();
    }

    for (int i = 0; i < (int)items.size(); ++i)
    {
        Vector2 p = origin + Vector2{(pillW + gap) * (float)i, 0.0f};
        Vector2 s{pillW, rowH};
        const bool active = *value == i;
        const bool hovered = Input::IsMouseOverRect(p, s) && !UI::BeingBlocked(GetParent()) && !GetHidden();
        if (hovered && Input::IsMouseClicked())
            *value = i;

        Color bg = active ? Style::accentColor.ScaleAlpha(0.28f) : Style::widgetBackground;
        if (hovered && !active)
            bg = Style::HoverFill(bg);
        Render::AddRectFilled(p, s, bg, s.y * 0.5f);
        if (active)
            Render::AddRect(p, s, Style::accentColor.ScaleAlpha(0.7f), s.y * 0.5f, 1.0f);
        Render::AddText(items[i], p + s / 2.0f, active ? Style::headerText : Style::dimmedText, TEXT_FLAG_NONE, {0.5f, 0.5f},
                        Render::Fonts::montserrat14px);
    }
}
