#include <Windows.h>
#include <d3d9.h>
#include <string>
#include <stdexcept>
#include <TlHelp32.h>
#include "main.hpp"
#include "config/build_config.hpp"
#include "debug/Logger.hpp"
#include "app/init.hpp"
#include "game/Loop.hpp"
#include "game/HudRender.hpp"
#include "game/AlertManager.hpp"
#include "game/CacheTypes.hpp"
#include "game/OffsetHealth.hpp"
#include <chrono>
#include <cstdio>
#include <iostream>
#include "app/AppWindow.hpp"
#include "app/Renderer.hpp"
#include "dgb/debug.h"
#include <DbgHelp.h>
#include "sdk/Platform.hpp"
#include "dgb/lazy.hpp"
#include "dgb/Spoof.h"
#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dbghelp.lib")

using namespace std::chrono;

auto lastTime = steady_clock::now();
int frameCount = 0;

inline bool pointersResolved = false;
volatile LONG g_coreReady = 0;
static bool g_DenuvoDetected = false;
static std::wstring g_GameExePath;

inline std::wstring QueryModulePath(DWORD pid)
{
	SPOOF_FUNC;
	std::wstring result = L"";

	HANDLE hProcess = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, pid);
	if (!hProcess)
		return result;

	wchar_t buffer[MAX_PATH];
	DWORD size = MAX_PATH;

	if (QueryFullProcessImageNameW(hProcess, 0, buffer, &size))
		result = buffer;

    CloseHandle(hProcess);
	return result;
}

inline PlatformVersion ResolvePlatformByPid(DWORD pid)
{
	SPOOF_FUNC;
	std::wstring exePath = QueryModulePath(pid);
	PlatformVersion detected = DetectPlatformFromExePath(exePath);
	return detected;
}

inline std::wstring GetEacSettingsPath(const std::wstring& exePath) {
    SPOOF_FUNC;
    std::wstring path = exePath;

    auto stripTrailingSep = [](std::wstring& s) {
        while (!s.empty() && (s.back() == L'\\' || s.back() == L'/'))
            s.pop_back();
    };

    for (int i = 0; i < 4; i++) {
        stripTrailingSep(path);
        size_t pos = path.find_last_of(L"\\/");
        if (pos == std::wstring::npos) return L"";
        path = path.substr(0, pos);
    }

    stripTrailingSep(path);
    return path + L"\\EasyAntiCheat\\Settings.json";
}

inline void DisableDenuvo(const std::wstring& exePath) {
    SPOOF_FUNC;

    std::wstring settingsPath = GetEacSettingsPath(exePath);
    if (settingsPath.empty()) {
        LI_FN(MessageBoxA)(nullptr, X("Patch failed.\nPlease try again or open ticket with the error #995"), X("Error"), MB_OK | MB_ICONERROR);
        LI_FN(ExitProcess)(0);
        return;
    }

    HANDLE hFile = LI_FN(CreateFileW)(settingsPath.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        LI_FN(MessageBoxA)(nullptr, X("Patch failed.\nPlease try again or open ticket with the error #996"), X("Error"), MB_OK | MB_ICONERROR);
        LI_FN(ExitProcess)(0);
        return;
    }

    DWORD fileSize = LI_FN(GetFileSize)(hFile, nullptr);
    if (fileSize == INVALID_FILE_SIZE || fileSize == 0 || fileSize > 0x10000) {
        LI_FN(CloseHandle)(hFile);
        LI_FN(MessageBoxA)(nullptr, X("Patch failed.\nPlease try again or open ticket with the error #997"), X("Error"), MB_OK | MB_ICONERROR);
        LI_FN(ExitProcess)(0);
        return;
    }

    std::string content(fileSize, '\0');
    DWORD bytesRead = 0;
    LI_FN(ReadFile)(hFile, &content[0], fileSize, &bytesRead, nullptr);
    LI_FN(CloseHandle)(hFile);

    bool patched = false;
    size_t pos = 0;
    while ((pos = content.find(X("PioneerGame-d.exe"), pos)) != std::string::npos) {
        content.replace(pos, 17, X("PioneerGame-e.exe"));
        patched = true;
        pos += 17;
    }

    if (!patched) {
        LI_FN(MessageBoxA)(nullptr, X("Patch failed.\nPlease try again or open ticket with the error #999."), X("Error"), MB_OK | MB_ICONERROR);
        LI_FN(ExitProcess)(0);
        return;
    }

    hFile = LI_FN(CreateFileW)(settingsPath.c_str(), GENERIC_WRITE, 0, nullptr, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (hFile == INVALID_HANDLE_VALUE) {
        LI_FN(MessageBoxA)(nullptr, X("Patch failed.\nPlease try again or open ticket with the error #998"), X("Error"), MB_OK | MB_ICONERROR);
        LI_FN(ExitProcess)(0);
        return;
    }

    DWORD bytesWritten = 0;
    LI_FN(WriteFile)(hFile, content.c_str(), (DWORD)content.size(), &bytesWritten, nullptr);
    LI_FN(CloseHandle)(hFile);

    LI_FN(MessageBoxA)(nullptr,
        X("Denuvo protection detected.\n\n"
           "Denuvo configuration has been patched automatically.\n"
           "Please restart the game and inject again."),
        X("Patch Applied"), MB_OK | MB_ICONINFORMATION);

    DWORD gamePid = findProcess(X(L"PioneerGame-d.exe"));
    if (gamePid) {
        HANDLE hProc = LI_FN(OpenProcess)(PROCESS_TERMINATE, FALSE, gamePid);
        if (hProc) {
            LI_FN(TerminateProcess)(hProc, 0);
            LI_FN(CloseHandle)(hProc);
        }
    }

    LI_FN(ExitProcess)(0);
}

auto BootTargetCtx() -> int {
    SPOOF_FUNC;

    ctx->process_id = findProcess(X(L"PioneerGame.exe"));

    if (!ctx->process_id) {
        ctx->process_id = findProcess(X(L"PioneerGame-d.exe"));






    }

    if (!ctx->process_id) {
        ctx->process_id = findProcess(X(L"PioneerGame-e.exe"));






    }

    if (!ctx->process_id) {
        PushNotification(X("Failed to find Arc Raiders process"), NotifType::Error, 9999.0f, true);
        global::bStopExecution = 1;
        return 1;
    }

    AppWindow::SetTargetPid(ctx->process_id);

    g_GameExePath = QueryModulePath(ctx->process_id);
    g_PlatformVersion = DetectPlatformFromExePath(g_GameExePath);

    ctx->image_base = ctx->find_image();

    bool cr3_resolved = false;
    for (int attempt = 0; attempt < 15; attempt++) {
        LI_FN(Sleep)(1000);

        ctx->cr3 = ctx->resolve_cr3_aggressive(ctx->process_id, ctx->image_base);

        if (ctx->cr3 != 0) {
            uint16_t peCheck = 0;
            ctx->read_physical(reinterpret_cast<PVOID>(ctx->image_base), &peCheck, sizeof(peCheck));
            if (peCheck == 0x5A4D) {
                cr3_resolved = true;
                break;
            }
        }
    }

    if (!cr3_resolved) {
        DBG_LOG("[Boot] CR3 resolve failed after retries");
        return 1;
    }

    LI_FN(Beep)(300, 300);
    return 0;
}

static bool CoreSetup() {
    SPOOF_FUNC;
    {
        const int memoryResult = ctx->initialize_handle();
        if (memoryResult != 0) {
            PushNotification(X("Memory interface initialization failed"), NotifType::Error, 9999.0f, true);
            return false;
        }
    }

    if (!ctx->is_ready()) {
        PushNotification(X("Memory interface is disabled in this build"), NotifType::Info, 8.0f);
        return false;
    }

    if (BootTargetCtx() != 0) {
        return false;
    }

    {
        auto health = OffsetHealth::RunHealthCheck(ctx->image_base);
        // Printed whether or not the check passes. It used to log only on failure, which
        // meant a passing run said nothing at all - including the GWorld line that now
        // reports the one pointer the whole ESP hangs off. Warnings matter too: a build
        // that starts with a broken GObjects decryption is running degraded, and that
        // should be visible rather than silent.
        for (const auto& issue : health.issues) {
            char line[256];
            snprintf(line, sizeof(line), "[OffsetHealth] group=%d severity=%d field=%s msg=%s",
                static_cast<int>(issue.group), static_cast<int>(issue.severity), issue.field.c_str(), issue.message.c_str());
            DBG_LOG(line);
        }
        if (!health.healthy) {
            PushNotification(X("Offset health check failed - see debug console"), NotifType::Error, 9999.0f, true);
            return false;
        }
    }

    if (g_GameExePath.empty())
        g_GameExePath = QueryModulePath(ctx->process_id);
    g_PlatformVersion = DetectPlatformFromExePath(g_GameExePath);

    LI_FN(Sleep)(2000);

    uint64_t vp = 0;
    uint32_t vpIdx = 0;

    // A missing viewport is only fatal if UWorld is unreachable without it. It used to be
    // treated as unconditionally fatal, so with the GObjects decryption broken this
    // returned false and SpawnWorkers() never ran - the ESP was not failing, it was never
    // starting. GWorld gives UWorld directly, which is all the player path needs; the
    // viewport only adds the ViewIndex write.
    // Startup no longer hinges on resolving a world right now. GWorld validation requires
    // a populated Levels array, which is not true while the game is loading or between
    // maps - so a single failed read here aborted the whole session even though
    // OffsetHealth had validated the same pointer seconds earlier. The raid state machine
    // already retries every 200ms; the workers just need to exist for it to do that.
    const bool haveViewPort = InitializeViewPort(vp, vpIdx);
    const uint64_t bootWorld = GetGWorld(ctx->image_base);

    NT::Store::SpawnWorkers();
    LaunchFrameSyncWorker();
    if (haveViewPort)
        NT::Store::Entity->SetViewPort(vp, vpIdx);

    if (!haveViewPort && !bootWorld) {
        DBG_LOG("[Main] no viewport and no world at boot; workers started, will retry\n");
        PushNotification(X("Waiting for world..."), NotifType::Info, 6.0f, true);
    } else if (!haveViewPort) {
        DBG_LOG("[Main] no viewport; running off GWorld\n");
    }

    return true;
}

static bool BeginSetup() {
    SPOOF_FUNC;
    return CoreSetup();
}

static DWORD WINAPI DeferredBootProc(LPVOID) {
    SPOOF_FUNC;
    if (!BeginSetup()) {
        return 1;
    }
    InterlockedExchange(&g_coreReady, 1);
    return 0;
}

static bool WaitForActivationKey()
{
    SPOOF_FUNC;

    while ((LI_FN(GetAsyncKeyState)(VK_F1) & 0x8000) != 0)
    {
        if (InterlockedCompareExchange(&global::bStopExecution, 0, 0))
            return false;
        LI_FN(Sleep)(50);
    }


    for (;;)
    {
        if (InterlockedCompareExchange(&global::bStopExecution, 0, 0))
            return false;

        if (LI_FN(GetAsyncKeyState)(VK_F1) & 0x8000)
            break;

        LI_FN(Sleep)(50);
    }

    while ((LI_FN(GetAsyncKeyState)(VK_F1) & 0x8000) != 0)
    {
        LI_FN(Sleep)(50);
    }

    return true;
}

static void RenderLoopIteration(UINT width, UINT height, bool& exitApp)
{
    SPOOF_FUNC;

    static bool s_prioSet = false;
    if (!s_prioSet) {
        s_prioSet = true;
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);
    }

    if (AppWindow::ShouldClose()) {
        exitApp = true;
        return;
    }

    if (!GfxPipe::BeginFrame())
    {
        if (AppWindow::ShouldClose())
            exitApp = true;
        LI_FN(Sleep)(1);
        return;
    }

    if (g_coreReady) {
        FrameDispatch();
    }
    else
    {
        RenderViewportLoadingNotification();
    }

    // Outside the g_coreReady branch on purpose: the HUD reads no game memory, so it has
    // no reason to wait for a world, and drawing it here rather than inside FrameDispatch
    // keeps it clear of the early returns and the swallowing catch in the ESP path. Order
    // puts it above ESP and below notifications and the menu.
    Hud::Draw(ImGui::GetBackgroundDrawList());
    Alerts::Tick(ImGui::GetBackgroundDrawList(), ImGui::GetIO().DeltaTime);

    RenderNotifications();
    GfxPipe::DrawMenu();

    GfxPipe::EndFrame();

    if (LI_FN(GetAsyncKeyState)(VK_END) & 0x8000)
        exitApp = true;

    if (InterlockedCompareExchange(&global::bStopExecution, 0, 0))
        exitApp = true;
}

void entry_sequence() {
    SPOOF_FUNC;

    // Two session-start lines with no session-end between them is two live overlays.
    // Each one creates its own D3D9 window, so INSERT opens two menus on top of each other.
    HANDLE already = CreateMutexA(nullptr, TRUE, "Local\\SyndikateArcSingleInstance");
    if (already && GetLastError() == ERROR_ALREADY_EXISTS)
    {
        LI_FN(MessageBoxA)(nullptr, X("Already running. Close the other instance first."), X("Syndikate Arc"), MB_OK | MB_ICONWARNING);
        LI_FN(ExitProcess)(1);
    }

    if (!LocalInit()) {
        LI_FN(ExitProcess)(1);
    }

    LI_FN(MessageBoxA)(nullptr, X("Press F1 in game lobby"), X("Injected"), MB_OK | MB_ICONASTERISK);

    if (!WaitForActivationKey())
    {
        LI_FN(Sleep)(1500);
        LI_FN(ExitProcess)(1);
    }

    try {
        if (!AppWindow::Create())
        {
            LI_FN(MessageBoxA)(
                nullptr,
                X("Failed to create the application window."),
                X("Window Error"),
                MB_OK | MB_ICONERROR);
            LI_FN(Sleep)(1500);
            LI_FN(ExitProcess)(1);
        }

        AppWindow::SyncToGame();

        if (!GfxPipe::Initialize(AppWindow::GetHandle()))
        {
            AppWindow::Destroy();
            LI_FN(Sleep)(1500);
            LI_FN(ExitProcess)(1);
        }

        UINT width = 0;
        UINT height = 0;
        // Fall back to a sane default until we latch onto the game HWND.
        if (!AppWindow::GetClientSize(width, height))
        {
            width = 1280;
            height = 720;
        }
        bool exitApp = false;

        HANDLE hGameInit = LI_FN(CreateThread)(nullptr, 0, DeferredBootProc, nullptr, 0, nullptr);
        if (hGameInit) LI_FN(CloseHandle)(hGameInit);

        while (!exitApp)
        {
            RenderLoopIteration(width, height, exitApp);
        }

        GfxPipe::Cleanup();
        AppWindow::Destroy();
    }
    catch (const std::exception&) {
        LI_FN(ExitProcess)(1);
    }
    catch (...) {
        LI_FN(ExitProcess)(1);
    }
}

#ifdef BUILD_DLL

DWORD WINAPI DeferredEntryProc(LPVOID lpParam) {
    SPOOF_FUNC;
    (void)lpParam;

    try {
        Logger_Init();
        entry_sequence();
    }
    catch (const std::exception&) {
        LI_FN(ExitProcess)(1);
    }
    catch (...) {
        LI_FN(ExitProcess)(1);
    }
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD ul_reason_for_call, LPVOID lpReserved) {
    SPOOF_FUNC;
    (void)lpReserved;
    switch (ul_reason_for_call) {
    case DLL_PROCESS_ATTACH:
        LI_FN(DisableThreadLibraryCalls)(hModule);
        {
            HANDLE h = LI_FN(CreateThread)(nullptr, 0, DeferredEntryProc, hModule, 0, nullptr);
            if (h) LI_FN(CloseHandle)(h);
        }
        break;
    case DLL_PROCESS_DETACH:
        break;
    }
    return TRUE;
}

#else

int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int) {
    SPOOF_FUNC;
    DBG_CONSOLE_INIT();
    Logger_Init();
    DBG_LOG("[Main] Console attached");

    entry_sequence();
    return 0;
}

#endif
