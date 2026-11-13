// The factories of the writers, one per target; each lives in its own file next to this one.
#pragma once

#include "../writer.h"

#include <memory>

namespace riley::config::writers {

std::unique_ptr<Writer> makeHyprlandLua();   // hyprland-lua.cpp: ~/.config/hypr/settings.lua
std::unique_ptr<Writer> makeHypridle();      // hypridle.cpp: ~/.config/hypr/idle.conf
std::unique_ptr<Writer> makeHyprpaper();     // hyprpaper.cpp: ~/.config/hypr/hyprpaper.conf
std::unique_ptr<Writer> makeGreeterSway();   // greeter-sway.cpp: the login screen's sway file

}  // namespace riley::config::writers
