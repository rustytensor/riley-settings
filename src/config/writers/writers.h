// The factories of the writers, one per target; each lives in its own file next to this one.
#pragma once

#include "../writer.h"

#include <memory>

namespace riley::config::writers {

std::unique_ptr<Writer> makeHyprlandLua();   // hyprland-lua.cpp: ~/.config/hypr/settings.lua
std::unique_ptr<Writer> makeHypridle();      // hypridle.cpp: ~/.config/hypr/idle.conf
std::unique_ptr<Writer> makeHyprpaper();     // hyprpaper.cpp: ~/.config/hypr/hyprpaper.conf
std::unique_ptr<Writer> makeGreeterSway();   // greeter-sway.cpp: the login screen's sway file
std::unique_ptr<Writer> makeRileyDock();     // riley-dock.cpp: the dock's pins file and riley.conf (own files)
std::unique_ptr<Writer> makeRileyDesktop();  // riley-desktop.cpp: ~/.config/riley/desktop.json (own file)

}  // namespace riley::config::writers
