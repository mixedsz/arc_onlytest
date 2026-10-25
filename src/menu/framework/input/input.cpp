#include <Cheat.hpp>
#include "menu/menu.hpp"
#include "settings/Settings.hpp"
// memory.hpp not used on arc-um

bool Input::OnWndProc(UINT msg, WPARAM w_param, LPARAM l_param)
{
    int current_key = 0;
    e_key_state current_state = e_key_state::KEYSTATE_NONE;

    switch (msg)
    {
    case WM_KEYDOWN:
    case WM_SYSKEYDOWN:
        if (w_param < 256U)
        {
            current_key = w_param;
            current_state = e_key_state::KEYSTATE_DOWN;
        }
        break;
    case WM_KEYUP:
    case WM_SYSKEYUP:
        if (w_param < 256U)
        {
            current_key = w_param;
            current_state = e_key_state::KEYSTATE_UP;
        }
        break;
    case WM_LBUTTONDOWN:
    case WM_LBUTTONUP:
    case WM_LBUTTONDBLCLK:
        current_key = VK_LBUTTON;
        current_state = msg == WM_LBUTTONUP ? e_key_state::KEYSTATE_UP : e_key_state::KEYSTATE_DOWN;
        break;
    case WM_RBUTTONDOWN:
    case WM_RBUTTONUP:
    case WM_RBUTTONDBLCLK:
        current_key = VK_RBUTTON;
        current_state = msg == WM_RBUTTONUP ? e_key_state::KEYSTATE_UP : e_key_state::KEYSTATE_DOWN;
        break;
    case WM_MBUTTONDOWN:
    case WM_MBUTTONUP:
    case WM_MBUTTONDBLCLK:
        current_key = VK_MBUTTON;
        current_state = msg == WM_MBUTTONUP ? e_key_state::KEYSTATE_UP : e_key_state::KEYSTATE_DOWN;
        break;
    case WM_XBUTTONDOWN:
    case WM_XBUTTONUP:
    case WM_XBUTTONDBLCLK:
        current_key = (GET_XBUTTON_WPARAM(w_param) == XBUTTON1 ? VK_XBUTTON1 : VK_XBUTTON2);
        current_state = msg == WM_XBUTTONUP ? e_key_state::KEYSTATE_UP : e_key_state::KEYSTATE_DOWN;
        break;
    default:
        return false;
    }

    keyStates[current_key] = (current_state == e_key_state::KEYSTATE_DOWN)
                                 ? (keyStates[current_key] == e_key_state::KEYSTATE_NONE || keyStates[current_key] == e_key_state::KEYSTATE_UP
                                                                                     ? e_key_state::KEYSTATE_PRESSED
                                                                                     : e_key_state::KEYSTATE_DOWN)
                                                                              : e_key_state::KEYSTATE_UP;

    return true;
}

bool Input::IsMouseOverRect(const Vector2 &position, const Vector2 &size)
{
    return ImGui::IsMouseHoveringRect(position.Floored(), Vector2(position + size).Floored(), false);
}

static void AdvanceKeyState(e_key_state &state, bool down)
{
    if (down)
    {
        if (state == e_key_state::KEYSTATE_NONE || state == e_key_state::KEYSTATE_UP || state == e_key_state::KEYSTATE_RELEASED)
        {
            state = e_key_state::KEYSTATE_PRESSED;
        }
        else
        {
            state = e_key_state::KEYSTATE_DOWN;
        }
    }
    else
    {
        if (state == e_key_state::KEYSTATE_DOWN || state == e_key_state::KEYSTATE_PRESSED)
        {
            state = e_key_state::KEYSTATE_RELEASED;
        }
        else
        {
            state = e_key_state::KEYSTATE_UP;
        }
    }
}

void Input::Poll()
{
    for (int i = 0; i < 256; i++)
        AdvanceKeyState(keyStates[i], (GetAsyncKeyState(i) & 0x8000) != 0);

    Gamepad::Update();
    for (int i = GP_FIRST; i < GP_LAST; i++)
        AdvanceKeyState(keyStates[i], Gamepad::IsDown(i));

    const bool menuOpen = Menu::isOpen;
    Gamepad::DriveCursor(menuOpen);

    // Face buttons stand in for the mouse so the pad can operate the panel
    // without duplicating hit-testing in every widget. Written after the
    // keyboard sweep so it wins, and only while held, which leaves a real mouse
    // click free to drive the same states on any other frame.
    if (menuOpen && padEnabled && padCursorEnabled && Gamepad::Connected())
    {
        static bool s_prevLeft = false;
        static bool s_prevRight = false;

        const bool left = Gamepad::IsDown(GP_A);
        const bool right = Gamepad::IsDown(GP_B);

        if (left)
            keyStates[VK_LBUTTON] = s_prevLeft ? e_key_state::KEYSTATE_DOWN : e_key_state::KEYSTATE_PRESSED;
        if (right)
            keyStates[VK_RBUTTON] = s_prevRight ? e_key_state::KEYSTATE_DOWN : e_key_state::KEYSTATE_PRESSED;

        s_prevLeft = left;
        s_prevRight = right;
    }
}

void Input::PostMessageQuick(const std::int32_t &virtualKey)
{
    PostMessage(GetForegroundWindow(), WM_KEYDOWN, virtualKey, 0);
    PostMessage(GetForegroundWindow(), WM_KEYUP, virtualKey, 0);
}

void Input::SendInputCommand(int inputKey, bool down, bool up)
{
    INPUT Input = {0};
    Input.type = INPUT_KEYBOARD;
    Input.ki.wVk = static_cast<WORD>(inputKey);

    if (down)
    {
        Input.ki.dwFlags = 0;
        SendInput(1, &Input, sizeof(Input));
    }

    if (up)
    {
        Input.ki.dwFlags = KEYEVENTF_KEYUP;
        SendInput(1, &Input, sizeof(Input));
    }
}