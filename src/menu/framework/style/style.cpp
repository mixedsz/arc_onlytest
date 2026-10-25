#include <Cheat.hpp>

void Style::Update() {
    if (!overrideAccent)
        accentColor = Color(168, 85, 247);

    if (!overrideStyle)
    {
        shadow = Color(0, 0, 5);
        background = Color(26, 26, 30);
        sidebar = Color(20, 20, 24);
        outline = Color(42, 42, 48);
        header = Color(20, 20, 24);
        container = Color(26, 26, 30);
        containerHeader = Color(26, 26, 30);
        headerText = Color(228, 228, 232);
        text = Color(228, 228, 232);
        dimmedText = Color(139, 139, 149);
        sectionHeader = Color(139, 139, 149);
        widgetBackground = Color(32, 32, 36);
        widgetBackgroundHovered = Color(40, 40, 45);
        warning = Color(234, 179, 8);
        success = Color(34, 197, 94);
        danger = Color(239, 68, 68);
        toggleOn = accentColor;
        toggleOff = Color(63, 63, 70);
        statusMuted = Color(accentColor.r, accentColor.g, accentColor.b, 180);
        hoverOverlay = Color(255, 255, 255, 13);
    }

    if (!GImGui)
        return;

    checkmark = Color::White();
}
