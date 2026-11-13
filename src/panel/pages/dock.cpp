#include "dock.h"

namespace riley::panel::pages {

bool DockBackend::refresh()
{
    return notReady();
}

bool DockBackend::dropOnDock(const QString&, double)
{
    return notReady();
}

bool DockBackend::dropOnRemove(const QString&)
{
    return notReady();
}

bool DockBackend::setScreens(const QStringList&)
{
    return notReady();
}

}  // namespace riley::panel::pages
