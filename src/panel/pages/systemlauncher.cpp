#include "systemlauncher.h"

namespace riley::panel::pages {

namespace {

// The system's own settings (Riley's app.py). The M3 gate's planted fault: another program.
QString program()
{
#ifdef RILEY_PLANT_FAULT_PAGES
    return QStringLiteral("gnome-control-center");
#else
    return QStringLiteral("hypr-settings");
#endif
}

}  // namespace

bool SystemLauncher::launch()
{
    if (!panel())
        return false;
    panel()->launch(program());
    return true;
}

}  // namespace riley::panel::pages
