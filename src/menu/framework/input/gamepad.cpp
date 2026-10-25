#include <Cheat.hpp>

#include "app/AppWindow.hpp"
#include "settings/Settings.hpp"

namespace
{
    // Minimal XInput ABI, declared locally so the translation unit has no
    // dependency on Xinput.h and no import library is added to the link line.
    struct XI_GAMEPAD
    {
        WORD wButtons;
        BYTE bLeftTrigger;
        BYTE bRightTrigger;
        SHORT sThumbLX;
        SHORT sThumbLY;
        SHORT sThumbRX;
        SHORT sThumbRY;
    };

    struct XI_STATE
    {
        DWORD dwPacketNumber;
        XI_GAMEPAD Gamepad;
    };

    struct XI_VIBRATION
    {
        WORD wLeftMotorSpeed;
        WORD wRightMotorSpeed;
    };

    struct XI_CAPABILITIES
    {
        BYTE Type;
        BYTE SubType;
        WORD Flags;
        XI_GAMEPAD Gamepad;
        XI_VIBRATION Vibration;
    };

    struct XI_CAPABILITIES_EX
    {
        XI_CAPABILITIES Capabilities;
        WORD VendorId;
        WORD ProductId;
        WORD VersionNumber;
        DWORD unk1;
    };

    constexpr WORD XI_DPAD_UP = 0x0001;
    constexpr WORD XI_DPAD_DOWN = 0x0002;
    constexpr WORD XI_DPAD_LEFT = 0x0004;
    constexpr WORD XI_DPAD_RIGHT = 0x0008;
    constexpr WORD XI_START = 0x0010;
    constexpr WORD XI_BACK = 0x0020;
    constexpr WORD XI_LTHUMB = 0x0040;
    constexpr WORD XI_RTHUMB = 0x0080;
    constexpr WORD XI_LSHOULDER = 0x0100;
    constexpr WORD XI_RSHOULDER = 0x0200;
    constexpr WORD XI_A = 0x1000;
    constexpr WORD XI_B = 0x2000;
    constexpr WORD XI_X = 0x4000;
    constexpr WORD XI_Y = 0x8000;

    constexpr DWORD XI_ERROR_SUCCESS = 0;

    constexpr WORD kVendorSony = 0x054C;
    constexpr WORD kVendorMicrosoft = 0x045E;

    using PFN_XInputGetState = DWORD(WINAPI *)(DWORD, XI_STATE *);
    using PFN_XInputGetCapabilitiesEx = DWORD(WINAPI *)(DWORD, DWORD, DWORD, XI_CAPABILITIES_EX *);

    HMODULE g_xinput = nullptr;
    PFN_XInputGetState g_getState = nullptr;
    PFN_XInputGetCapabilitiesEx g_getCapsEx = nullptr;
    bool g_loadAttempted = false;

    int g_userIndex = -1;
    bool g_connected = false;
    int g_detectedLayout = Gamepad::LayoutXbox;
    const char *g_deviceName = "None";

    bool g_buttons[GP_COUNT] = {};

    float g_lx = 0.0f, g_ly = 0.0f;
    float g_rx = 0.0f, g_ry = 0.0f;
    float g_lt = 0.0f, g_rt = 0.0f;

    bool g_comboPrev = false;

    // Virtual cursor state.
    bool g_cursorOwned = false;
    float g_cursorX = 0.0f, g_cursorY = 0.0f;
    ImVec2 g_lastWritten = ImVec2(-1.0f, -1.0f);

    void LoadXInput()
    {
        if (g_loadAttempted)
            return;
        g_loadAttempted = true;

        // 1_4 ships with Windows 8 and later; the older names are only reachable
        // if a redistributable put them on the machine.
        static const wchar_t *kCandidates[] = {
            L"XInput1_4.dll",
            L"xinput1_3.dll",
            L"xinput9_1_0.dll",
        };

        for (const wchar_t *name : kCandidates)
        {
            g_xinput = LoadLibraryW(name);
            if (g_xinput)
                break;
        }

        if (!g_xinput)
            return;

        g_getState = reinterpret_cast<PFN_XInputGetState>(GetProcAddress(g_xinput, "XInputGetState"));

        // Undocumented, exported by ordinal only. Gives the real VID/PID so the
        // Auto layout can tell a DualSense from an Xbox pad. Absent on the older
        // DLLs, in which case Auto falls back to Xbox labels.
        g_getCapsEx = reinterpret_cast<PFN_XInputGetCapabilitiesEx>(GetProcAddress(g_xinput, reinterpret_cast<LPCSTR>(108)));
    }

    void ResolveDevice(int index)
    {
        g_detectedLayout = Gamepad::LayoutXbox;
        g_deviceName = "Xbox Controller";

        if (!g_getCapsEx)
        {
            g_deviceName = "XInput Controller";
            return;
        }

        XI_CAPABILITIES_EX caps{};
        if (g_getCapsEx(1, static_cast<DWORD>(index), 0, &caps) != XI_ERROR_SUCCESS)
        {
            g_deviceName = "XInput Controller";
            return;
        }

        if (caps.VendorId == kVendorSony)
        {
            g_detectedLayout = Gamepad::LayoutPlayStation;
            g_deviceName = "PlayStation Controller";
        }
        else if (caps.VendorId == kVendorMicrosoft)
        {
            g_detectedLayout = Gamepad::LayoutXbox;
            g_deviceName = "Xbox Controller";
        }
        else
        {
            g_deviceName = "XInput Controller";
        }
    }

    // Radial deadzone with rescaling, so the first usable unit of deflection
    // starts at zero instead of jumping to the deadzone magnitude.
    void ApplyStickDeadzone(float rawX, float rawY, float deadzone, float &outX, float &outY)
    {
        const float mag = std::sqrt(rawX * rawX + rawY * rawY);
        if (mag <= deadzone || mag <= 0.0f)
        {
            outX = 0.0f;
            outY = 0.0f;
            return;
        }

        float scaled = (mag - deadzone) / (1.0f - deadzone);
        if (scaled > 1.0f)
            scaled = 1.0f;

        outX = (rawX / mag) * scaled;
        outY = (rawY / mag) * scaled;
    }

    float NormalizeThumb(SHORT v)
    {
        // -32768 has no positive counterpart; clamping keeps the range symmetric.
        float f = static_cast<float>(v) / 32767.0f;
        return std::clamp(f, -1.0f, 1.0f);
    }

    void Set(int pseudoVk, bool down)
    {
        g_buttons[pseudoVk - GP_FIRST] = down;
    }

    void ClearAll()
    {
        std::memset(g_buttons, 0, sizeof(g_buttons));
        g_lx = g_ly = g_rx = g_ry = 0.0f;
        g_lt = g_rt = 0.0f;
    }
} // namespace

void Gamepad::Update()
{
    if (!padEnabled)
    {
        if (g_connected)
        {
            g_connected = false;
            g_userIndex = -1;
            g_deviceName = "None";
            ClearAll();
        }
        return;
    }

    LoadXInput();
    if (!g_getState)
    {
        g_connected = false;
        g_deviceName = "XInput unavailable";
        ClearAll();
        return;
    }

    XI_STATE state{};
    bool found = false;

    // Polling an empty slot is expensive enough to matter at frame rate, so a
    // known-good index is reused and a full scan only runs once a second.
    if (g_userIndex >= 0 && g_getState(static_cast<DWORD>(g_userIndex), &state) == XI_ERROR_SUCCESS)
    {
        found = true;
    }
    else
    {
        static ULONGLONG s_nextScan = 0;
        const ULONGLONG now = GetTickCount64();
        if (g_userIndex >= 0 || now >= s_nextScan)
        {
            for (int i = 0; i < 4; ++i)
            {
                if (g_getState(static_cast<DWORD>(i), &state) == XI_ERROR_SUCCESS)
                {
                    g_userIndex = i;
                    ResolveDevice(i);
                    found = true;
                    break;
                }
            }

            if (!found)
            {
                g_userIndex = -1;
                s_nextScan = now + 1000;
            }
        }
    }

    if (!found)
    {
        if (g_connected)
        {
            g_connected = false;
            g_deviceName = "None";
        }
        ClearAll();
        return;
    }

    g_connected = true;

    const XI_GAMEPAD &pad = state.Gamepad;
    const WORD b = pad.wButtons;

    const float deadzone = std::clamp(padDeadzone, 0.0f, 0.9f);
    ApplyStickDeadzone(NormalizeThumb(pad.sThumbLX), NormalizeThumb(pad.sThumbLY), deadzone, g_lx, g_ly);
    ApplyStickDeadzone(NormalizeThumb(pad.sThumbRX), NormalizeThumb(pad.sThumbRY), deadzone, g_rx, g_ry);

    g_lt = static_cast<float>(pad.bLeftTrigger) / 255.0f;
    g_rt = static_cast<float>(pad.bRightTrigger) / 255.0f;

    const float trigger = std::clamp(padTriggerThreshold, 0.05f, 0.95f);

    Set(GP_A, (b & XI_A) != 0);
    Set(GP_B, (b & XI_B) != 0);
    Set(GP_X, (b & XI_X) != 0);
    Set(GP_Y, (b & XI_Y) != 0);
    Set(GP_LB, (b & XI_LSHOULDER) != 0);
    Set(GP_RB, (b & XI_RSHOULDER) != 0);
    Set(GP_LT, g_lt >= trigger);
    Set(GP_RT, g_rt >= trigger);
    Set(GP_BACK, (b & XI_BACK) != 0);
    Set(GP_START, (b & XI_START) != 0);
    Set(GP_L3, (b & XI_LTHUMB) != 0);
    Set(GP_R3, (b & XI_RTHUMB) != 0);
    Set(GP_DPAD_UP, (b & XI_DPAD_UP) != 0);
    Set(GP_DPAD_DOWN, (b & XI_DPAD_DOWN) != 0);
    Set(GP_DPAD_LEFT, (b & XI_DPAD_LEFT) != 0);
    Set(GP_DPAD_RIGHT, (b & XI_DPAD_RIGHT) != 0);

    // Digital stick directions need more travel than the deadzone alone or a
    // diagonal flick would fire two binds at once.
    constexpr float kDirection = 0.55f;
    Set(GP_LS_UP, g_ly >= kDirection);
    Set(GP_LS_DOWN, g_ly <= -kDirection);
    Set(GP_LS_LEFT, g_lx <= -kDirection);
    Set(GP_LS_RIGHT, g_lx >= kDirection);
    Set(GP_RS_UP, g_ry >= kDirection);
    Set(GP_RS_DOWN, g_ry <= -kDirection);
    Set(GP_RS_LEFT, g_rx <= -kDirection);
    Set(GP_RS_RIGHT, g_rx >= kDirection);
}

bool Gamepad::Connected()
{
    return g_connected;
}

int Gamepad::UserIndex()
{
    return g_userIndex;
}

int Gamepad::ResolvedLayout()
{
    if (padLayout == LayoutXbox || padLayout == LayoutPlayStation)
        return padLayout;
    return g_detectedLayout;
}

const char *Gamepad::DeviceName()
{
    return g_deviceName;
}

bool Gamepad::IsDown(int pseudoVk)
{
    if (!IsGamepadKey(pseudoVk))
        return false;
    return g_buttons[pseudoVk - GP_FIRST];
}

const char *Gamepad::KeyName(int pseudoVk)
{
    const bool ps = ResolvedLayout() == LayoutPlayStation;

    switch (pseudoVk)
    {
    case GP_A:
        return ps ? "Cross" : "A";
    case GP_B:
        return ps ? "Circle" : "B";
    case GP_X:
        return ps ? "Square" : "X";
    case GP_Y:
        return ps ? "Triangle" : "Y";
    case GP_LB:
        return ps ? "L1" : "LB";
    case GP_RB:
        return ps ? "R1" : "RB";
    case GP_LT:
        return ps ? "L2" : "LT";
    case GP_RT:
        return ps ? "R2" : "RT";
    case GP_BACK:
        return ps ? "Share" : "Back";
    case GP_START:
        return ps ? "Options" : "Start";
    case GP_L3:
        return ps ? "L3" : "LS";
    case GP_R3:
        return ps ? "R3" : "RS";
    case GP_DPAD_UP:
        return "D-Up";
    case GP_DPAD_DOWN:
        return "D-Down";
    case GP_DPAD_LEFT:
        return "D-Left";
    case GP_DPAD_RIGHT:
        return "D-Right";
    case GP_LS_UP:
        return "LS Up";
    case GP_LS_DOWN:
        return "LS Down";
    case GP_LS_LEFT:
        return "LS Left";
    case GP_LS_RIGHT:
        return "LS Right";
    case GP_RS_UP:
        return "RS Up";
    case GP_RS_DOWN:
        return "RS Down";
    case GP_RS_LEFT:
        return "RS Left";
    case GP_RS_RIGHT:
        return "RS Right";
    default:
        return "None";
    }
}

bool Gamepad::MenuComboPressed()
{
    bool held = false;

    switch (padMenuCombo)
    {
    case ComboL3R3:
        held = IsDown(GP_L3) && IsDown(GP_R3);
        break;
    case ComboBackStart:
        held = IsDown(GP_BACK) && IsDown(GP_START);
        break;
    default:
        held = false;
        break;
    }

    const bool fired = held && !g_comboPrev;
    g_comboPrev = held;
    return fired;
}

void Gamepad::DriveCursor(bool menuOpen)
{
    ImGuiIO &io = ImGui::GetIO();

    if (!padEnabled || !padCursorEnabled || !g_connected || !menuOpen)
    {
        g_cursorOwned = false;
        return;
    }

    // A physical mouse move lands somewhere other than the point written last
    // frame; that hands control back so the two devices never fight.
    if (g_cursorOwned)
    {
        const float dx = io.MousePos.x - g_lastWritten.x;
        const float dy = io.MousePos.y - g_lastWritten.y;
        if ((dx * dx + dy * dy) > 4.0f)
            g_cursorOwned = false;
    }

    const bool stickActive = (g_lx != 0.0f || g_ly != 0.0f);

    if (!g_cursorOwned)
    {
        if (!stickActive)
            return;

        // ImGui parks the pointer at -FLT_MAX when it has no valid position.
        if (io.MousePos.x > -1000.0f && io.MousePos.y > -1000.0f)
        {
            g_cursorX = io.MousePos.x;
            g_cursorY = io.MousePos.y;
        }
        else
        {
            g_cursorX = io.DisplaySize.x * 0.5f;
            g_cursorY = io.DisplaySize.y * 0.5f;
        }
        g_cursorOwned = true;
    }

    float dt = io.DeltaTime;
    if (dt <= 0.0f || dt > 0.25f)
        dt = 1.0f / 60.0f;

    const float mag = std::clamp(std::sqrt(g_lx * g_lx + g_ly * g_ly), 0.0f, 1.0f);
    if (mag > 0.0f)
    {
        // Quadratic response: small deflections stay precise enough to land on a
        // slider handle while full deflection still crosses the panel quickly.
        const float step = padCursorSpeed * mag * mag * dt;
        g_cursorX += (g_lx / mag) * step;
        g_cursorY -= (g_ly / mag) * step; // stick up is +Y, screen up is -Y
    }

    g_cursorX = std::clamp(g_cursorX, 0.0f, std::max(io.DisplaySize.x - 1.0f, 0.0f));
    g_cursorY = std::clamp(g_cursorY, 0.0f, std::max(io.DisplaySize.y - 1.0f, 0.0f));

    io.MousePos = ImVec2(g_cursorX, g_cursorY);
    g_lastWritten = io.MousePos;

    // Move the real cursor too, otherwise the visible pointer sits still while
    // the widgets respond somewhere else. Safe here: the overlay owns the
    // foreground whenever the menu is up, so this cannot reach the game camera.
    if (HWND hwnd = AppWindow::GetHandle())
    {
        POINT pt{static_cast<LONG>(g_cursorX), static_cast<LONG>(g_cursorY)};
        if (ClientToScreen(hwnd, &pt))
            SetCursorPos(pt.x, pt.y);
    }

    if (std::fabs(g_ry) > 0.0f)
        io.MouseWheel += g_ry * padScrollSpeed * dt;
}
