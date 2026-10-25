#pragma once

// Pseudo virtual-key codes for gamepad controls.
//
// Real VK codes stop at 0xFE, so starting the gamepad block at 0x100 can never
// collide with a keyboard or mouse code. That lets every pad control ride the
// same Input::keyStates table as the keyboard, which in turn means the keybind
// widget, IsKeyPressed/IsKeyDown, and every existing bind site keep working
// with no per-call-site knowledge of the pad.
enum : int
{
    GP_FIRST = 0x100,

    GP_A = GP_FIRST, // Cross
    GP_B,            // Circle
    GP_X,            // Square
    GP_Y,            // Triangle

    GP_LB, // L1
    GP_RB, // R1
    GP_LT, // L2
    GP_RT, // R2

    GP_BACK,  // Share
    GP_START, // Options
    GP_L3,
    GP_R3,

    GP_DPAD_UP,
    GP_DPAD_DOWN,
    GP_DPAD_LEFT,
    GP_DPAD_RIGHT,

    GP_LS_UP,
    GP_LS_DOWN,
    GP_LS_LEFT,
    GP_LS_RIGHT,

    GP_RS_UP,
    GP_RS_DOWN,
    GP_RS_LEFT,
    GP_RS_RIGHT,

    GP_LAST,
    GP_COUNT = GP_LAST - GP_FIRST,
};

// Size of Input::keyStates: the 256 VK codes plus the gamepad block.
inline constexpr int kInputStateCount = GP_LAST;

namespace Gamepad
{
    enum Layout : int
    {
        LayoutAuto = 0,
        LayoutXbox,
        LayoutPlayStation,
    };

    enum MenuCombo : int
    {
        ComboL3R3 = 0,
        ComboBackStart,
        ComboNone,
    };

    // Polled once per frame from Input::Poll.
    void Update();

    bool Connected();
    int UserIndex();

    // LayoutXbox or LayoutPlayStation - never LayoutAuto.
    int ResolvedLayout();

    // Vendor string for the menu status line, e.g. "Xbox Controller".
    const char *DeviceName();

    bool IsDown(int pseudoVk);

    constexpr bool IsGamepadKey(int vk)
    {
        return vk >= GP_FIRST && vk < GP_LAST;
    }

    // Face-button label under the active layout, e.g. "R2" vs "RT".
    const char *KeyName(int pseudoVk);

    // Edge-triggered: true on the frame the configured open combo completes.
    bool MenuComboPressed();

    // Steers ImGui's mouse position and wheel from the sticks while the menu is
    // up. Called at the tail of Input::Poll, after the keyboard sweep.
    void DriveCursor(bool menuOpen);
} // namespace Gamepad
