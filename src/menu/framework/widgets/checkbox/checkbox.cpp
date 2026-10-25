#include <Cheat.hpp>

void CCheckbox::Render()
{
    SetContainedAlpha(Render::alpha);

    if (!GetValue())
        return;

    const float rowH = Style::rowHeight;
    float width = GetSize().x;
    if (width < 40.0f)
    {
        CObject *p = GetParent<CObject>();
        width = p ? p->GetSize().x : 200.0f;
    }
    SetSize(Vector2{width, rowH});

    const Vector2 trackSize{36.0f, 18.0f};
    const float inset = Style::controlInset;
    const Vector2 trackPos = GetPosition() + Vector2{GetSize().x - trackSize.x - inset, (rowH - trackSize.y) * 0.5f};
    const bool onAccessories = ImGui::GetIO().MousePos.x > GetPosition().x + GetSize().x - (88.0f + inset) &&
                               ImGui::GetIO().MousePos.x < trackPos.x;

    bool hovered = Input::IsMouseOverRect(GetPosition(), GetSize()) && !UI::BeingBlocked(GetParent()) && !GetHidden();
    if ((hovered && !onAccessories && Input::IsMouseClicked()) || (hovered && Input::IsKeyPressed(VK_RETURN)))
    {
        *GetValue() = !*GetValue();
        SetTooltipTime(0.0f);
    }

    SetHoverAlpha(std::clamp(GetHoverAlpha() + (12.0f * ImGui::GetIO().DeltaTime * (hovered ? 1.0f : -1.0f)), 0.0f, 1.0f));
    SetActiveAlpha(std::clamp(GetActiveAlpha() + (14.0f * ImGui::GetIO().DeltaTime * (*GetValue() ? 1.0f : -1.0f)), 0.0f, 1.0f));

    if (GetHoverAlpha() > 0.0f)
        Render::AddRectFilled(GetPosition(), GetSize(), Style::hoverOverlay.ScaleAlpha(GetHoverAlpha()), 4.0f);

    float labelX = 0.0f;
    if (GetHasBadge())
    {
        Render::AddCircleFilled(GetPosition() + Vector2{6.0f, rowH * 0.5f}, 4.0f, GetBadge().OverrideAlpha(Render::alpha));
        labelX = 16.0f;
    }

    Render::AddText(GetName(), GetPosition() + Vector2{labelX, rowH * 0.5f},
                    Style::dimmedText.Lerp(Style::text, 0.65f + 0.35f * GetActiveAlpha()), TEXT_FLAG_NONE, {0.0f, 0.5f},
                    Render::Fonts::montserrat14px);

    const Vector2 extras = trackPos;

    Vector2 accessory = extras - Vector2{22.0f, 0.0f};
    accessory.y = GetPosition().y + (rowH - 16.0f) * 0.5f;
    Vector2 accSize{16.0f, 16.0f};

    for (auto c : GetChildren())
    {
        CColorPicker *cp = dynamic_cast<CColorPicker *>(c);
        if (cp)
        {
            accessory.x -= 4.0f;
            Render::AddRectFilled(accessory, accSize, cp->GetValue()->OverrideAlpha(Render::alpha), 4.0f);
            Render::AddRect(accessory, accSize, Style::outline, 4.0f);
            bool cp_hovered = Input::IsMouseOverRect(accessory, accSize) && !UI::BeingBlocked(GetParent()) && !GetHidden();
            if (cp_hovered && Input::IsMouseClicked())
            {
                cp->SetPosition(ImGui::GetIO().MousePos);
                cp->SetOpened(true);
                UI::SetBlockingObject(cp, GetParent());
            }
            if (cp_hovered)
            {
                if (ImGui::GetIO().KeyCtrl && (Input::IsKeyDown('C') || ImGui::IsKeyPressed(ImGuiKey_C)))
                    Style::copiedColor = *cp->GetValue();
                if (ImGui::GetIO().KeyCtrl && (Input::IsKeyDown('V') || ImGui::IsKeyPressed(ImGuiKey_V)))
                    *cp->GetValue() = Style::copiedColor;
            }
            float alpha_save = Render::alpha;
            Render::alpha *= cp->GetOpenAlpha();
            cp->Render();
            Render::alpha = alpha_save;
            accessory.x -= accSize.x + 6.0f;
        }

        CPopup *pu = dynamic_cast<CPopup *>(c);
        if (pu)
        {
            if (Render::Fonts::fontAwesome14px)
            {
                Render::AddText(ICON_FA_GEAR, accessory + accSize / 2.0f,
                                Style::dimmedText.Lerp(Style::text, pu->GetOpenAlpha()),
                                TEXT_FLAG_NONE, {0.5f, 0.5f}, Render::Fonts::fontAwesome14px);
            }
            bool gear_hovered = Input::IsMouseOverRect(accessory, accSize) && !UI::BeingBlocked(GetParent()) && !GetHidden();
            if (gear_hovered && Input::IsMouseClicked())
            {
                pu->SetPosition(ImGui::GetIO().MousePos);
                pu->SetOpened(true);
                UI::SetBlockingObject(pu, GetParent());
            }
            float alpha_save = Render::alpha;
            Render::alpha *= pu->GetOpenAlpha();
            pu->Render();
            Render::alpha = alpha_save;
            accessory.x -= accSize.x + 6.0f;
        }
    }

    Color track = Style::toggleOff.Lerp(Style::toggleOn, GetActiveAlpha());
    Render::AddRectFilled(trackPos, trackSize, track.OverrideAlpha(Render::alpha), trackSize.y * 0.5f);

    const float knobR = 7.0f;
    const float knobX = trackPos.x + 9.0f + (trackSize.x - 18.0f) * GetActiveAlpha();
    Render::AddCircleFilled(Vector2{knobX, trackPos.y + trackSize.y * 0.5f}, knobR, Color::White().OverrideAlpha(Render::alpha));
}
