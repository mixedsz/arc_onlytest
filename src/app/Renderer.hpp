#pragma once

#include <Windows.h>
#include <d3d9.h>
#include <dwmapi.h>

#include "app/AppWindow.hpp"
#include "app/RenderDevice.hpp"
#include "main.hpp"
#include "gui/gui.h"
#include "gui/gui_colors.h"
#include "imgui.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"
#include "menu/menu.hpp"
#include "menu/framework/render/render.hpp"

#pragma comment(lib, "d3d9.lib")
#pragma comment(lib, "dwmapi.lib")

namespace GfxPipe
{
	namespace Kern
	{
		inline LPDIRECT3D9 d3d = nullptr;
		inline LPDIRECT3DDEVICE9 device = nullptr;
		inline D3DPRESENT_PARAMETERS presentParameters{};
		inline bool imguiReady = false;
		inline bool deviceLost = false;

		inline bool TryCreateDevice(
			HWND window,
			DWORD behavior,
			bool useDepth)
		{
			ZeroMemory(&presentParameters, sizeof(presentParameters));
			presentParameters.Windowed = TRUE;
			presentParameters.SwapEffect = D3DSWAPEFFECT_DISCARD;
			presentParameters.BackBufferFormat = D3DFMT_A8R8G8B8;
			presentParameters.EnableAutoDepthStencil = useDepth ? TRUE : FALSE;
			presentParameters.AutoDepthStencilFormat = D3DFMT_D16;
			presentParameters.PresentationInterval = D3DPRESENT_INTERVAL_IMMEDIATE;
			presentParameters.hDeviceWindow = window;

			const HRESULT result = d3d->CreateDevice(
				D3DADAPTER_DEFAULT,
				D3DDEVTYPE_HAL,
				window,
				behavior | D3DCREATE_FPU_PRESERVE,
				&presentParameters,
				&device);

			return SUCCEEDED(result) && device != nullptr;
		}

		inline bool BuildDevice(HWND window)
		{
			if (window == nullptr || !IsWindow(window))
				return false;

			d3d = Direct3DCreate9(D3D_SDK_VERSION);
			if (d3d == nullptr)
				return false;

			struct Attempt
			{
				DWORD behavior;
				bool useDepth;
			};

			constexpr Attempt attempts[] = {
				{ D3DCREATE_HARDWARE_VERTEXPROCESSING, true },
				{ D3DCREATE_HARDWARE_VERTEXPROCESSING, false },
				{ D3DCREATE_MIXED_VERTEXPROCESSING, false },
				{ D3DCREATE_SOFTWARE_VERTEXPROCESSING, false },
			};

			for (const Attempt& attempt : attempts)
			{
				if (TryCreateDevice(window, attempt.behavior, attempt.useDepth))
					return true;

				if (device != nullptr)
				{
					device->Release();
					device = nullptr;
				}
			}

			d3d->Release();
			d3d = nullptr;
			return false;
		}

		inline void ResetDevice()
		{
			if (device == nullptr)
				return;

			if (test::PlatformBadgeAtlas) {
				test::PlatformBadgeAtlas->Release();
				test::PlatformBadgeAtlas = nullptr;
			}

			ImGui_ImplDX9_InvalidateDeviceObjects();
			const HRESULT result = device->Reset(&presentParameters);
			if (FAILED(result))
			{
				deviceLost = true;
				return;
			}

			ImGui_ImplDX9_CreateDeviceObjects();
			deviceLost = false;
		}

		inline bool Initialize(HWND window)
		{
			if (!BuildDevice(window))
			{
				MessageBoxA(
					nullptr,
					"Failed to initialize the local Direct3D renderer.",
					"Renderer Error",
					MB_OK | MB_ICONERROR);
				return false;
			}

			IMGUI_CHECKVERSION();
			ImGui::CreateContext();
			ImGuiIO& io = ImGui::GetIO();
			io.IniFilename = nullptr;

			ImGuiStyle& style = ImGui::GetStyle();
			style.WindowPadding = ImVec2(0, 0);
			style.WindowBorderSize = 0;
			style.ItemSpacing = ImVec2(20, 20);
			style.ScrollbarSize = 4.0f;
			// AA is expensive with dense ESP; keep fill AA only.
			style.AntiAliasedLines = false;
			style.AntiAliasedLinesUseTex = false;
			style.AntiAliasedFill = true;
			style.Colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
			style.Colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
			style.Colors[ImGuiCol_PopupBg] = ImVec4(0, 0, 0, 0);

			const bool win32Ready = ImGui_ImplWin32_Init(window);
			const bool dx9Ready = win32Ready && ImGui_ImplDX9_Init(device);
			if (!dx9Ready)
			{
				if (win32Ready)
					ImGui_ImplWin32_Shutdown();
				ImGui::DestroyContext();

				MessageBoxA(
					nullptr,
					"Failed to initialize the ImGui renderer.",
					"Renderer Error",
					MB_OK | MB_ICONERROR);
				return false;
			}

			ui::initialize_fonts();
			f.RenderFonts();
			Render::Fonts::Init();
			Menu::Init();

			test::Device9 = device;
			wnd::gfx::hwnd = window;
			imguiReady = true;
			return true;
		}

		inline void Cleanup()
		{
			if (test::PlatformBadgeAtlas) {
				test::PlatformBadgeAtlas->Release();
				test::PlatformBadgeAtlas = nullptr;
			}
			test::Device9 = nullptr;

			if (imguiReady)
			{
				Menu::Shutdown();
				ImGui_ImplDX9_Shutdown();
				ImGui_ImplWin32_Shutdown();
				ImGui::DestroyContext();
				imguiReady = false;
			}

			if (device != nullptr)
			{
				device->Release();
				device = nullptr;
			}

			if (d3d != nullptr)
			{
				d3d->Release();
				d3d = nullptr;
			}
		}

		inline bool BeginFrame()
		{
			if (!imguiReady || device == nullptr)
				return false;

			AppWindow::PumpMessages();
			if (AppWindow::ShouldClose())
				return false;

			AppWindow::SyncToGame();

			UINT resizeWidth = 0;
			UINT resizeHeight = 0;
			if (AppWindow::ConsumeResize(resizeWidth, resizeHeight))
			{
				presentParameters.BackBufferWidth = resizeWidth;
				presentParameters.BackBufferHeight = resizeHeight;
				ResetDevice();
			}

			if (deviceLost)
			{
				const HRESULT cooperativeLevel = device->TestCooperativeLevel();
				if (cooperativeLevel == D3DERR_DEVICENOTRESET)
					ResetDevice();

				if (deviceLost)
					return false;
			}

			ImGui_ImplDX9_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			return true;
		}

		inline void PaceToCompositor()
		{
			// Align overlay presents with DWM so ESP tracks in-game fluidity
			// instead of spinning uncapped against the game thread.
			static LARGE_INTEGER freq = {};
			static LARGE_INTEGER last = {};
			if (freq.QuadPart == 0) {
				QueryPerformanceFrequency(&freq);
				QueryPerformanceCounter(&last);
			}

			double targetHz = 144.0;
			DWM_TIMING_INFO ti{};
			ti.cbSize = sizeof(ti);
			if (SUCCEEDED(DwmGetCompositionTimingInfo(nullptr, &ti)) && ti.rateRefresh.uiDenominator > 0) {
				targetHz = (double)ti.rateRefresh.uiNumerator / (double)ti.rateRefresh.uiDenominator;
				if (targetHz < 30.0) targetHz = 30.0;
				if (targetHz > 560.0) targetHz = 560.0;
			}

			const double targetDt = 1.0 / targetHz;
			LARGE_INTEGER now{};
			QueryPerformanceCounter(&now);
			const double elapsed = double(now.QuadPart - last.QuadPart) / double(freq.QuadPart);
			if (elapsed < targetDt) {
				const double remain = targetDt - elapsed;
				// Sleep most of the wait; spin the last ~0.5ms for tighter cadence.
				if (remain > 0.0015) {
					Sleep((DWORD)((remain - 0.0008) * 1000.0));
				}
				do {
					QueryPerformanceCounter(&now);
				} while (double(now.QuadPart - last.QuadPart) / double(freq.QuadPart) < targetDt);
			}
			QueryPerformanceCounter(&last);
		}

		inline void EndFrame()
		{
			if (!imguiReady || device == nullptr)
				return;

			ImGui::EndFrame();

			device->SetRenderState(D3DRS_ZENABLE, FALSE);
			device->SetRenderState(D3DRS_ALPHABLENDENABLE, FALSE);
			device->SetRenderState(D3DRS_SCISSORTESTENABLE, FALSE);
			// Color clear only — skip Z clear when depth is unused.
			device->Clear(
				0,
				nullptr,
				D3DCLEAR_TARGET,
				D3DCOLOR_RGBA(0, 0, 0, 0),
				1.0f,
				0);

			if (device->BeginScene() >= 0)
			{
				ImGui::Render();
				ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());
				device->EndScene();
			}

			const HRESULT result = device->Present(
				nullptr,
				nullptr,
				nullptr,
				nullptr);

			if (result == D3DERR_DEVICELOST)
				deviceLost = true;

			PaceToCompositor();
		}

		inline bool IsReady() noexcept
		{
			return imguiReady && device != nullptr;
		}
	}

	inline bool Initialize(HWND window)
	{
		return Kern::Initialize(window);
	}

	inline void Cleanup()
	{
		Kern::Cleanup();
	}

	inline bool BeginFrame()
	{
		return Kern::BeginFrame();
	}

	inline void EndFrame()
	{
		Kern::EndFrame();
	}

	inline void DrawMenu()
	{
		if (Kern::IsReady())
			ui::Render();
	}
}
