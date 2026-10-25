#include <Cheat.hpp>

void CGrid::Render()
{
    SetContainedAlpha(Render::alpha);

    float width = GetSize().x;
    if (width < 40.0f)
    {
        CObject *p = GetParent<CObject>();
        width = p ? p->GetSize().x : 200.0f;
    }

    const float colW = (width - gap * (float)(columns - 1)) / (float)columns;
    float y = 0.0f;
    float rowH = 0.0f;
    int i = 0;

    for (auto c : GetChildren())
    {
        c->UpdateState();
        if (c->GetHidden() && c->GetAlpha() <= 0.0f)
            continue;

        const int col = i % columns;
        if (col == 0 && i > 0)
        {
            y += rowH + gap;
            rowH = 0.0f;
        }

        c->SetPosition(GetPosition() + Vector2{(colW + gap) * (float)col, y});
        c->SetSize(Vector2{colW, c->GetSize().y});
        c->Render();
        rowH = std::max(rowH, c->GetSize().y);
        ++i;
    }

    SetSize(Vector2{width, y + rowH});
}
