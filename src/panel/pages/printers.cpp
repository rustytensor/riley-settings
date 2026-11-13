#include "printers.h"

namespace riley::panel::pages {

bool PrintersBackend::refresh()
{
    return notReady();
}

bool PrintersBackend::printTestPage(const QString&)
{
    return notReady();
}

bool PrintersBackend::cancelJob(const QString&)
{
    return notReady();
}

bool PrintersBackend::setOption(const QString&, const QString&, const QString&)
{
    return notReady();
}

QString PrintersBackend::userOption(const QString&, const QString&)
{
    notReady();
    return {};
}

bool PrintersBackend::setDefault(const QString&)
{
    return notReady();
}

bool PrintersBackend::openSetupTool()
{
    return notReady();
}

}  // namespace riley::panel::pages
