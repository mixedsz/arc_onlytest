#pragma once

class CContainer : public CObject
{
public:
    CContainer(std::string name, bool forceContentHeight = false, float contentHeightFraction = 0.0f)
    {
        SetName(name);
        SetScrollState(false);
        SetScrollOffset(0.0f);
        SetScrollOffsetLerp(0.0f);
        SetForcedContentHeight(forceContentHeight);
        SetForcedHeightFraction(contentHeightFraction);
        persistId = name;
        collapsed = false;
        collapsible = true;
        expandAnim = 1.0f;
        hoverAlpha = 0.0f;
        loadedPersist = false;
    }

    void Render() override;

    bool GetScrollState() const
    {
        return scrollState;
    }

    void SetScrollState(bool scrollState)
    {
        this->scrollState = scrollState;
    }

    float GetScrollOffset() const
    {
        return scrollOffset;
    }

    void SetScrollOffset(float scrollOffset)
    {
        this->scrollOffset = scrollOffset;
    }

    float GetScrollOffsetLerp() const
    {
        return scrollOffsetLerp;
    }

    void SetScrollOffsetLerp(float scrollOffsetLerp)
    {
        this->scrollOffsetLerp = scrollOffsetLerp;
    }

    bool GetForcedContentHeight() const
    {
        return forcedContentHeight;
    }

    void SetForcedContentHeight(bool forcedContentHeight)
    {
        this->forcedContentHeight = forcedContentHeight;
    }

    float GetForcedHeightFraction() const
    {
        return forcedHeightFraction;
    }

    void SetForcedHeightFraction(float forcedHeightFraction)
    {
        this->forcedHeightFraction = forcedHeightFraction;
    }

    bool GetCollapsible() const { return collapsible; }
    void SetCollapsible(bool v) { collapsible = v; }

    bool GetCollapsed() const { return collapsed; }
    void SetCollapsed(bool v) { collapsed = v; }

    const std::string &GetPersistId() const { return persistId; }
    void SetPersistId(std::string id) { persistId = std::move(id); }

    float GetExpandAnim() const { return expandAnim; }
    void SetExpandAnim(float v) { expandAnim = v; }

    float GetHoverAlpha() const { return hoverAlpha; }
    void SetHoverAlpha(float v) { hoverAlpha = v; }

    bool GetLoadedPersist() const { return loadedPersist; }
    void SetLoadedPersist(bool v) { loadedPersist = v; }

private:
    bool forcedContentHeight;
    float forcedHeightFraction;

    float scrollOffset;
    float scrollOffsetLerp;
    bool scrollState;

    bool collapsible;
    bool collapsed;
    bool loadedPersist;
    std::string persistId;
    float expandAnim;
    float hoverAlpha;
};
