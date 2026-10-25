#include <Cheat.hpp>

void CSeperator::Render()
{
    SetContainedAlpha(Render::alpha);

    Vector2 size = Vector2{GetParent() ? GetParent()->GetSize().x : GetSize().x, GetName().empty() ? 8.0f : 18.0f};
    SetSize(size);

    if (!GetName().empty())
    {
        const std::string header = UI::ToUpper(GetName());
        Render::AddText(header, GetPosition() + Vector2{0.0f, size.y * 0.5f}, Style::sectionHeader, TEXT_FLAG_NONE, {0.0f, 0.5f},
                        Render::Fonts::montserrat14px);
        return;
    }

    Render::AddRectFilled(GetPosition() + Vector2{0.0f, size.y * 0.5f}, Vector2{size.x, 1.0f}, Style::outline);
}
