#include <Cheat.hpp>

void CSubtab::Render()
{
    if (!this->ShouldShow() && this->GetSubtabAlpha() <= 0.0f)
        return;

    const Vector2 contentOrigin = GetPosition();
    const float availableH = GetSize().y;
    const Vector2 area{GetSize().x, availableH};

    float alpha = Render::alpha;
    Render::alpha *= this->GetSubtabAlpha();

    float y = 0.0f;
    const float scroll = std::roundf(this->GetScrollOffsetLerp());

    Render::PushClipRect(contentOrigin, area, true);

    for (auto &c : GetChildren())
    {
        if (dynamic_cast<CSubtab *>(c))
            continue;

        c->UpdateState();
        if (c->GetHidden() && c->GetAlpha() <= 0.0f)
            continue;

        const float colW = (std::max)(80.0f, area.x - Style::scrollbarGutter);
        c->SetPosition(contentOrigin + Vector2{0.0f, y - scroll});
        c->SetSize(Vector2{colW, c->GetSize().y});

        float alpha_save = Render::alpha;
        Render::alpha *= c->GetAlpha();
        c->Render();
        Render::alpha = alpha_save;

        y += c->GetSize().y + Style::sectionSpacing;
    }

    Render::PopClipRect();

    const float maxScroll = std::max(0.0f, y - availableH);
    if (maxScroll <= 0.0f)
    {
        this->SetScrollOffset(0.0f);
    }
    else
    {
        const bool overBody = Input::IsMouseOverRect(contentOrigin, area);
        if (overBody && ImGui::GetIO().MouseWheel != 0.0f && !UI::BeingBlocked(GetParent()))
        {
            this->SetScrollOffset(std::clamp(this->GetScrollOffset() - (ImGui::GetIO().MouseWheel * 40.0f), 0.0f, maxScroll));
        }
        else
        {
            this->SetScrollOffset(std::clamp(this->GetScrollOffset(), 0.0f, maxScroll));
        }
    }

    this->SetScrollOffsetLerp(std::lerp(this->GetScrollOffsetLerp(), this->GetScrollOffset(), 25.0f * ImGui::GetIO().DeltaTime));

    if (maxScroll > 0.0f)
    {
        const float colW = (std::max)(80.0f, area.x - Style::scrollbarGutter);
        const Vector2 trackPos = GetPosition() + Vector2{colW + 4.0f, 0.0f};
        const Vector2 trackSize = Vector2{4.0f, availableH};
        Render::AddRectFilled(trackPos, trackSize, Style::background, Style::rounding);
        const float visibleRatio = std::clamp(availableH / std::max(y, 1.0f), 0.1f, 1.0f);
        const float thumbHeight = trackSize.y * visibleRatio;
        const float thumbY = (this->GetScrollOffset() / maxScroll) * (trackSize.y - thumbHeight);
        Render::AddRectFilled(trackPos + Vector2{0.0f, thumbY}, Vector2{trackSize.x, thumbHeight}, Style::accentColor, Style::rounding);
    }

    Render::alpha = alpha;
}
