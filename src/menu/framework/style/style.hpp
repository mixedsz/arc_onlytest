#pragma once

namespace Style
{
    void Update();

    inline bool overrideAccent = false;
    inline bool overrideStyle = false;

    inline Color copiedColor = Color::White();

    inline Color accentColor = Color(168, 85, 247);
    inline Color checkmark = Color(255, 255, 255);
    inline Color shadow = Color(0, 0, 0);
    inline Color background = Color(26, 26, 30);
    inline Color sidebar = Color(20, 20, 24);
    inline Color outline = Color(42, 42, 48);
    inline Color header = Color(20, 20, 24);
    inline Color container = Color(26, 26, 30);
    inline Color containerHeader = Color(26, 26, 30);
    inline Color headerText = Color(228, 228, 232);
    inline Color dimmedText = Color(139, 139, 149);
    inline Color sectionHeader = Color(139, 139, 149);
    inline Color text = Color(228, 228, 232);
    inline Color widgetBackground = Color(32, 32, 36);
    inline Color widgetBackgroundHovered = Color(40, 40, 45);
    inline Color warning = Color(234, 179, 8);
    inline Color success = Color(34, 197, 94);
    inline Color danger = Color(239, 68, 68);
    inline Color toggleOn = Color(168, 85, 247);
    inline Color toggleOff = Color(63, 63, 70);
    inline Color statusMuted = Color(168, 85, 247, 180);
    inline Color hoverOverlay = Color(255, 255, 255, 13);

    inline float rounding = 6.0f;
    inline float padding = 10.0f;
    inline float contentPadding = 20.0f;
    inline float contentPadRight = 24.0f;
    inline float scrollbarGutter = 16.0f;
    inline float controlInset = 10.0f;
    inline float sectionSpacing = 16.0f;

    inline float headerHeight = 48.0f;
    inline float containerHeaderHeight = 28.0f;
    inline float rowHeight = 32.0f;
    inline float pillHeight = 34.0f;

    inline float sidebarWidth = 200.0f;
    inline float sidebarCollapsedWidth = 56.0f;
    inline float sidebarCollapseAt = 800.0f;
    inline float navItemHeight = 40.0f;

    inline float minWindowWidth = 750.0f;
    inline float minWindowHeight = 500.0f;

    inline float ContentRightPad()
    {
        return contentPadRight;
    }

    inline Color HoverFill(const Color &base)
    {
        return base.Lerp(Color::White(), 0.05f);
    }
}
