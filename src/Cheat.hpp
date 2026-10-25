#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

#include <windows.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>
#include <format>
#include <mutex>
#include <unordered_map>
#include <map>
#include <span>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <thread>
#include <chrono>
#include <cstring>
#include <cctype>

#include <d3d9.h>

#ifndef LOG
#define LOG(...) ((void)0)
#endif

#include "sdk/Math.hpp"

struct Rect
{
	Vector2 position = Vector2(), size = Vector2();
	Vector2 Min() const { return position; }
	Vector2 Max() const { return position + size; }
	bool IsValid() const
	{
		return position.x > 0.0f && position.y > 0.0f && size.x > 0.0f && size.y > 0.0f;
	}
	bool Intersects(const Vector2& p, const Vector2& s) const
	{
		Vector2 _min = p;
		Vector2 _max = p + s;
		return _min.y < Max().y && _max.y > Min().y && _min.x < Max().x && _max.x > Min().x;
	}
};

#include "menu/string_encryption.hpp"

#ifndef IM_VEC2_CLASS_EXTRA
#define IM_VEC2_CLASS_EXTRA \
	ImVec2(const Vector2& v) : x((float)v.x), y((float)v.y) {} \
	operator Vector2() const { return Vector2(x, y); }
#endif

#include "imgui.h"
#include "imgui_internal.h"
#include "imgui_impl_dx9.h"
#include "imgui_impl_win32.h"

#include "utils/color.hpp"

#include "menu/framework/color.hpp"
#include "menu/framework/input/input.hpp"
#include "menu/framework/render/render.hpp"
#include "menu/framework/render/assets/font_awesome.hpp"
#include "menu/framework/style/style.hpp"
#include "menu/framework/object/object.hpp"
#include "menu/framework/window/window.hpp"
#include "menu/framework/container/container.hpp"
#include "menu/framework/tab/tab.hpp"
#include "menu/framework/tab/subtab.hpp"

#include "menu/framework/widgets/popup/popup.hpp"
#include "menu/framework/widgets/label/label.hpp"
#include "menu/framework/widgets/button/button.hpp"
#include "menu/framework/widgets/checkbox/checkbox.hpp"
#include "menu/framework/widgets/slider/slider.hpp"
#include "menu/framework/widgets/dropdown/dropdown.hpp"
#include "menu/framework/widgets/multi_dropdown/multi_dropdown.hpp"
#include "menu/framework/widgets/radio/radio.hpp"
#include "menu/framework/widgets/grid/grid.hpp"
#include "menu/framework/widgets/color_picker/color_picker.hpp"
#include "menu/framework/widgets/listbox/listbox.hpp"
#include "menu/framework/widgets/text_input/text_input.hpp"
#include "menu/framework/widgets/confirmation_button/confirmation_button.hpp"
#include "menu/framework/widgets/seperator/seperator.hpp"
#define PAWJOB_MENU_KEYBIND_WIDGET_ENABLED
#include "menu/framework/widgets/keybind/keybind.hpp"
#include "menu/framework/ui/ui.hpp"
