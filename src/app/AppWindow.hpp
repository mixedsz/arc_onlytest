#pragma once

#include <Windows.h>
#include <dwmapi.h>
#include <TlHelp32.h>

#include "imgui.h"
#include "imgui_impl_win32.h"

#pragma comment(lib, "dwmapi.lib")

#ifndef WDA_EXCLUDEFROMCAPTURE
#define WDA_EXCLUDEFROMCAPTURE 0x00000011
#endif
#ifndef WDA_NONE
#define WDA_NONE 0x00000000
#endif

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(
	HWND window,
	UINT message,
	WPARAM wParam,
	LPARAM lParam);

namespace AppWindow
{
	// Benign names — avoid "overlay" / game-cheat class titles.
	inline wchar_t ClassName[32] = L"IME";
	inline constexpr wchar_t WindowTitle[] = L"";

	inline HWND handle = nullptr;
	inline HWND gameHwnd = nullptr;
	inline HWND previousForeground = nullptr;
	inline HINSTANCE instance = nullptr;
	inline bool classRegistered = false;
	inline bool interactive = false;
	inline bool quitRequested = false;
	inline UINT pendingWidth = 0;
	inline UINT pendingHeight = 0;
	inline int lastX = INT_MIN;
	inline int lastY = INT_MIN;
	inline int lastW = 0;
	inline int lastH = 0;
	inline DWORD targetPid = 0;

	inline void SetTargetPid(DWORD pid) noexcept
	{
		targetPid = pid;
	}

	inline DWORD FindGamePid()
	{
		if (targetPid != 0)
		{
			HANDLE proc = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, targetPid);
			if (proc)
			{
				CloseHandle(proc);
				return targetPid;
			}
			targetPid = 0;
		}

		static const wchar_t* kNames[] = {
			L"PioneerGame.exe",
			L"PioneerGame-Win64-Shipping.exe",
			L"PioneerGame-d.exe",
			L"PioneerGame-e.exe",
		};

		HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
		if (snap == INVALID_HANDLE_VALUE)
			return 0;

		PROCESSENTRY32W pe{};
		pe.dwSize = sizeof(pe);
		DWORD found = 0;
		if (Process32FirstW(snap, &pe))
		{
			do
			{
				for (const wchar_t* name : kNames)
				{
					if (_wcsicmp(pe.szExeFile, name) == 0)
					{
						found = pe.th32ProcessID;
						break;
					}
				}
			} while (!found && Process32NextW(snap, &pe));
		}
		CloseHandle(snap);
		if (found)
			targetPid = found;
		return found;
	}

	inline HWND FindGameWindow()
	{
		const DWORD pid = FindGamePid();
		if (!pid)
			return nullptr;

		struct EnumData
		{
			DWORD pid;
			HWND best;
			int bestArea;
		} data{ pid, nullptr, 0 };

		EnumWindows(
			[](HWND hwnd, LPARAM lParam) -> BOOL
			{
				auto* d = reinterpret_cast<EnumData*>(lParam);
				DWORD windowPid = 0;
				GetWindowThreadProcessId(hwnd, &windowPid);
				if (windowPid != d->pid || !IsWindowVisible(hwnd) || IsIconic(hwnd))
					return TRUE;
				if (GetWindow(hwnd, GW_OWNER) != nullptr)
					return TRUE;

				RECT rc{};
				if (!GetClientRect(hwnd, &rc))
					return TRUE;

				const int area = (rc.right - rc.left) * (rc.bottom - rc.top);
				if (area > d->bestArea && area > 20000)
				{
					d->bestArea = area;
					d->best = hwnd;
				}
				return TRUE;
			},
			reinterpret_cast<LPARAM>(&data));

		return data.best;
	}

	inline bool streamProofEnabled = true;

	inline void ApplyStreamProof(HWND hwnd)
	{
		if (!hwnd)
			return;
		SetWindowDisplayAffinity(hwnd, streamProofEnabled ? WDA_EXCLUDEFROMCAPTURE : WDA_NONE);
	}

	inline void SetStreamProof(bool enabled)
	{
		if (streamProofEnabled == enabled)
			return;
		streamProofEnabled = enabled;
		if (handle && IsWindow(handle))
			ApplyStreamProof(handle);
	}

	inline void ApplyStealth(HWND hwnd)
	{
		if (!hwnd)
			return;

		ApplyStreamProof(hwnd);

		// Don't appear in taskbar / alt-tab aggressively.
		LONG_PTR style = GetWindowLongPtrW(hwnd, GWL_EXSTYLE);
		style |= WS_EX_TOOLWINDOW | WS_EX_NOACTIVATE;
		style &= ~(WS_EX_APPWINDOW | WS_EX_TOPMOST);
		SetWindowLongPtrW(hwnd, GWL_EXSTYLE, style);
	}

	// Re-apply stealth after style changes (menu interactive toggle, etc.).
	inline void RefreshStealth()
	{
		if (handle && IsWindow(handle))
			ApplyStealth(handle);
	}

	inline LRESULT CALLBACK WindowProc(
		HWND window,
		UINT message,
		WPARAM wParam,
		LPARAM lParam)
	{
		if (ImGui::GetCurrentContext() != nullptr
			&& ImGui_ImplWin32_WndProcHandler(window, message, wParam, lParam))
		{
			return TRUE;
		}

		switch (message)
		{
		case WM_SIZE:
			if (wParam != SIZE_MINIMIZED)
			{
				pendingWidth = static_cast<UINT>(LOWORD(lParam));
				pendingHeight = static_cast<UINT>(HIWORD(lParam));
			}
			return 0;

		case WM_NCHITTEST:
			return interactive ? HTCLIENT : HTTRANSPARENT;

		case WM_MOUSEACTIVATE:
			return interactive ? MA_ACTIVATE : MA_NOACTIVATE;

		case WM_ACTIVATE:
			// Never steal focus while click-through.
			if (!interactive && LOWORD(wParam) != WA_INACTIVE)
				return 0;
			break;

		case WM_ERASEBKGND:
			return 1;

		case WM_CLOSE:
			DestroyWindow(window);
			return 0;

		case WM_DESTROY:
			handle = nullptr;
			quitRequested = true;
			PostQuitMessage(0);
			return 0;

		default:
			break;
		}

		return DefWindowProcW(window, message, wParam, lParam);
	}

	// A DPI-unaware thread gets virtualized window geometry: on a 2560x1440 display at
	// 150% scaling GetClientRect reports 1707x960, the overlay is created at that size,
	// and DWM stretches the result back over the game. That is what paints the ESP at a
	// lower resolution than Arc and leaves it short of the right/bottom edges.
	// Per-monitor-v2 gives real pixels for both our client rect and the game's.
	// Thread-scoped rather than process-wide on purpose — under BUILD_DLL we are inside
	// a host process and must not change its awareness, which would re-scale its own UI.
	// Window awareness is inherited from the creating thread's context, and Create(),
	// SyncToGame(), and the render loop all run on this one thread, so a single call
	// covers window creation and every geometry call that follows.
	inline void EnableDpiAwareness()
	{
		static bool applied = false;
		if (applied)
			return;
		applied = true;

		HMODULE user32 = GetModuleHandleW(L"user32.dll");
		if (user32 == nullptr)
			return;

		// Opaque DPI_AWARENESS_CONTEXT values: -4 per-monitor-v2, -3 per-monitor.
		// Declared locally because the winuser.h prototypes are gated behind WINVER.
		using SetThreadCtxFn = void* (WINAPI*)(void*);
		auto setThreadCtx = reinterpret_cast<SetThreadCtxFn>(
			GetProcAddress(user32, "SetThreadDpiAwarenessContext")); // Win10 1607+

		if (setThreadCtx != nullptr)
		{
			if (setThreadCtx(reinterpret_cast<void*>(-4)) != nullptr)
				return;
			if (setThreadCtx(reinterpret_cast<void*>(-3)) != nullptr)
				return;
		}

#ifndef BUILD_DLL
		// Standalone overlay process on a build without the thread-scoped API: process
		// wide is safe here because the process is ours.
		using SetProcessAwarenessFn = HRESULT(WINAPI*)(int);
		if (HMODULE shcore = LoadLibraryW(L"shcore.dll"))
		{
			auto setProcessAwareness = reinterpret_cast<SetProcessAwarenessFn>(
				GetProcAddress(shcore, "SetProcessDpiAwareness"));
			if (setProcessAwareness != nullptr
				&& SUCCEEDED(setProcessAwareness(2))) // PROCESS_PER_MONITOR_DPI_AWARE
				return;
		}

		using SetProcessDpiAwareFn = BOOL(WINAPI*)(void);
		if (auto setProcessDpiAware = reinterpret_cast<SetProcessDpiAwareFn>(
				GetProcAddress(user32, "SetProcessDPIAware")))
			setProcessDpiAware();
#endif
	}

	inline bool Create()
	{
		if (handle != nullptr && IsWindow(handle))
			return true;

		EnableDpiAwareness();

		quitRequested = false;
		instance = GetModuleHandleW(nullptr);
		if (instance == nullptr)
			return false;

		// Unique-enough class per run without advertising "overlay".
		wsprintfW(ClassName, L"Static.%08X", GetCurrentProcessId() ^ 0xA5A5A5A5u);

		WNDCLASSEXW windowClass{};
		windowClass.cbSize = sizeof(windowClass);
		windowClass.style = CS_HREDRAW | CS_VREDRAW;
		windowClass.lpfnWndProc = WindowProc;
		windowClass.hInstance = instance;
		windowClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		windowClass.lpszClassName = ClassName;
		windowClass.hbrBackground = nullptr;

		if (RegisterClassExW(&windowClass) != 0)
		{
			classRegistered = true;
		}
		else if (GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
		{
			return false;
		}

		gameHwnd = FindGameWindow();

		// No WS_EX_TOPMOST — owned by the game so other windows can cover ESP.
		const DWORD extendedStyle =
			WS_EX_LAYERED
			| WS_EX_TRANSPARENT
			| WS_EX_TOOLWINDOW
			| WS_EX_NOACTIVATE;

		handle = CreateWindowExW(
			extendedStyle,
			ClassName,
			WindowTitle,
			WS_POPUP,
			0,
			0,
			2,
			2,
			gameHwnd, // owner → z-order follows Arc
			nullptr,
			instance,
			nullptr);

		if (handle == nullptr)
			return false;

		SetLayeredWindowAttributes(handle, RGB(0, 0, 0), 255, LWA_ALPHA);

		MARGINS margins{ -1 };
		DwmExtendFrameIntoClientArea(handle, &margins);

		ApplyStealth(handle);

		ShowWindow(handle, SW_HIDE);
		return true;
	}

	// Stick to Arc's client rect and sit directly above the game (not above the desktop).
	inline void SyncToGame()
	{
		if (handle == nullptr || !IsWindow(handle))
			return;

		// Geometry / HWND work is expensive — do not do it every Present.
		static ULONGLONG s_lastSyncMs = 0;
		const ULONGLONG nowMs = GetTickCount64();
		const bool due = (nowMs - s_lastSyncMs) >= 32; // ~30 Hz window tracking
		if (!due && gameHwnd && IsWindow(gameHwnd) && IsWindowVisible(handle))
			return;
		s_lastSyncMs = nowMs;

		HWND game = gameHwnd;
		if (game == nullptr || !IsWindow(game) || !IsWindowVisible(game) || IsIconic(game))
			game = FindGameWindow();

		gameHwnd = game;
		if (game == nullptr)
		{
			ShowWindow(handle, SW_HIDE);
			return;
		}

		// Keep ownership so the overlay stacks with the game, under other apps.
		const HWND owner = reinterpret_cast<HWND>(GetWindowLongPtrW(handle, GWLP_HWNDPARENT));
		if (owner != game)
			SetWindowLongPtrW(handle, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(game));

		RECT client{};
		if (!GetClientRect(game, &client))
		{
			ShowWindow(handle, SW_HIDE);
			return;
		}

		POINT tl{ client.left, client.top };
		POINT br{ client.right, client.bottom };
		ClientToScreen(game, &tl);
		ClientToScreen(game, &br);

		const int x = tl.x;
		const int y = tl.y;
		const int w = br.x - tl.x;
		const int h = br.y - tl.y;
		if (w < 64 || h < 64)
		{
			ShowWindow(handle, SW_HIDE);
			return;
		}

		const bool moved =
			x != lastX || y != lastY || w != lastW || h != lastH;

		static ULONGLONG s_lastZOrderMs = 0;
		const bool needZOrder = (nowMs - s_lastZOrderMs) > 500;

		if (moved || needZOrder)
		{
			SetWindowPos(
				handle,
				game,
				x,
				y,
				w,
				h,
				SWP_SHOWWINDOW | SWP_NOACTIVATE | (moved ? 0 : SWP_NOMOVE | SWP_NOSIZE));
			s_lastZOrderMs = nowMs;
		}
		else if (!IsWindowVisible(handle))
		{
			ShowWindow(handle, SW_SHOWNA);
		}

		if (moved)
		{
			lastX = x;
			lastY = y;
			lastW = w;
			lastH = h;
			pendingWidth = static_cast<UINT>(w);
			pendingHeight = static_cast<UINT>(h);
		}

		static bool s_stealthApplied = false;
		if (!s_stealthApplied)
		{
			ApplyStealth(handle);
			s_stealthApplied = true;
		}
	}

	inline void SetInteractive(bool enabled)
	{
		if (handle == nullptr || !IsWindow(handle) || interactive == enabled)
			return;

		interactive = enabled;
		LONG_PTR style = GetWindowLongPtrW(handle, GWL_EXSTYLE);

		if (enabled)
		{
			previousForeground = GetForegroundWindow();
			style &= ~(WS_EX_TRANSPARENT | WS_EX_NOACTIVATE);
		}
		else
		{
			style |= WS_EX_TRANSPARENT | WS_EX_NOACTIVATE;
		}

		// Never re-introduce TOPMOST.
		style &= ~WS_EX_TOPMOST;
		SetWindowLongPtrW(handle, GWL_EXSTYLE, style);
		ApplyStreamProof(handle);
		SetWindowPos(
			handle,
			gameHwnd ? gameHwnd : HWND_TOP,
			0,
			0,
			0,
			0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_FRAMECHANGED
				| (enabled ? 0 : SWP_NOACTIVATE));

		if (enabled)
		{
			SetForegroundWindow(handle);
			SetFocus(handle);
		}
		else if (previousForeground != nullptr
			&& previousForeground != handle
			&& IsWindow(previousForeground))
		{
			SetForegroundWindow(previousForeground);
			previousForeground = nullptr;
		}
		else if (gameHwnd && IsWindow(gameHwnd))
		{
			SetForegroundWindow(gameHwnd);
		}
	}

	inline void PumpMessages()
	{
		MSG message{};
		while (PeekMessageW(&message, nullptr, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
				quitRequested = true;

			TranslateMessage(&message);
			DispatchMessageW(&message);
		}
	}

	inline bool ConsumeResize(UINT& width, UINT& height)
	{
		if (pendingWidth == 0 || pendingHeight == 0)
			return false;

		width = pendingWidth;
		height = pendingHeight;
		pendingWidth = 0;
		pendingHeight = 0;
		return true;
	}

	inline bool GetClientSize(UINT& width, UINT& height)
	{
		if (handle == nullptr)
			return false;

		// Prefer live game client size so DX matches Arc immediately.
		if (gameHwnd && IsWindow(gameHwnd))
		{
			RECT client{};
			if (GetClientRect(gameHwnd, &client))
			{
				width = static_cast<UINT>(client.right - client.left);
				height = static_cast<UINT>(client.bottom - client.top);
				if (width != 0 && height != 0)
					return true;
			}
		}

		RECT client{};
		if (!GetClientRect(handle, &client))
			return false;

		width = static_cast<UINT>(client.right - client.left);
		height = static_cast<UINT>(client.bottom - client.top);
		return width != 0 && height != 0;
	}

	inline bool ShouldClose() noexcept
	{
		return quitRequested;
	}

	inline HWND GetHandle() noexcept
	{
		return handle;
	}

	inline HWND GetGameHandle() noexcept
	{
		return gameHwnd;
	}

	inline void Destroy()
	{
		if (handle != nullptr && IsWindow(handle))
			DestroyWindow(handle);

		handle = nullptr;
		gameHwnd = nullptr;

		if (classRegistered && instance != nullptr)
			UnregisterClassW(ClassName, instance);

		classRegistered = false;
		instance = nullptr;
	}
}
