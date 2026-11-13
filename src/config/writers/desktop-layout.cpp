#include "desktop-layout.h"

namespace riley::config::desktop {

bool resolve(const QJsonObject&, const QString&, QJsonObject*, QString* error)
{
    *error = QStringLiteral("desktop layout: not ready");
    return false;
}

}  // namespace riley::config::desktop
