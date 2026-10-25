#include "gui/gui.h"
#include "dgb/Spoof.h"
#include "menu/menu.hpp"
#include "menu/framework/input/input.hpp"

void ui::Render()
{
	SPOOF_FUNC;
	Input::Poll();
	Menu::Tick();
}
