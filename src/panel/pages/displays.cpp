#include "displays.h"

namespace riley::panel::pages {

bool DisplaysBackend::load()
{
    return notReady();
}

bool DisplaysBackend::dragMove(const QString&, double, double)
{
    return notReady();
}

bool DisplaysBackend::dragEnd(const QString&)
{
    return notReady();
}

bool DisplaysBackend::setScreenField(const QString&, const QString&, const QString&)
{
    return notReady();
}

bool DisplaysBackend::reset()
{
    return notReady();
}

bool DisplaysBackend::apply()
{
    return notReady();
}

bool DisplaysBackend::keep()
{
    return notReady();
}

bool DisplaysBackend::revert()
{
    return notReady();
}

}  // namespace riley::panel::pages
