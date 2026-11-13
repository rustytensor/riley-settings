#include "desktop.h"

namespace riley::panel::pages {

bool DesktopBackend::reload()
{
    return notReady();
}

bool DesktopBackend::addFolder(const QString&)
{
    return notReady();
}

bool DesktopBackend::setHidden(const QString&, bool)
{
    return notReady();
}

bool DesktopBackend::removeBox(const QString&)
{
    return notReady();
}

}  // namespace riley::panel::pages
