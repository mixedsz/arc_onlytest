#include <Cheat.hpp>
#include "menu.hpp"
#include "settings/Settings.hpp"
#include <filesystem>
#include <shellapi.h>
#include <commdlg.h>
#include <iterator>

#pragma comment(lib, "comdlg32.lib")

static CContainer* Section(CObject* parent, const char* title, const char* persistId)
{
	auto* box = parent->AddObject<CContainer>(title);
	box->SetPersistId(persistId);
	return box;
}

static const char kB64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

static std::string B64Encode(const std::string& in)
{
	std::string out;
	out.reserve(((in.size() + 2) / 3) * 4);
	int val = 0, valb = -6;
	for (unsigned char c : in)
	{
		val = (val << 8) + c;
		valb += 8;
		while (valb >= 0)
		{
			out.push_back(kB64[(val >> valb) & 0x3F]);
			valb -= 6;
		}
	}
	if (valb > -6)
		out.push_back(kB64[((val << 8) >> (valb + 8)) & 0x3F]);
	while (out.size() % 4)
		out.push_back('=');
	return out;
}

static std::string B64Decode(const std::string& in)
{
	std::string clean;
	for (char c : in)
	{
		if (c != '\r' && c != '\n' && c != ' ')
			clean.push_back(c);
	}
	int T[256];
	std::fill(std::begin(T), std::end(T), -1);
	for (int i = 0; i < 64; ++i)
		T[(unsigned char)kB64[i]] = i;
	std::string out;
	int val = 0, valb = -8;
	for (unsigned char c : clean)
	{
		if (T[c] == -1)
			break;
		val = (val << 6) + T[c];
		valb += 6;
		if (valb >= 0)
		{
			out.push_back(char((val >> valb) & 0xFF));
			valb -= 8;
		}
	}
	return out;
}

static bool CopyToClipboard(const std::string& text)
{
	if (!OpenClipboard(nullptr))
		return false;
	EmptyClipboard();
	HGLOBAL mem = GlobalAlloc(GMEM_MOVEABLE, text.size() + 1);
	if (!mem)
	{
		CloseClipboard();
		return false;
	}
	char* dst = (char*)GlobalLock(mem);
	memcpy(dst, text.c_str(), text.size() + 1);
	GlobalUnlock(mem);
	SetClipboardData(CF_TEXT, mem);
	CloseClipboard();
	return true;
}

static std::string ReadClipboard()
{
	if (!OpenClipboard(nullptr))
		return {};
	HANDLE h = GetClipboardData(CF_TEXT);
	std::string out;
	if (h)
	{
		const char* p = (const char*)GlobalLock(h);
		if (p)
			out = p;
		GlobalUnlock(h);
	}
	CloseClipboard();
	return out;
}

static bool PickOpenPath(char* buf, DWORD bufSize)
{
	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = buf;
	ofn.nMaxFile = bufSize;
	ofn.lpstrFilter = "Config (*.dat;*.cfg)\0*.dat;*.cfg\0All\0*.*\0";
	ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST;
	return GetOpenFileNameA(&ofn) == TRUE;
}

static bool PickSavePath(char* buf, DWORD bufSize)
{
	OPENFILENAMEA ofn{};
	ofn.lStructSize = sizeof(ofn);
	ofn.lpstrFile = buf;
	ofn.nMaxFile = bufSize;
	ofn.lpstrFilter = "Config (*.dat)\0*.dat\0All\0*.*\0";
	ofn.lpstrDefExt = "dat";
	ofn.Flags = OFN_OVERWRITEPROMPT | OFN_PATHMUSTEXIST;
	return GetSaveFileNameA(&ofn) == TRUE;
}

// Reports the live pad state. A plain CLabel is fixed at construction, and the
// tab is only built once, so the text has to be refreshed at draw time.
class CControllerStatus : public CLabel
{
public:
	CControllerStatus() : CLabel(X("Status"), true, Color(148, 163, 184)) {}

	void Render() override
	{
		char buf[96] = {};
		if (!padEnabled)
		{
			std::snprintf(buf, sizeof(buf), X("Status:  off"));
			SetCustomColor(Color(148, 163, 184));
		}
		else if (Gamepad::Connected())
		{
			std::snprintf(buf, sizeof(buf), X("Status:  %s  •  slot %d"), Gamepad::DeviceName(), Gamepad::UserIndex() + 1);
			SetCustomColor(Color(74, 222, 128));
		}
		else
		{
			std::snprintf(buf, sizeof(buf), X("Status:  no controller detected"));
			SetCustomColor(Color(148, 163, 184));
		}

		SetName(buf);
		CLabel::Render();
	}
};

void Menu::InitMiscTab(CTab* tab)
{
	if (auto Box = Section(tab, X("General"), X("settings.general")))
	{
		Box->AddObject<CCheckbox>(X("Streamproof"), &streamproof);
		Box->AddObject<CKeybind>(X("Combat Mode Key"), &Menu::combatModeKeybind);
	}
	if (auto Box = Section(tab, X("Controller"), X("settings.controller")))
	{
		Box->AddObject<CControllerStatus>();
		Box->AddObject<CCheckbox>(X("Enable Controller"), &padEnabled,
			X("Reads XInput. Xbox pads work directly; DualSense needs Steam Input or DS4Windows."));
		Box->AddObject<CDropdown>(X("Button Layout"), &padLayout,
			std::vector<std::string>{ X("Auto"), X("Xbox"), X("PlayStation") },
			X("Labels shown on keybinds. Auto reads the pad's vendor ID, which reports Xbox through Steam Input."));
		Box->AddObject<CDropdown>(X("Open Menu With"), &padMenuCombo,
			std::vector<std::string>{ X("L3 + R3"), X("Back + Start"), X("Disabled") });
		Box->AddObject<CCheckbox>(X("Cursor Control"), &padCursorEnabled,
			X("Left stick moves the pointer, A/Cross clicks, B/Circle right-clicks, right stick scrolls."));
		Box->AddObject<CCheckbox>(X("Bumpers Switch Tabs"), &padTabCycle);
		Box->AddObject<CSliderFloat>(X("Cursor Speed"), &padCursorSpeed, 300.f, 3000.f, X("{:.0f}"));
		Box->AddObject<CSliderFloat>(X("Scroll Speed"), &padScrollSpeed, 2.f, 40.f, X("{:.0f}"));
		Box->AddObject<CSliderFloat>(X("Stick Deadzone"), &padDeadzone, 0.05f, 0.60f, X("{:.2f}"));
		Box->AddObject<CSliderFloat>(X("Trigger Threshold"), &padTriggerThreshold, 0.05f, 0.95f, X("{:.2f}"));
	}
	if (auto Box = Section(tab, X("Debug"), X("settings.debug")))
	{
		Box->AddObject<CCheckbox>(X("Debug Console"), &debugConsoleEnabled);
		Box->AddObject<CCheckbox>(X("Debug Overlay"), &debugOverlayEnabled);
		Box->AddObject<CCheckbox>(X("Benchmark"), &benchmarkEnabled);
		Box->AddObject<CCheckbox>(X("Auto-Save Logs"), &AutoSaveLogs);
		Box->AddObject<CCheckbox>(X("Log Errors Only"), &LogErrorsOnly);
		Box->AddObject<CCheckbox>(X("Log Transport Errors"), &LogTransportErrors);
#ifdef _DEBUG
		Box->AddObject<CDropdown>(X("Camera Projection Test"), &camProjectionTest,
			std::vector<std::string>{ X("Current"), X("POV"), X("Locked Read-Back") },
			X("Debug only. Tests which FOV source aligns ESP with the rendered image. Does not write game memory. Resets to Current each launch."));
#endif
	}
	if (auto Box = Section(tab, X("Camera"), X("settings.camera")))
	{
		Box->AddObject<CCheckbox>(X("FOV Changer"), &zoomChangerEnabled,
			X("Always-on FOV override in degrees. Values above your in-game FOV widen the view; below zooms in."));
		Box->AddObject<CSliderFloat>(X("FOV Value"), &zoomChangerFOV, 20.f, 130.f, X("{:.0f}"));
		Box->AddObject<CCheckbox>(X("Zoom"), &zoomChangerKeybindEnabled,
			X("Hold-to-zoom on the key below. Overrides FOV Changer while held."));
		Box->AddObject<CKeybind>(X("Zoom Key"), &Menu::zoomKeybind);
		Box->AddObject<CSliderFloat>(X("Zoom Level"), &zoomChangerMagnification, 1.f, 20.f, X("{:.1f}x"),
			X("True magnification against your unzoomed FOV, like a scope. 4x is roughly a marksman optic, 20x is extreme."));
	}
	if (auto Box = Section(tab, X("Flashlight"), X("settings.flashlight")))
	{
		auto flash = Box->AddObject<CCheckbox>(X("Flashlight Color"), &Cfg::Scene::flashlightColorChangerEnabled);
		flash->AddObject<CColorPicker>(X("Flashlight Color"), &Menu::colFlashlight, true);
	}
	if (auto Box = Section(tab, X("Theme"), X("settings.theme")))
	{
		Box->AddObject<CColorPicker>(X("Menu Color"), &Menu::colMenu, false);
		if (auto row = Box->AddObject<CGrid>(4, 8.0f))
		{
			row->AddObject<CButton>(X("Purple"), []() { Menu::colMenu = Color(168, 85, 247); });
			row->AddObject<CButton>(X("Blue"), []() { Menu::colMenu = Color(59, 130, 246); });
			row->AddObject<CButton>(X("Red"), []() { Menu::colMenu = Color(239, 68, 68); });
			row->AddObject<CButton>(X("Green"), []() { Menu::colMenu = Color(34, 197, 94); });
		}
	}
}

void Menu::InitConfigsTab(CTab* tab)
{
	if (auto Box = Section(tab, X("Current Config"), X("configs.current")))
	{
		Box->SetCollapsible(false);
		Box->AddObject<CTextInput>(X("Config Name"), &g_cfgSlotName, false, true,
			X("Letters, numbers, _ and - only. 'default' is your main config."));
		Box->AddObject<CLabel>(X("Saved under LocalAppData\\Microsoft\\CLR"), true, Style::dimmedText);
	}

	if (auto Box = Section(tab, X("Actions"), X("configs.actions")))
	{
		Box->SetCollapsible(false);
		if (auto grid = Box->AddObject<CGrid>(2, 8.0f))
		{
			grid->AddObject<CConfirmationButton>(X("Save Configuration"),
				[]() {
					Menu::SyncToSettings();
					cfg_invalidate_path_cache();
					cfg_save();
				},
				[]() {},
				X("Overwrite the current config slot?"));
			grid->AddObject<CConfirmationButton>(X("Load Configuration"),
				[]() {
					cfg_invalidate_path_cache();
					cfg_load();
					Menu::SyncFromSettings(true);
				},
				[]() {},
				X("Load this slot and replace current settings?"));
			grid->AddObject<CConfirmationButton>(X("Delete Configuration"),
				[]() {
					cfg_invalidate_path_cache();
					std::error_code ec;
					std::filesystem::remove(cfg_get_file(), ec);
					g_cfgStatus = ec ? "Delete failed" : "Config deleted";
				},
				[]() {},
				X("Permanently delete this config file?"));
			grid->AddObject<CButton>(X("Open Config Folder"), []() {
				ensure_cfg_dir();
				ShellExecuteA(nullptr, "open", cfg_get_dir().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
			});
		}
	}

	if (auto Box = Section(tab, X("Import / Export"), X("configs.share")))
	{
		Box->AddObject<CButton>(X("Import from file"), []() {
			char path[MAX_PATH]{};
			if (!PickOpenPath(path, MAX_PATH))
				return;
			std::ifstream in(path, std::ios::binary);
			if (!in)
			{
				g_cfgStatus = "Import failed";
				return;
			}
			std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			ensure_cfg_dir();
			std::ofstream out(cfg_get_file(), std::ios::binary | std::ios::trunc);
			if (!out)
			{
				g_cfgStatus = "Import failed (write)";
				return;
			}
			out.write(data.data(), (std::streamsize)data.size());
			out.close();
			cfg_invalidate_path_cache();
			cfg_load();
			Menu::SyncFromSettings(true);
			g_cfgStatus = "Imported config";
		});
		Box->AddObject<CButton>(X("Export to file"), []() {
			Menu::SyncToSettings();
			cfg_save();
			char path[MAX_PATH] = "syndikate-arc.dat";
			if (!PickSavePath(path, MAX_PATH))
				return;
			std::error_code ec;
			std::filesystem::copy_file(cfg_get_file(), path, std::filesystem::copy_options::overwrite_existing, ec);
			g_cfgStatus = ec ? "Export failed" : "Exported config";
		});
		Box->AddObject<CButton>(X("Share Config"), []() {
			Menu::SyncToSettings();
			cfg_save();
			std::ifstream in(cfg_get_file(), std::ios::binary);
			if (!in)
			{
				g_cfgStatus = "Share failed";
				return;
			}
			std::string data((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
			uiShareCode = std::string("SYNARC1.") + B64Encode(data);
			CopyToClipboard(uiShareCode);
			g_cfgStatus = "Share code copied";
		});
		Box->AddObject<CTextInput>(X("Share code"), &uiShareCode, false, true);
		Box->AddObject<CButton>(X("Import share code"), []() {
			std::string code = uiShareCode.empty() ? ReadClipboard() : uiShareCode;
			const std::string prefix = "SYNARC1.";
			if (code.rfind(prefix, 0) == 0)
				code = code.substr(prefix.size());
			std::string data = B64Decode(code);
			if (data.empty())
			{
				g_cfgStatus = "Invalid share code";
				return;
			}
			ensure_cfg_dir();
			std::ofstream out(cfg_get_file(), std::ios::binary | std::ios::trunc);
			if (!out)
			{
				g_cfgStatus = "Import failed (write)";
				return;
			}
			out.write(data.data(), (std::streamsize)data.size());
			out.close();
			cfg_invalidate_path_cache();
			cfg_load();
			Menu::SyncFromSettings(true);
			g_cfgStatus = "Imported share code";
		});
	}
}
