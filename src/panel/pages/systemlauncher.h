// The system launcher: the menu entry of the system-launcher page launches hypr-settings and leaves the page shown as it was (docs/pages-contract.md, "The system launcher").
// M3 scaffolding: a stub. Every function refuses with "<page title>: not ready".
#pragma once

#include "common.h"

#include <QString>
#include <QStringList>
#include <QVariantList>

namespace riley::panel::pages {

class SystemLauncher : public PageBackend {
    Q_OBJECT
    QML_ELEMENT

public:
    explicit SystemLauncher(QObject* parent = nullptr) : PageBackend(parent) {}

    Q_INVOKABLE bool launch();

Q_SIGNALS:
    void changed();

};

}  // namespace riley::panel::pages
