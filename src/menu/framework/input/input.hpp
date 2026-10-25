#pragma once

#include "gamepad.hpp"

enum class e_key_state : int
{
    KEYSTATE_NONE = 0,
    KEYSTATE_DOWN,
    KEYSTATE_UP,
    KEYSTATE_PRESSED,
    KEYSTATE_RELEASED
};

namespace Input
{
    bool OnWndProc(UINT msg, WPARAM w_param, LPARAM l_param);
    void Poll();

    bool IsMouseOverRect(const Vector2 &position, const Vector2 &size);

    void PostMessageQuick(const std::int32_t &virtualKey);

    void SendInputCommand(int inputKey, bool down = true, bool up = true);

    // Indices 0..255 are virtual-key codes; GP_FIRST..GP_LAST-1 are gamepad
    // controls, so every consumer below works on either device unmodified.
    inline std::array<e_key_state, static_cast<std::size_t>(kInputStateCount)> keyStates = {};

    inline bool IsKeyDown(const std::uint32_t button_code)
    {
        return keyStates[button_code] == e_key_state::KEYSTATE_DOWN;
    }

    inline bool IsKeyReleased(const std::uint32_t button_code)
    {
        if (keyStates[button_code] == e_key_state::KEYSTATE_RELEASED)
        {
            keyStates[button_code] = e_key_state::KEYSTATE_UP;
            return true;
        }

        return false;
    }

    inline bool IsKeyPressed(const std::uint32_t button_code)
    {
        if (keyStates[button_code] == e_key_state::KEYSTATE_PRESSED)
        {
            keyStates[button_code] = e_key_state::KEYSTATE_DOWN;
            return true;
        }
        return false;
    }

    inline bool IsMouseClicked(bool right_click = false)
    {
        return IsKeyPressed(right_click ? VK_RBUTTON : VK_LBUTTON);
    }

    // Held-state for a bound key from either device. Feature code must use this
    // rather than GetAsyncKeyState, which cannot see the gamepad pseudo-codes.
    inline bool IsBindDown(int key)
    {
        if (key <= 0)
            return false;
        if (Gamepad::IsGamepadKey(key))
            return Gamepad::IsDown(key);
        if (key >= 256)
            return false;
        return (GetAsyncKeyState(key) & 0x8000) != 0;
    }

    // Per-bind latch for EvalBind. Toggle mode needs somewhere to remember the
    // previous frame's edge and the current toggle state.
    struct BindState
    {
        bool prevDown = false;
        bool toggled = false;
    };

    // mode: 0 = disabled, 1 = hold, 2 = toggled, 3 = always.
    // Must be called exactly once per frame per bind, or a held key registers as
    // several toggle edges. Consumers that run inside loops or conditional draw
    // blocks should read a cached result instead of calling this directly.
    inline bool EvalBind(int key, int mode, BindState &state)
    {
        switch (mode)
        {
        case 1:
            return IsBindDown(key);
        case 2:
        {
            const bool down = IsBindDown(key);
            if (down && !state.prevDown)
                state.toggled = !state.toggled;
            state.prevDown = down;
            return state.toggled;
        }
        case 3:
            return true;
        default:
            return false;
        }
    }
} // namespace Input
