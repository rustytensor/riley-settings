#include "systemlauncher.h"

namespace riley::panel::pages {

bool SystemLauncher::launch()
{
    if (!panel())
        return false;
    // The M3 gate's planted fault: the entry launches another program.
    panel()->launch(Controller::plantFaultPages() ? QStringLiteral("gnome-control-center")
                                                  : QStringLiteral("hypr-settings"));
    return true;
}

}  // namespace riley::panel::pages
